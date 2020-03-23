#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include <sys/mman.h>
#include <unistd.h>

#include "thread.h"

pthread_rwlock_t thread_map_lock = PTHREAD_RWLOCK_INITIALIZER;
static struct list_head thread_map[THREAD_MAP_SIZE];

// 初始化线程map
void
thread_map_init(void)
{
    int i;
    for(i=0; i<THREAD_MAP_SIZE; i++) {
        INIT_LIST_HEAD(thread_map + i);
    }
}

// 初始化线程结构体
thread_t *
thread_new(void)
{
    thread_t *thread = calloc(1, sizeof(thread_t));
    if(!thread){
      perror("calloc");
      abort();
    }

    thread->pg_size_ = sysconf(_SC_PAGESIZE);
    assert(thread->pg_size_ > 0);
    // 分配内存区域
    thread->code_cache_ = mmap(
        NULL, thread->pg_size_,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0
    );

    if (!thread->code_cache_) {
        perror("mmap");
        abort();
    }

    INIT_LIST_HEAD(&thread->thread_list_entry_);

    return thread;
}

// 释放线程对象
void
thread_del(thread_t *thread)
{
    assert(thread != NULL);
    assert(list_empty(&thread->thread_list_entry_));
    assert(thread->code_cache_ != NULL);

    // 取消映射内存区域
    munmap(thread->code_cache_, thread->pg_size_);

    free(thread);
}

// 不加锁的查找
static thread_t *
thread_map_find_internal(pid_t thread_id)
{
    struct list_head *head;
    thread_t *pos;

    head = thread_map + (thread_id % THREAD_MAP_SIZE);

    list_for_each_entry(pos, head, thread_list_entry_) {
        if(pos->tid == thread_id){
            return pos;
        }
    }

    return NULL;
}

// 增加线程map对象
int
thread_map_add(thread_t *thread)
{
    struct list_head *head;
    thread_t *exist;

    assert(thread != NULL);
    assert(list_empty(&thread->thread_list_entry_));

    pthread_rwlock_wrlock(&thread_map_lock);
    exist = thread_map_find_internal(thread->tid);
    if (exist != NULL){
        pthread_rwlock_unlock(&thread_map_lock);
        return -1;
    }

    head = thread_map + (thread->tid % THREAD_MAP_SIZE);
    list_add(&thread->thread_list_entry_, head);
    pthread_rwlock_unlock(&thread_map_lock);

    return 0;
}

// 从哈希表中删除, 并返回原来的对象
thread_t *
thread_map_del(pid_t thread_id)
{
    struct list_head *head;
    thread_t *exist;

    pthread_rwlock_wrlock(&thread_map_lock);
    exist = thread_map_find_internal(thread_id);
    if (exist != NULL){
        pthread_rwlock_unlock(&thread_map_lock);
        return NULL;
    }

    list_del(&exist->thread_list_entry_);
    pthread_rwlock_unlock(&thread_map_lock);

    return 0;
}

// 查找线程对象
thread_t *
thread_map_find(pid_t thread_id)
{
    thread_t *pos;

    pthread_rwlock_rdlock(&thread_map_lock);
    pos = thread_map_find_internal(thread_id);
    pthread_rwlock_unlock(&thread_map_lock);

    return pos;
}

