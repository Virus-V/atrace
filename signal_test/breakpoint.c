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
static struct list_head slot_sets;

void 
slot_set_init(void)
{
  INIT_LIST_HEAD(&slot_sets);
  slot_set_add();
}

void 
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

int
slot_alloc(slot_set_t **slot, unsigned int *index)
{
  slot_set_t *curr;
  int idx = 0, pos;
  // 遍历slot_sets链表，找到第一个非空的slot_set
  pthread_rwlock_rdlock(&slot_sets_lock);
  list_for_each_entry(curr, &slot_sets, slot_set_list) {
    if (curr->used_slot_count == SLOT_NUM) {
      continue;
    }

    for (; idx<SLOT_MAP_LEN; idx++) {
      if (curr->free_slot[idx] == 0) {
        continue;
      }
      // TODO
      pos = __builtin_ffs(curr->free_slot[idx]) + idx * UINT_BIT_NUM;
    }

    goto out;
  }
out:
  pthread_rwlock_unlock(&slot_sets_lock);
  // 从free_slot bitmap中找到第一个为1的位，然后返回索引
  
  // 判断索引是否越界
  // 将位清零，然后返回相应的位
  
}

breakpoint_t *
breakpoint_new(void)
{
    breakpoint_t *bkpt = calloc(1, sizeof(breakpoint_t));
    if(!bkpt){
      perror("calloc");
      abort();
    }

    // 初始化节点
    INIT_LIST_HEAD(&bkpt->breakpoint_chain);
    rb_init_node(&bkpt->breakpoint_tree);

    return bkpt;
}

// void
// breakpoint_del(breakpoint_t *bkpt)
// {
//     assert(bkpt != NULL);
//     assert(list_empty(&bkpt->breakpoint_chain_));
//     assert(RB_EMPTY_NODE(&bkpt->breakpoint_tree_));

//     free(bkpt);
// }

// 通过address查找断点
static breakpoint_t *
breakpoint_rb_find_internal(addr_t addr)
{
  struct rb_node *rbnode;
  breakpoint_t *bp;

  rbnode = breakpoints.rb_node;
  while (rbnode != NULL) {
    // 获得断点对象
    bp = container_of(rbnode, breakpoint_t, breakpoint_tree);

    if (addr < bp->address)
      rbnode = rbnode->rb_left;
    else if (addr > bp->address)
      rbnode = rbnode->rb_right;
    else
      return bp;
  }

  return NULL;
}

static breakpoint_t *
breakpoint_rb_find(addr_t addr)
{
  breakpoint_t *bp;

  pthread_mutex_lock(&breakpoint_lock);
  bp = breakpoint_rb_find_internal(addr);
  pthread_mutex_unlock(&breakpoint_lock);
  return bp;
}

// 将断点增加到红黑树
// 成功 0 节点已存在 -1
static int
breakpoint_rb_insert_internal(breakpoint_t *bkpt)
{
  struct rb_node **tmp, *parent = NULL;
  breakpoint_t *bp;

  tmp = &breakpoints.rb_node;
  while (*tmp) {
    bp = container_of(*tmp, breakpoint_t, breakpoint_tree);

    parent = *tmp;
    if (bkpt->address < bp->address)
      tmp = &((*tmp)->rb_left);
    else if (bkpt->address > bp->address)
      tmp = &((*tmp)->rb_right);
    else
      return -1;
  }

  /* Add new node and rebalance tree. */
  rb_link_node(&bkpt->breakpoint_tree, parent, tmp);
  rb_insert_color(&bkpt->breakpoint_tree, &breakpoints);

  return 0;
}

static int
breakpoint_rb_insert(breakpoint_t *bkpt)
{
  int ret;

  pthread_mutex_lock(&breakpoint_lock);
  ret = breakpoint_rb_insert_internal(bkpt);
  pthread_mutex_unlock(&breakpoint_lock);

  return ret;
}

// 将addr断点从红黑树中删除，并返回原对象
// 如果不存在，则返回NULL
static breakpoint_t *
breakpoint_rb_delete_internal(addr_t addr)
{
  breakpoint_t *bp;

  bp = breakpoint_rb_find_internal(addr);
  if (!bp) {
    return NULL;
  }

  rb_erase(&bp->breakpoint_tree, &breakpoints);
  RB_CLEAR_NODE(&bp->breakpoint_tree);

  return bp;
}

static breakpoint_t *
breakpoint_rb_delete(addr_t addr)
{
  breakpoint_t *bp;

  pthread_mutex_lock(&breakpoint_lock);
  bp = breakpoint_rb_delete_internal(addr);
  pthread_mutex_unlock(&breakpoint_lock);

  return bp;
}

