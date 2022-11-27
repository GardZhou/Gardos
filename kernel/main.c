#include "print.h"
#include "init.h"
#include "memory.h"

int main(){
    put_str("I am kernel\n");
    init_all();

    void* addr = get_kernel_pages(3);
    put_str("\n get_kerne_page start vaddr is ");
    put_int((uint32_t vaddr));
    put_str("\n");
    
    while(1);
    return 0;
}