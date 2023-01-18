#include "timer.h"
#include "io.h"
#include "print.h"
#include "thread.h"

#define IRQ0_FREQUENCY 100
#define INPUT_FREQUENCY 1193180
#define COUNTER0_VALUE INPUT_FREQUENCY / IRQ0_FREQUENCY
#define COUNTER0_PORT 0x40
#define COUNTER0_NO 0
#define COUNTER0_MDOE 2
#define READ_WRITE_LATCH 3
#define PIT_CONTROL_PORT 0x43

//把操作的计数器counter_no 读写锁属性rw1，计数器模式counter_mode写入模式控制寄存器
//并赋予初值counter-value
static void frequency_set(uint8_t counter_port, \
                            uint8_t counter_no, \
                            uint8_t rw1, \
                            uint8_t counter_mode, \
                            uint16_t counter_value) {
//往控制字寄存器端口0x43写入控制字
    outb(PIT_CONTROL_PORT, \
    (uint8_t)(counter_no << 6 | rw1 << 4 | counter_mode << 1));
//先写入counter-value的低8位
    outb(counter_port, (uint8_t)counter_value);
    //再写入高8位
    outb(counter_port, (uint8_t)(counter_value >> 8));       
}

uint32_t ticks=0;
// 时钟的中断处理函数
static void intr_timer_handler(void) {
   task_struct* cur_thread = running_thread();

   ASSERT(cur_thread -> stack_magic == 0x19870916); // 检查栈是否溢出

   cur_thread -> elapsed_ticks++; // 记录此线程占用处理器的总时间数
   ticks++; // 用户态和内核态的总时间数

   if(cur_thread -> ticks == 0) // 若线程的时间片用完了就开始调度新的进程上处理器
      schedule();
   else // 将当前线程的时间片 -1
      cur_thread -> ticks--;
}


/* 初始化PIT8253 */
void timer_init(void) {
    put_str("timer_init start\n");
    /* 设置8253的定时周期 */
    frequency_set(COUNTER0_PORT, \
                    COUNTER0_NO, \
                    READ_WRITE_LATCH, \
                    COUNTER0_MDOE, \
                    COUNTER0_VALUE);
    put_str("timer_init done\n");
}