#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <sys/mman.h>

#include "common.h"
#include "breakpoint.h"
#include "object.h"

// 所有断点在红黑树中
// 使用基数树也可以,但是断点场景是查找次数远大于插入和删除次数
// 所以在此场景红黑树和基数树没有太大区别
static RB_ROOT(breakpoints);
static pthread_mutex_t breakpoint_lock = PTHREAD_MUTEX_INITIALIZER;

// 指令槽集合
pthread_rwlock_t slot_sets_lock = PTHREAD_RWLOCK_INITIALIZER;
static LIST_HEAD(slot_sets);

void
slot_set_deinit(void)
{
  // TODO 释放所有
}


static void
slot_set_add(void)
{
  slot_set_t *slot;
  void *map_ptr = MAP_FAILED;

  slot = calloc(1, sizeof(slot_set_t));
  if (!slot) {
    perror("calloc");
    abort();
  }

  memset(slot->free_slot, -1, sizeof(slot->free_slot));
  pthread_mutex_init(&slot->lock, NULL);
  INIT_LIST_HEAD(&slot->slot_set_list);

  map_ptr = mmap(
    NULL, SLOT_PAGE_SIZE,
    PROT_READ | PROT_WRITE | PROT_EXEC,
    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0
  );

  if (map_ptr == MAP_FAILED) {
    perror("mmap");
    abort();
  }

  slot->slots = (uintptr_t)map_ptr;

  // 插入到链表头
  pthread_rwlock_wrlock(&slot_sets_lock);
  list_add(&slot->slot_set_list, &slot_sets);
  pthread_rwlock_unlock(&slot_sets_lock);
}

// 分配一个slot，如果空间不够则创建
static void
slot_alloc(slot_set_t **slot, unsigned int *index)
{
  slot_set_t *curr;
  int idx, pos;

retry:
  idx = 0;
  // 遍历slot_sets链表，找到第一个非空的slot_set
  pthread_rwlock_rdlock(&slot_sets_lock);
  list_for_each_entry(curr, &slot_sets, slot_set_list) {
    pthread_mutex_lock(&curr->lock);
    if (curr->used_slot_count == SLOT_NUM) {
      pthread_mutex_unlock(&curr->lock);
      continue;
    }

    for (; idx<SLOT_MAP_LEN; idx++) {
      if (curr->free_slot[idx] == 0) {
        continue;
      }
      // 获得当前最低位的1, __builtin_ffs(0x1) => 1
      pos = __builtin_ffs(curr->free_slot[idx]);
      if ((pos + idx * UINT_BIT_NUM) > SLOT_NUM) {
        // 当前pos大于slot_num,表示到达了末尾
        break;
      }
      pos--;  // 修正为偏移
      goto found;
    }

    pthread_mutex_unlock(&curr->lock);
  }
  // 找不到空闲的slot, 创建slot_set
  pthread_rwlock_unlock(&slot_sets_lock);
  // FIXME 可能会增加两个slot_set
  slot_set_add();
  goto retry;

found:
  pthread_rwlock_unlock(&slot_sets_lock);
  // 将pos对应的二进制位清零
  curr->free_slot[idx] &= ~(0x1 << pos);
  curr->used_slot_count++;
  pthread_mutex_unlock(&curr->lock);

  *slot = curr;
  *index = idx * UINT_BIT_NUM + pos;
}

// 释放一个slot
static void
slot_free(slot_set_t *slot, unsigned int index)
{
  assert(slot != NULL);
  assert(index < UINT_BIT_NUM);
  int idx, pos;

  idx = index / UINT_BIT_NUM;
  pos = index % UINT_BIT_NUM;

  pthread_mutex_lock(&slot->lock);
  if (slot->free_slot[idx] & (0x1 << pos)) {
    pthread_mutex_unlock(&slot->lock);
    return;
  }
  // 将相应的位置1
  slot->free_slot[idx] |= 0x1 << pos;
  slot->used_slot_count--;
  assert(slot->used_slot_count >= 0);
  pthread_mutex_unlock(&slot->lock);
}

// 刷新对应slot的缓存
static void 
slot_invalid_cache(slot_set_t *slot, unsigned int index)
{
  uintptr_t start, end;
  assert(slot != NULL);
  
  start = slot->slots + index * SLOT_SIZE;
  end = start + SLOT_SIZE;

  __builtin___clear_cache((void *)start, (void *)end);
}

