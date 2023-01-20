#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H
#include "stdint.h"
#include "list.h"

/* 自定义通用函数类型, 在很多线程函数中作为形参类型 */
typedef void thread_func(void*);

/* 进程或线程的状态 */
enum task_status {
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_HANGING,
    TASK_DIED,
};

/***************       中断栈intr_stack     ****************
 * 此结构用于中断发生时保存进程/线程的上下文环境
 * intr_exit的出栈操作是此结构的逆向操作
 * 此栈在线程自己的内核栈中位置固定,所在页的最顶端
 *********************************************************/
typedef struct intr_stack {
    uint32_t vec_no;        //kernel.S宏 宏VECTOR中push %1压入的中断号
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;
    //虽然pushad把esp压入，但是esp在不断变化，所以会被popad忽略
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

    /* 以下由cpu从低特权级进入高特权级时压入 */
    uint32_t error_code;        //error_code会在eip之后被压入
    uint32_t (*eip) (void);
    uint32_t cs;
    uint32_t eflags;
    void* esp;
    uint32_t ss;
}intr_stack;

/********************   线程栈thread_stack  ******************
 * 线程自己的栈，用户存储线程中待执行的函数
 * 此结构在线程自己的内核栈中不固定
 * 仅用在switch_to时博爱村线程环境
 * 实际位置取决于实际情况
 ************************************************************/
typedef struct thread_stack {
   uint32_t ebp;
   uint32_t ebx;
   uint32_t edi;
   uint32_t esi;

   /* 线程第一次执行时，eip指向待调用的函数kernel_thread
    * 其他时候,eip是指向switch_to的返回地址 */ 
   void (*eip) (thread_func* func, void* func_arg);

   /***** 以下仅供第一次被cpu调度时使用 *****/

   /* 参数unused_ret只为占位置充数为返回地址 */
   void (*unused_ret_addr);
   thread_func* function;
   void* func_arg;
}thread_stack;

typedef struct task_struct {
    uint32_t* self_kstack;      //各内核都用自己的内核栈
    enum task_status status;
    uint8_t priority;
    char name[16];
    uint8_t ticks;

    //此任务自运行起，占用了多少cpu滴答数
    uint32_t elapsed_ticks;

    //general_tag用于线程在一般队列中的节点
    list_elem general_tag;

    //用于thread_list_all中的结点
    list_elem all_list_tag;

    uint32_t pgdir;             //进程自己虚拟页表的地址
    uint32_t stack_magic;       //栈的边界标记，用于检测栈的溢出
}task_struct;


#endif