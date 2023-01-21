#include "memory.h"
#include "stdint.h"
#include "print.h"
#include "bitmap.h"
#include "string.h"
#include "debug.h"

#define PG_SIZE 4096

/************************** 位图地址 ******************************
 * 因为0xc009f000是内核主线程栈项, 0xc009e000是内核主线程的pcb
 * 一个页框大小的位图可表示128MB内存,位图位置安排在地址0xc009a000
 * 这样本系统最大支持4个页框的位图,即512MB
*****************************************************************/
#define MEM_BITMAP_BASE 0xc009a000

#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

/* 0xc0000000是内核从虚拟地址3G起
    0x100000意指跨国低端1MB内存，让虚拟地址在逻辑上连续 */
#define K_HEAP_START 0xc0100000

/* 内存池结构 */
typedef struct pool {
    bitmap pool_bitmap;
    uint32_t phy_addr_start;
    uint32_t pool_size;
}pool;

pool kernel_pool, user_pool;
virtual_addr kernel_vaddr;

/* 在pf表示的虚拟内存池中申请pg_cnt个虚拟页 
 * 成功返回虚拟页起始地址，失败返回NULL */
static void* vaddr_get(enum pool_flags pf, uint32_t pg_cnt) {
    int vaddr_start = 0, bit_idx_start = -1;
    uint32_t cnt = 0;
    if(pf == PF_KERNEL) {
        bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);
        if(bit_idx_start == -1) {
            return NULL;
        }
        while(cnt < pg_cnt) {
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt, 1);
            cnt++;
        }
        vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
    } else {
        //用户内存池
    }
    return (void*)vaddr_start;
}

/* 得到虚拟地址vaddr对应的pte指针 */
uint32_t* pte_ptr(uint32_t vaddr) {
    /* 先访问到页表自己 + \
     * 再用页目录项pde（页目录内页表的索引）作为pte的索引访问到页表 + \
     * 再用pte的索引作为页内偏移 */
    uint32_t* pte = (uint32_t*)(0xffc00000 + \
        ((vaddr & 0xffc00000) >> 10) + \
        PTE_IDX(vaddr) * 4);
    return pte;
}

/* 得到虚拟地址vaddr对应的pde指针 */
uint32_t* pde_ptr(uint32_t vaddr) {
    /* 0xfffff用来访问到页表本身所在的地址 */
    uint32_t* pde = (uint32_t*)((0xfffff000) + PDE_IDX(vaddr) * 4);
    return pde;
}

/* 在m_pool指向的物理内存池中分配一个物理页
 * 成功则返回地址，失败返回NULL */
static void* palloc(pool* m_pool) {
    /*扫描或设置位图要保证原子操作*/
    int bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1);
    if(bit_idx == -1) {
        return NULL;
    }
    bitmap_set(&m_pool->pool_bitmap,bit_idx , 1);
    uint32_t page_phyaddr = ((bit_idx * PG_SIZE) + m_pool->phy_addr_start);
    return (void*)page_phyaddr;
}

/* 页表中添加虚拟地址vaddr和物理地址page_ohyaddr */
static void page_table_add(void* _vaddr, void* _page_phyaddr) {
    uint32_t vaddr = (uint32_t)_vaddr;
    uint32_t page_phyaddr = (uint32_t)_page_phyaddr;
    uint32_t* pde = pde_ptr(vaddr);
    uint32_t* pte = pte_ptr(vaddr);

    //执行*pte会访问到空的*pde.所以pde创建完成后才能执行*pte
    //先在页目录内判断目录项的P位,若为1，则表示该表已存在
    if(*pde & 0x00000001) {
        //页目录项和页表项的第0位为P，此处判断目录项是否存在
        ASSERT(!(*pte & 0x00000001));
        if(!(*pte & 0x00000001)) {
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        } else {
            PANIC("pte repeat");
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        }
    } else { //目录项不存在。所以要先创建页目录项再创建页表项
        /* 页表用到的页框一律从内核空间先分配 */
        uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);
        *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        /* 分配到的物理页地址pde_phyaddr对应的物理内存清0
         * 避免里面的陈旧数据变成了页表项
         * 访问到pde对应的物理地址, 用pte取高20位即可
         * 因为pte基于该地址的物理地址再寻址
         * 把低20位置0便是该pde对应的物理页的起始 */
        memset((void*)((int)pte & 0xfffff000), 0, PG_SIZE);

        ASSERT(!(*pte & 0x00000001));
        *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    }

}

