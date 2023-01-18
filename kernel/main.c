#include "print.h"
#include "init.h"
#include "memory.h"
#include "thread.h"

void k_a(void*);
void k_b(void*);

int main(){
    put_str("I am kernel\n");
    init_all();

    // void* addr = get_kernel_pages(3);
    // put_str("\n get_kerne_page start vaddr is ");
    // put_int((uint32_t vaddr));
    // put_str("\n");

    thread_start("k_a", 31, k_a, "argA");
    thread_start("k_b", 8, k_b, "argB");
    
    intr_enable();

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