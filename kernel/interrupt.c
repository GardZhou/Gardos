#include "interrupt.h"
#include "stdint.h"
#include "global.h"
#include "io.h"

#define IDT_DESC_CNT 0x21 //目前总共支持的中断数
#define PIC_M_CTRL 0x20
#define PIC_M_DATA 0x21
#define PIC_S_CTRL 0xa0
#define PIC_S_DATA 0xa1

#define EFLAGS_IF 0x00000200        //eflags寄存器中的if位为1
#define GET_EFLAGS(EFLAG_VAR) asm volatile("pushfl; popl %0" : "=g" (EFLAG_VAR))

//中断描述符结构体
struct gate_desc {
    uint16_t func_offset_low_word;
    uint16_t selector;
    uint8_t dcount;     //此项为双计数值，为门描述符中的第4字节
                        //为固定值，不用考虑
    uint8_t attribute;
    uint16_t func_offset_high_word;
};

//静态函数声明
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function);
static struct gate_desc idt[IDT_DESC_CNT];
char* intr_name[IDT_DESC_CNT];          //用于保存异常的名字
intr_handler idt_table[IDT_DESC_CNT];   //定义中断处理程序数组，在kernel.S中定义的intrXXentry
                                        //只是中断处理程序的入口,最终调用ide_table里的处理程序

extern intr_handler intr_entry_table[IDT_DESC_CNT];     //声明引用定义在kernel.S中的中断处理函数入口

//初始化可编程中断控制器8259A
static void pic_init(void) {
    outb (PIC_M_CTRL, 0x11);
    outb (PIC_M_DATA, 0x20);
    outb (PIC_M_DATA, 0x04);
    outb (PIC_M_DATA, 0x01);

    outb (PIC_S_CTRL, 0x11);
    outb (PIC_S_DATA, 0x28);
    outb (PIC_S_DATA, 0x02);
    outb (PIC_S_DATA, 0x01);

    outb (PIC_M_DATA, 0xfe);
    outb (PIC_S_DATA, 0xff);

    put_str("   pic_init done\n")
}


//创建中断门描述符
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function) {
    p_gdesc->func_offset_low_word = (uint32_t)function & 0x0000FFFF;
    p_gdesc->selector = SELECTOR_K_CODE;
    p_gdesc->dcount = 0;
    p_gdesc->attribute = attr;
    p_gdesc->func_offset_high_word = ((uint32_t)function & 0xFFFF0000) >> 16;
}

static void idt_desc_init(void) {
    int i;
    for(i = 0;i<IDT_DESC_CNT;++i) {
        make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
    }
    put_str("  idt_desc_init done\n");
}

static void general_intr_handler(uint8_t vec_nr) {
    if (vec_nr == 0x27 || vec_nr == 0x2f) {
        //IRQ7和IRQ15会产生伪中断，无需处理
        //0x2f是从片0x8259A上的最后一个IRQ引脚，保留项
        return ;
    }
    set_cursor(0);
    int cursor_pos = 0;
    while(cursor_pos < 320) {
        put_char(' ');
        cursor_pos++;
    }

    set_cursor(0);      //重置光标为左上角
    put_str("!      excetion message begin      \n");
    put_str(intr_name[vec_nr]);
    if (vec_nr == 14) {    //若为pagefault,打印缺失地址并暂停
        int page_fault_vaddr = 0;
        asm ("movl %%cr2, %0" : "=r" (page_fault_vaddr));   //cr2是存放造成pagefault的地址
        put_str("\npage fault addr is ");put_str(page_fault_vaddr);
    }
    put_str("!      excetion message end      \n");
    //能进入中断处理程序就说明已经关中断
    //不会中断死循环
    while(1);
}

void register_handler(uint8_t vector_no, intr_handler function) {
    idt_table[vector_no] = function;
}

static void exception_init(void) {
    int i;
    for(i=0;i<IDT_DESC_CNT;i++) {
        //idt_table中的函数是在进入中断后根据中断向量号调用的
        //见kernel中的call [idt_table + %1*4]
        idt_table[i] = general_intr_handler;
        //默认为general_intr_handler
        //以后由register_handler来注册具体处理函数
        intr_name[i] = "unknown";
    }

    intr_name[0] = "#DE Divide Error";
    intr_name[1] = "#DB Debug Exception";
    intr_name[2] = "NMI Interrupt";
    intr_name[3] = "#BP Breakpoint Exception";
    intr_name[4] = "#OF Overflow Exception";
    intr_name[5] = "#BR BOUND Range Exceeded Exception";
    intr_name[6] = "#UD Invalid Opcode Exception";
    intr_name[7] = "#NM Device Not Available Exception";
    intr_name[8] = "#DF Double Fault Exception";
    intr_name[9] = "Coprocessor Segment Overrun";
    intr_name[10] = "#TS Invalid TSS Exception";
    intr_name[11] = "#NP Segment Not Present";
    intr_name[12] = "#SS Stack Fault Exception";
    intr_name[13] = "#GP General Protection Exception";
    intr_name[14] = "#PF Page-Fault Exception";
    //intr_name[15]是intel保留项,未使用
    intr_name[16] = "#MF x87 FPU Floating-Point Error";
    intr_name[17] = "#AC Alignment Check Exception";
    intr_name[18] = "MC Machine-Check Exception";
    intr_name[19] = "#XF SIMD Floating-Point Exception";
}

//开中断，并返回开中断前的状态
intr_status intr_enable() {
    intr_status old_status;
    if (INTR_ON == intr_get_status()) {
        old_status = INTR_ON;
        return old_status;
    } else {
        old_status = INTR_OFF;
        asm volatile("sti");        //开中断，sti指令将IF位置1
        return old_status;
    }
}

//关中断,并且返回关中断前的状态
intr_status intr_disable() {
    intr_status old_status;
    if (INTR_ON == intr_get_status()) {
        old_status = INTR_ON;
        asm volatile("cli" : : : "memory");     //开中断,cli指令将IF位置0
        return old_status;
    } else {
        old_status = INTR_OFF;
        return old_status;
    }
}

//将中断状态设置为status
intr_status intr_set_status(intr_status status) {
    return status & INTR_ON ? intr_enable() : intr_disable();
}

//获取当前中断状态
intr_status intr_get_status() {
    uint32_t eflags = 0;
    GET_EFLAGS(eflags);
    return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF;
}

//完成有关中断的所有初始化工作
void idt_init() {
    put_str("idt_init start\n");
    idt_desc_init();
    exception_init();   //异常名初始化并注册通常的中断处理函数
    pic_init();         //初始化8259A

    //加载idt
    uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t)((uint32_t)idt << 16)));
    asm volatile("lidt %0" : : "m" (idt_operand));
    put_str("idt_init done\n");
}