/* 分配pg_cnt个页空间，成功则返回起始虚拟地址，失败返回NULL */
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt) {
    ASSERT(pg_cnt > 0 && pg_cnt < 3840);
    /* 主要为三个动作的合成：
     * 1.通过vaddr_get在虚拟内存池中申请虚拟地址
     * 2.通过palloc在物理内存池中申请物理页
     * 3.通过page_table_add完成两者映射 */
    void* vaddr_start = vaddr_get(pf,pg_cnt);
    if(vaddr_start == NULL) {
        return NULL;
    }
    uint32_t vaddr = (uint32_t)vaddr_start;
    uint32_t cnt = pg_cnt;

    pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;

    /* 虚拟地址连续而物理地址连续，逐个做映射 */
    while(cnt-- > 0) {
        void* page_phyaddr = palloc(mem_pool);
        if(page_phyaddr == NULL) {      //失败时要将已申请的虚拟地址和物理页全部回收
            return NULL;
        }
        page_table_add((void*)vaddr, page_phyaddr);     //在页表中做映射
        vaddr += PG_SIZE;
    }
    return vaddr_start;
}

/* 从内核物理池中申请一页内存
 * 成功返回虚拟地址，失败返回NULL */
void* get_kernel_pages(uint32_t pg_cnt) {
    void* vaddr = malloc_page(PF_KERNEL, pg_cnt);
    if(vaddr != NULL) {
        memset(vaddr, 0, pg_cnt * PG_SIZE);
    }
    return vaddr;
}

/* 初始化内存池 */
static void mem_pool_init(uint32_t all_mem) {
    put_str("   mem_pool_init start\n");
    uint32_t page_table_size = PG_SIZE * 256;
        //页表大小=1页的页目录项+第0和第768个页目录项指向同一个页表+
        //第769-1022个页目录项共指向254个页表，共256个页框
    
    uint32_t used_mem = page_table_size + 0x100000;
    uint32_t free_mem = all_mem - used_mem;
    uint16_t all_free_pages = free_mem / PG_SIZE;

    uint16_t kernel_free_pages = all_free_pages / 2;
    uint16_t user_free_pages = all_free_pages - kernel_free_pages;

    /* 为简化位图操作，余数不处理，坏处是会丢失内存，
    好处是不用做内存的越界检查，因为位图表示的内存少于实际物理内存 */
    //kernel bitmap的长度,位图中一位表示一页,以字节为单位
    uint32_t kbm_length = kernel_free_pages / 8;
    uint32_t ubm_length = user_free_pages / 8;

    //kernel pool start 
    uint32_t kp_start = used_mem;
    uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;

    kernel_pool.phy_addr_start = kp_start;
    user_pool.phy_addr_start = up_start;

    kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
    user_pool.pool_size = user_free_pages * PG_SIZE;

    kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
    user_pool.pool_bitmap.btmp_bytes_len = ubm_length;

    /*******************    内核内存池和用户内存池位图      **********************
     * 位图是全局的数据，长度不固定
     * 全局或静态数组需要在编译知道其长度
     * 而我们需要根据总内存大小算出需要多少字节
     * 所以改为指定一块内存来生成位图
    **************************************************************************/
    //内核使用的最高地址为0xc009f000，这是主线程的栈地址
    //32MB内存占用的位图为2kb
    //内核内存池位图先定于0xc009a000处
    kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;
    user_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length);

    /********************   输出内存池信息  ***********************/
    put_str("   kernel_pool_bitmap_start:");
    put_str((int)kernel_pool.pool_bitmap.bits);
    put_str(" kernel_pool_phy_addr_start:");
    put_int(kernel_pool.phy_addr_start);
    put_str("\n");

    put_str("user_pool_bitmap_start:");
    put_str((int)user_pool.pool_bitmap.bits);
    put_str(" user_pool_phy_addr_start:");
    put_int(user_pool.phy_addr_start);
    put_str("\n");

    /* 将位图置0 */
    bitmap_init(&kernel_pool.pool_bitmap);
    bitmap_init(&user_pool.pool_bitmap);

    /* 初始化内核虚拟地址的位图,按实际物理大小生成数组 */
    //用于维护内核堆的虚拟地址，故大小和内核内存池大小一致
    kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;
    kernel_vaddr.vaddr_bitmap.bits = \
        (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);

    kernel_vaddr.vaddr_start = K_HEAP_START;
    bitmap_init(&kernel_vaddr.vaddr_bitmap);
    put_str("   mem_pool_init done\n");
}

/* 内核管理部分初始化入口 */
void mem_init(void) {
    put_str("mem_init start\n");
    uint32_t mem_bytes_total = (*(uint32_t*)(0xb00));
    mem_pool_init(mem_bytes_total);
    put_str("mem_init done\n");
}

