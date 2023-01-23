#include "thread.h"
#include "global.h"
#include "string.h"
#include "memory.h"
#include "list.h"
#include "interrupt.h"
#include "debug.h"

#define PG_SIZE 4096

task_struct* main_thread;
list thread_ready_list;         //就绪队列
list thread_all_list;           //所有任务队列
static list_elem* thread_tag;  //保存队列中的线程节点

extern void switch_to(task_struct* cur, task_struct* next);

/* 获取当前线程pcb的指针 */
task_struct* running_thread() {
    uint32_t esp;
    asm volatile ("mov %%esp, %0" : "=g" (esp));
    /* 取esp整数部分,即pcb起始地址 */
    return (task_struct*)(esp & 0xfffff000);
}

/* 由kernel_thread执行function(func_arg) */
static void kernel_thread(thread_func* function, void* func_arg) {
/* 执行function前要开中断,避免后面的时钟中断被屏蔽,而无法调度其它线程 */
    intr_enable();
    function(func_arg);
}

/* 初始化线程thread_stack
    将待执行的函数和参数放到thread_stack相应的位置 */
void thread_create(task_struct* pthread, thread_func function, void* func_arg) {
    //预留中断使用栈的空间
    pthread->self_kstack -= sizeof(intr_stack);

    //留出线程栈空间
    pthread->self_kstack -= sizeof(thread_stack);
    thread_stack* kthread_stack = (thread_stack*)pthread->self_kstack;
    kthread_stack->eip = kernel_thread;
    kthread_stack->function = function;
    kthread_stack->func_arg = func_arg;
    kthread_stack->ebp = kthread_stack->ebx = \
    kthread_stack->esi = kthread_stack->edi = 0;
}

void init_thread(task_struct* pthread, char* name, int prio) {
    memset(pthread, 0, sizeof(*pthread));
    strcpy(pthread->name, name);

    if(pthread == main_thread) {
        pthread->status = TASK_RUNNING;
    } else {
        pthread->status = TASK_READY;
    }
    //self_kstack是线程自己在内核态使用的栈顶地址
    pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
    pthread->priority = prio;
    pthread->ticks = prio;
    pthread->elapsed_ticks = 0;
    pthread->pgdir = NULL;
    pthread->stack_magic = 0x19870916;  //自定义魔数
}

task_struct* thread_start(char* name, int prio,thread_func function, void* func_arg) {
    //包括用户进程在内的pcb都位于内核空间
    task_struct* thread = get_kernel_pages(1);

    init_thread(thread, name, prio);
    thread_create(thread, function, func_arg);

    //确保之前不在队列中
    ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
    /* 加入就绪队列 */
    list_append(&thread_ready_list, &thread->general_tag);

    //确保之前不在队列中
    ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
    /* 加入全部线程队列 */
    list_append(&thread_all_list, &thread->all_list_tag);

    // asm volatile ("mov %0, %%esp; \
    //                 pop %%ebp; pop %%ebx; pop %%edi; pop %%esi; \
    //                 ": : "g" (thread->self_kstack) : "memory");
    return thread;
}

// 任务调度
void schedule() {
    ASSERT(intr_get_status() == INTR_OFF);

    task_struct* cur = running_thread(); // 获取当前正在执行的线程的 PCB 地址

    if(cur -> status == TASK_RUNNING) { // 判断当前线程的时间片是否到期
        ASSERT(!elem_find(&thread_ready_list, &cur -> general_tag));
        // 那么就加入就绪队列
        list_append(&thread_ready_list, &cur -> general_tag);
        cur -> ticks = cur -> priority; // 重置时间片
        cur -> status = TASK_READY;     // 重置状态为就绪状态
    } else {
        /* 若此线程需要某事件发生后才能继续上cpu运行,
           不需要将其加入队列,因为当前线程不在就绪队列中。
        */
    }

    ASSERT(!list_empty(&thread_ready_list));
    thread_tag = NULL;
    // 弹出队列的首元素，准备将其调度上处理器上执行
    thread_tag = list_pop(&thread_ready_list);
    task_struct* next = elem2entry(struct task_struct, general_tag, thread_tag);
    next -> status = TASK_RUNNING;
    switch_to(cur, next);
}


/* 将kernel中的main函数完善为主线程 */
static void make_main_thread(void) {
/* 因为main线程早已运行,咱们在loader.S中进入内核时的mov esp,0xc009f000,
就是为其预留了tcb,地址为0xc009e000,因此不需要通过get_kernel_page另分配一页*/
   main_thread = running_thread();
   init_thread(main_thread, "main", 31);

/* main函数是当前线程,当前线程不在thread_ready_list中,
 * 所以只将其加在thread_all_list中. */
   ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));
   list_append(&thread_all_list, &main_thread->all_list_tag);
}

/* 初始化线程环境 */
void thread_init(void) {
   put_str("thread_init start\n");
   list_init(&thread_ready_list);
   list_init(&thread_all_list);
/* 将当前main函数创建为线程 */
   make_main_thread();
   put_str("thread_init done\n");
}
