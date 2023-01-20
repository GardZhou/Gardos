%include "boot.inc"
; start->分段设置->开启保护模式->设置分页->加载内核并跳转
SECTION loader vstart=LOADER_BASE_ADDR
LOADER_STACK_TOP equ LOADER_BASE_ADDR
; jmp loader_start

;构建gdt及其内部的描述符
GDT_BASE:  dd   0x00000000
            dd  0x00000000
CODE_DESC:  dd  0x0000FFFF
            dd  DESC_CODE_HIGH4
DATA_STACK_DESC:    dd  0x0000FFFF
                    dd  DESC_DATA_HIGH4
VIDEO_DESC: dd  0x80000007 ;limit=(0xbffff-0xb8000)/4k=0x7
            dd  DESC_VIDEO_HIGH4    ;此时dp1为0
GDT_SIZE equ    $ - GDT_BASE
GDT_LIMIT equ   GDT_SIZE - 1
times 60 dq 0   ;此处预留60个描述符的空位

;相当与(CODE_DESC-GDT_BASE)/8 + TI_GDT + RPL0
SELECTOR_CODE equ (0x0001<<3) + TI_GDT + RPL0
SELECTOR_DATA equ (0x0002<<3) + TI_GDT + RPL0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

;以下为gdt的指针，前二字节是gdt界限，后4字节是gdt起始地址
gdt_ptr dw GDT_LIMIT
        dd GDT_BASE
loadermsg db '2 loader in real'

jmp loader_start


; mov byte [gs:160],'V'

; jmp $

loader_start:
    mov sp, LOADER_BASE_ADDR
    mov bp, loadermsg           ;ES:BP=字符串地址
    mov cx, 17                  ;CX=字符串长度
    mov ax, 0x1301              ;AH=13, AL=01h
    mov bx, 0x001f              ;页号为0(BH=0) 蓝底粉红字:BL=1fgh
    mov dx, 0x1800              ;
    int 0x10                    ;10h号中断

;------------------准备进入保护模式--------------------------
;1 打开A20
;2 加载gdt
;3 将cr0的pe位置设1

;--------------------打开A20----------------------------
in al, 0x92
or al, 0000_0010B
out 0x92, al

;--------------------加载GDT----------------------------
lgdt [gdt_ptr]

;--------------------cr0第0位置1------------------------
mov eax, cr0
or eax, 0x00000001
mov cr0, eax

;--------------加载kernel-----------------
mov eax, KERNEL_START_SECTOR ;kernel.bin所在的扇区号
mov ebx,KERNEL_BIN_BASE_ADDR ;从磁盘读出后写入到ebx指定的地址
mov ecx,200                  ;读入的扇区数

call rd_disk_m_32

;创建页目录并初始化页内存位图
call setup_page
;将描述附表地址以及偏移量写入内存gdt_ptr,一会用新地址重新加载
sgdt [gdt_ptr]
;将gdt描述符中视频段描述符的段基址+0xc0000000
mov ebx,[gdt_ptr+2]
or dword [ebx+0x18+4],0xc0000000
;视频段是第三个描述符,每个描述符8字节 故0x18
;段描述副的高4字节的最高位是段基质的31-24位

;将gdt的基质加上0xc0000000使其成为内核所在的高地址
add dword [gdt_ptr+2],0xc0000000
;站指针同样映射到内核地址
add esp,0xc0000000

;把页目录地址付给cr3
mov eax, PAGE_DIR_TABLE_POS
mov cr3, eax

;打开cr0的pg位
mov eax,cr0
or eax,0x80000000
mov cr0, eax

;开启分页后用gdt新的地址重新加载
lgdt [gdt_ptr]

jmp SELECTOR_CODE:enter_kernel      ;强制刷新流水线跟新gdt
enter_kernel:
    call kernel_init
    mov esp, 0xc009f000
    jmp KERNEL_ENTRY_POINT          ;用地址0x1500访问测试


; jmp dword SELECTOR_CODE:p_mode_start    ;刷新流水线

; [bits 32]
; p_mode_start:
;     mov ax, SELECTOR_DATA
;     mov ds, ax
;     mov es, ax
;     mov ss, ax
;     mov esp, LOADER_STACK_TOP
;     mov ax, SELECTOR_VIDEO
;     mov gs, ax

;     mov byte [gs:160], 'P'

;     jmp $

setup_page:
;先把页目录占用的空间逐字节清零
    mov ecx,4096
    mov esi,0
.clear_page_dir:
    mov byte [PAGE_DIR_TABLE_POS+esi],0
    inc esi
    loop .clear_page_dir

;开始创建页目录项(PDE)
.create_pde:  ;创建page directory entry
    mov eax,PAGE_DIR_TABLE_POS
    add eax,0x1000  ;此时eax为第一个页表的位置及属性
    mov ebx,eax     ;此处为ebx赋值，是为.create_pde做准备，ebx为基址

;下面将页目录项0和0xc00都存为第一个页表的地址，每个页表表示4MB内存
;这样0xc03fffff以下的地址和0x003fffff以下的地址都将指向相同的页表
;这是为将地址映射为内核地址做准备
    or eax,PG_US_U|PG_RW_W|PG_P
    ;页目录项的属性RW和P位置1,US为1,表示用户属性,所有特权级别都可以访问
    mov [PAGE_DIR_TABLE_POS + 0xc00],eax

