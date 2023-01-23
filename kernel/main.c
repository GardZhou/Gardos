#include "print.h"
#include "init.h"
#include "memory.h"
#include "thread.h"
#include "interrupt.h"

void k_a(void*);
void k_b(void*);

int main(){
    put_str("I am kernel\n");
    init_all();

    // void* addr = get_kernel_pages(3);
    // put_str("\n get_kerne_page start vaddr is ");
    // put_int(((uint32_t)addr));
    // put_str("\n");

put_str("b1 ");
    thread_start("k_a", 30, k_a, "argA ");
    thread_start("k_b", 8, k_b, "argB ");
    put_str("b2 ");

    intr_enable();
put_str("b3 ");

    while(1) {
        put_str("main ");
    }
    return 0;
}

void k_a(void* arg) {
    char* para=arg;
    while(1) {
        put_str(para);
    }
}

void k_b(void* arg) {
    char* para = arg;
    while(1) {
        put_str(para);
    }
}