// 查找给定地址上面的断点
breakpoint_t *
breakpoint_find(addr_t addr)
{
  // 查找红黑树中是否存在它
  return breakpoint_rb_find(addr);
}

// 增加一个断点
int
breakpoint_add(breakpoint_t *bkpt)
{
  breakpoint_t *bp;
  object_t *obj;

  assert(bkpt != NULL);
  assert(list_empty(&bkpt->breakpoint_chain));
  assert(RB_EMPTY_NODE(&bkpt->breakpoint_tree));

  obj = object_get_by_address(bkpt->address);
  if (!obj) {
    return -1;
  }

  // object不支持打断点
  if (object_attr_flag_NO_BKPT(obj)) {
    return -1;
  }

  // 插入到红黑树中
  pthread_mutex_lock(&breakpoint_lock);
  bp = breakpoint_rb_find_internal(bkpt->address);
  if (bp) {
    pthread_mutex_unlock(&breakpoint_lock);
    return -1;
  }

  breakpoint_rb_insert_internal(bkpt);
  pthread_mutex_unlock(&breakpoint_lock);

  // 加入到object中
  pthread_mutex_lock(&obj->bpc_lock);
  list_add(&bkpt->breakpoint_chain, &obj->breakpoint_chain);
  bkpt->obj = obj;
  pthread_mutex_unlock(&obj->bpc_lock);

  return 0;
}

// 移除一个断点
int
breakpoint_remove(breakpoint_t *bkpt)
{
  object_t *obj;
  breakpoint_t *bp;

  assert(bkpt != NULL && bkpt->obj != NULL);
  assert(!list_empty(&bkpt->breakpoint_chain));
  assert(!RB_EMPTY_NODE(&bkpt->breakpoint_tree));

  // 如果breakpoint处于启用状态, 则返回失败
  if (breakpoint_attr_flag_ENABLE(bkpt)) {
    return -1;
  }

  obj = bkpt->obj;

  pthread_mutex_lock(&obj->bpc_lock);
  list_del_init(&bkpt->breakpoint_chain);
  bkpt->obj = NULL;
  pthread_mutex_unlock(&obj->bpc_lock);

  // 从红黑树中删除
  bp = breakpoint_rb_delete(bkpt->address);
  assert(bp == bkpt);

  return 0;
}

// 启用一个断点对象
// 不可对一个断点并行调用
int
breakpoint_enable(breakpoint_t *bkpt)
{
  breakpoint_t *bp;

  assert(bkpt != NULL && bkpt->obj != NULL);

  // 如果断点不存在，则失败
  bp = breakpoint_find(bkpt->address);
  if (!bp || bp != bkpt) {
    return -1;
  }

  // XXX 此处是否要加锁保护
  if (breakpoint_attr_flag_ENABLE(bkpt)) {
    return 0;
  }
  // 设置使能标志位
  breakpoint_attr_set_ENABLE(bkpt);

  // NOTIC 实际修改指令之前，要保证前面工作已经就位
  // 去除text地址写保护
  object_memory_unlock(bkpt->obj);
  // 替换断点指令
  // 读取原有指令, 记录到breakpoint对象, 写入断点指令到位置
  bkpt->instruction = *((volatile instr_t *)bkpt->address);
  // 此处写入要是原子的, 也就是要发出一次总线请求
  *((volatile instr_t *)bkpt->address) = (instr_t)0xD4200060;  // brk #0x03
  // 增加写保护, 并flush icache
  object_memory_lock(bkpt->obj);

  return 0;
}

// 停用一个断点对象
int
breakpoint_disable(breakpoint_t *bkpt)
{
  breakpoint_t *bp;

  assert(bkpt != NULL && bkpt->obj != NULL);

  // 如果断点不存在，则失败
  bp = breakpoint_find(bkpt->address);
  if (!bp || bp != bkpt) {
    return -1;
  }

  // 恢复原有的指令
  // 去除text地址写保护
  object_memory_unlock(bkpt->obj);
  // 此处写入要是原子的, 也就是要发出一次总线请求
  *((volatile instr_t *)bkpt->address) = bkpt->instruction;
  // 增加写保护, 并flush icache
  object_memory_lock(bkpt->obj);

  // XXX 此处是否要加锁保护
  if (!breakpoint_attr_flag_ENABLE(bkpt)) {
    return 0;
  }

  // 清除使能标志位
  breakpoint_attr_clr_ENABLE(bkpt);

  return 0;
}

