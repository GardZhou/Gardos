#ifndef __KERNEL_INTERRUPT_H
#define __KERNEL_INERRUPT_H
#include "stdint.h"

typedef void* intr_handler;
void idt_init(void);

/* 定义中断的两种状态
    INTR_OFF
    INTR_ON
 */
typedef enum intr_status {
    INTR_OFF,
    INTR_ON 
}intr_status;

intr_status intr_get_status(void);
intr_status intr_set_status(intr_status);
intr_status intr_enable(void);
intr_status intr_disable(void);
void idt_init(void);

#endif