;一个页表占用四个字节
    ;0xc00表示第768个页表占用的目录项,0xc00以上的目录项用于内核空间
    ;页表的0xc0000000-0xffffffff共计1G属于内核
    ;0x0-0xbfffffff共计3G属于用户进程
    sub eax,0x1000
    mov [PAGE_DIR_TABLE_POS+4092],eax
;使最后一个目录项指向页目录表自己的地址

;下面创建页表项PTE
    mov ecx,256     ;1M低端内存 每页大小4k=256
    mov esi,0
    mov edx, PG_US_U|PG_RW_W|PG_P  ;属性为7 US=1,RW-1,P=1
.create_pte:
    mov [ebx+esi*4],edx
    add edx,4096
    inc esi
    loop .create_pte

;创建内核其他页表的PDE
    mov eax,PAGE_DIR_TABLE_POS
    add eax,0x2000  ;此时eax为第二个页表的位置
    or eax,PG_US_U|PG_RW_W|PG_P
    mov ebx,PAGE_DIR_TABLE_POS
    mov ecx,254     ;范围为769-1022的所有目录项数量
    mov esi,769
.create_kernel_pde:
    mov [ebx+esi*4],eax
    inc esi
    add eax,0x1000
    loop .create_kernel_pde
    ret

;------------------将kernel.bin中的segment拷贝到编译的地址-----------------
kernel_init:
    xor eax,eax
    xor,ebx,ebx     ;ebx记录程序头表地址
    xor ecx,ecx     ;cx记录程序头表中的program header 数量
    xor edx,edx     ;dx记录program header尺寸,即e_phentsize

    mov dx,[KERNEL_BIN_BASE_ADDR+42]
        ;偏移文件42字节处的属性是即e_phentsize,表示program header大小
    mov ebx,[KERNEL_BIN_BASE_ADDR+28]
        ;此处是e_phoff
        ;表示第一个program header在文件中的偏移量

    add ebx,KERNEL_BIN_BASE_ADDR
    mov cx,[KERNEL_BIN_BASE_ADDR+44]
        ;偏移文件开始部分44字节的地方是e_phnum,表示有几个program header

.each_segment:
    cmp byte [ebx+0],PT_NULL
        ;若p_byte=PT_NULL 说明此program header为使用
    je .PT_NULL
    
    ;Wei memcpy压入参数 从右往左依次压入
    ;函数原型类似于memcpy(dst,src,size)
    push dword [ebx+16]
        ;program header中偏移16字节的地方是p_filesz
        ;压如memcpy的第三个参数size

    mov eax,[ebx+4]     ;据程序头4字节的是P_offset
    add eax, KERNEL_BIN_BASE_ADDR
        ;加上kernel.bin被加载到的物理位置,eax为该段的物理地址
    push eax    ;第二个参数
    push dword [ebx+8]
        ;偏移程序头8字节的位置是p_vaddr
    call mem_cpy
    add esp.12

.PT_NULL:
    add ebx,edx
        ;edx为程序头大小即E_phensize
        ;在此ebx指向下一个程序头
    loop .each_segment
    ret

;---------逐字节拷贝 mem_cpy(dts,src.size)--------------
mem_cpy:
    cld
    push ebp
    mov ebp,esp
    push ecx        ;rep指令用到了ecx
                    ;但ecx对外层段的循环还有用,先入站备份
    mov edi,[ebp+8]     ;dst
    mov esi,[ebp+12]    ;src
    mov ecx,[ebp+16]    ;size
    rep movsb

    ;恢复环境
    pop ecx
    pop ebp
    ret


rd_disk_m_32:
        ;eax:LBA扇区号
        ;ebx=将数据写入的内存地址
        ;ecx=读入扇区数
        mov esi, eax  ;备份eax
        mov edi,ecx     ;备份cx

        ;设置要读取的扇区数
        mov edx,0x1f2
        mov cl,al
        out edx,al

        mov eax,esi

        ;存入LBA地址到端口0x1f3-0x1f6
        mov edx,0x1f3
        out edx,al

        mov cl,8
        shr eax,cl
        mov edx,0x1f4
        out edx,al

        mov edx,0x1f5
        shr eax,cl
        out edx,al

        mov edx,0x1f6
        shr eax,cl      
        and al,0x0f     ;lba第24-27位
        or al,0xe0      ;设置 7-4位为1110,表示lba模式
        out edx,al

        mov edx,0x1f7   ;向0x1f7端口写入读命令,0x20
        mov al,0x20
        out edx,al

    ;检测硬盘状态
    .not_ready:
        nop
        in al,edx
        and al,0x88 ;第三位为1代表硬盘控制器已准备好数据传输
                    ;第7位为1代表硬盘忙
        cmp al,0x08
        jnz .not_ready
        ;读数据
        mov eax,edi
        mov edx,256
        mul edx
        mov ecx,eax

        mov edx,0x1f0

    .go_on_read:
        in eax,edx
        mov [ebx],eax
        add ebx,2
        loop .go_on_read
        ret



