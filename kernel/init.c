#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "timer.h"
#include "memory.h"
#include "thread.h"
#include "sync.h"
#include "console.h"
#include "keyboard.h"

//初始化所有模块
void init_all() {
    put_str("init_all\n");
    idt_init();         //初始化中断
    mem_init();
    thread_init();
    timer_init();
    console_init();   //控制台初始化最好放在开中断之前
    keyboard_init();  // 键盘初始化
}