// 创建普通断点，普通断点是在代码中的
breakpoint_t *
breakpoint_new(void)
{
  breakpoint_t *bkpt = calloc(1, sizeof(breakpoint_t));
  if(!bkpt){
    perror("calloc");
    abort();
  }

  // 初始化
  INIT_LIST_HEAD(&bkpt->breakpoint_chain);
  rb_init_node(&bkpt->bp_enter.rb_node);
  rb_init_node(&bkpt->bp_return.rb_node);

  pthread_mutex_init(&bkpt->lock, NULL);

  // 记录当前断点对象
  bkpt->bp_enter.bkpt = bkpt;
  bkpt->bp_return.bkpt = bkpt;

  return bkpt;
}

// 通过address查找断点
static struct bp_entry *
breakpoint_rb_find_internal(addr_t addr)
{
  struct rb_node *rbnode;
  struct bp_entry *bp;

  rbnode = breakpoints.rb_node;
  while (rbnode != NULL) {
    // 获得断点对象
    bp = container_of(rbnode, struct bp_entry, rb_node);

    if (addr < bp->address)
      rbnode = rbnode->rb_left;
    else if (addr > bp->address)
      rbnode = rbnode->rb_right;
    else
      return bp;
  }

  return NULL;
}

static struct bp_entry *
breakpoint_rb_find(addr_t addr)
{
  struct bp_entry *bp;

  pthread_mutex_lock(&breakpoint_lock);
  bp = breakpoint_rb_find_internal(addr);
  pthread_mutex_unlock(&breakpoint_lock);
  return bp;
}

// 将断点增加到红黑树
// 成功 0 节点已存在 -1
static int
breakpoint_rb_insert_internal(struct bp_entry *entry)
{
  struct rb_node **tmp, *parent = NULL;
  struct bp_entry *bp;

  assert(RB_EMPTY_NODE(&entry->rb_node));

  tmp = &breakpoints.rb_node;
  while (*tmp) {
    bp = container_of(*tmp, struct bp_entry, rb_node);

    parent = *tmp;
    if (entry->address < bp->address)
      tmp = &((*tmp)->rb_left);
    else if (entry->address > bp->address)
      tmp = &((*tmp)->rb_right);
    else
      return -1;
  }

  /* Add new node and rebalance tree. */
  rb_link_node(&entry->rb_node, parent, tmp);
  rb_insert_color(&entry->rb_node, &breakpoints);

  return 0;
}

static int
breakpoint_rb_insert(struct bp_entry *entry)
{
  int ret;

  pthread_mutex_lock(&breakpoint_lock);
  ret = breakpoint_rb_insert_internal(entry);
  pthread_mutex_unlock(&breakpoint_lock);

  return ret;
}

// 将断点插入到红黑树，如果存在则返回-1
static int
breakpoint_rb_search_insert(struct bp_entry *entry)
{
  int ret;
  struct bp_entry *exist;

  pthread_mutex_lock(&breakpoint_lock);
  exist = breakpoint_rb_find_internal(entry->address);
  if (exist) {
    pthread_mutex_unlock(&breakpoint_lock);
    return -1;
  }

  breakpoint_rb_insert_internal(entry);
  pthread_mutex_unlock(&breakpoint_lock);

  return ret;
}

// 将addr断点从红黑树中删除，并返回原对象
// 如果不存在，则返回NULL
static struct bp_entry *
breakpoint_rb_delete_internal(addr_t addr)
{
  struct bp_entry *bp;

  bp = breakpoint_rb_find_internal(addr);
  if (!bp) {
    return NULL;
  }

  rb_erase(&bp->rb_node, &breakpoints);
  RB_CLEAR_NODE(&bp->rb_node);

  return bp;
}

static struct bp_entry *
breakpoint_rb_delete(addr_t addr)
{
  struct bp_entry *bp;

  pthread_mutex_lock(&breakpoint_lock);
  bp = breakpoint_rb_delete_internal(addr);
  pthread_mutex_unlock(&breakpoint_lock);

  return bp;
}

// 查找给定地址上面的断点
breakpoint_t *
breakpoint_find(addr_t addr)
{
  struct bp_entry *entry;
  
  entry = breakpoint_rb_find(addr);
  if (!entry) {
    return NULL;
  }

  return entry->bkpt;
}

// 增加一个断点
// 加入到红黑树
int
breakpoint_add(breakpoint_t *bkpt)
{
  object_t *obj;

  assert(bkpt != NULL);
  assert(list_empty(&bkpt->breakpoint_chain));

  pthread_mutex_lock(&bkpt->lock);

  obj = object_get_by_address(bkpt->bp_enter.address);
  if (!obj) {
    pthread_mutex_unlock(&bkpt->lock);
    return -1;
  }

  // object不支持打断点
  if (object_attr_flag_NO_BKPT(obj)) {
    pthread_mutex_unlock(&bkpt->lock);
    return -2;
  }

  // 插入到红黑树中
  if (breakpoint_rb_search_insert(&bkpt->bp_enter) ) {
    pthread_mutex_unlock(&bkpt->lock);
    return -3;
  }
  
  // 有slot，指令需要单步
  if (!breakpoint_attr_flag_NO_SLOT(bkpt)) {
    volatile unsigned int *instr_ptr;
    // 分配slot
    slot_alloc(&bkpt->slot_set, &bkpt->slot_index);
    assert(bkpt->slot_set != NULL);

    // 初始化指令slot
    instr_ptr = (volatile unsigned int *)BP_GET_SLOT_OFFSET(bkpt->slot_set, bkpt->slot_index);
    // 拷贝指令数据到slot
    *instr_ptr = *((volatile unsigned int *)bkpt->bp_enter.address);
    *(instr_ptr + 1) = 0xD4200060u; // brk #0x03
    bkpt->bp_return.address = (addr_t)(instr_ptr + 1);

    // 刷新icache
    slot_invalid_cache(bkpt->slot_set, bkpt->slot_index);

    // 将bp_return插入到红黑树中
    if (breakpoint_rb_search_insert(&bkpt->bp_return)) {
      pthread_mutex_unlock(&bkpt->lock);
      slot_free(bkpt->slot_set, bkpt->slot_index);
      return -4;
    }
  }

  // 加入到object中
  pthread_mutex_lock(&obj->bpc_lock);
  list_add(&bkpt->breakpoint_chain, &obj->breakpoint_chain);
  bkpt->obj = obj;
  pthread_mutex_unlock(&obj->bpc_lock);
  
  pthread_mutex_unlock(&bkpt->lock);

  return 0;
}

// // 移除一个断点
// int
// breakpoint_remove(addr_t address)
// {
//   breakpoint_t *exist;

//   assert(bkpt != NULL);
//   assert(!RB_EMPTY_NODE(&bkpt->breakpoint_tree));

//   // 如果breakpoint处于启用状态, 则返回失败
//   if (breakpoint_attr_flag_ENABLE(bkpt)) {
//     return -1;
//   }

//   if (!breakpoint_attr_flag_RETURN(bkpt)) {
//     object_t *obj;
//     breakpoint_normal_t *bp = (breakpoint_normal_t *)bkpt;
//     assert(bp->obj != NULL);
//     assert(!list_empty(&bp->breakpoint_chain));

//     obj = bp->obj;

//     pthread_mutex_lock(&obj->bpc_lock);
//     list_del_init(&bp->breakpoint_chain);
//     bp->obj = NULL;
//     pthread_mutex_unlock(&obj->bpc_lock);
//   }
//   // 从红黑树中删除
//   exist = breakpoint_rb_delete(bkpt->address);
//   assert(exist == bkpt);

//   return 0;
// }

// 启用一个断点对象
int
breakpoint_enable(breakpoint_t *bkpt)
{ 
  assert(bkpt != NULL);
  assert(!RB_EMPTY_NODE(&bkpt->bp_enter.rb_node));

  pthread_mutex_lock(&bkpt->lock);
  if (breakpoint_attr_flag_ENABLE(bkpt)) {
    pthread_mutex_unlock(&bkpt->lock);
    return 0;
  }
  // 设置使能标志位
  breakpoint_attr_set_ENABLE(bkpt);

  // 去除text地址写保护
  object_memory_unlock(bkpt->obj);
  // 替换断点指令
  *((volatile unsigned int *)bkpt->bp_enter.address) = 0xD4200060;  // brk #0x03
  // 增加写保护, 并flush icache
  object_memory_lock(bkpt->obj);

  pthread_mutex_unlock(&bkpt->lock);

  return 0;
}

// 停用一个断点对象
int
breakpoint_disable(breakpoint_t *bkpt)
{
  volatile unsigned int *instr_ptr;

  assert(bkpt != NULL);
  assert(!RB_EMPTY_NODE(&bkpt->bp_enter.rb_node));

  pthread_mutex_lock(&bkpt->lock);

  // 如果当前已经是disable状态了
  if (!breakpoint_attr_flag_ENABLE(bkpt)) {
    pthread_mutex_unlock(&bkpt->lock);
    return 0;
  }

  breakpoint_attr_clr_ENABLE(bkpt);

  instr_ptr = (volatile unsigned int *)BP_GET_SLOT_OFFSET(bkpt->slot_set, bkpt->slot_index);

  // 去除text地址写保护
  object_memory_unlock(bkpt->obj);
  // 恢复原有指令 此处写入要是原子的, 也就是要发出一次总线请求
  *((volatile unsigned int *)bkpt->bp_enter.address) = *instr_ptr;
  // 增加写保护, 并flush icache
  object_memory_lock(bkpt->obj);

  pthread_mutex_unlock(&bkpt->lock);

  return 0;
}

