%include "boot.inc"
SECTION loader vstart=LOADER_BASE_ADDR
LOADER_STACK_TOP equ LOADER_BASE_ADDR
jmp loader_start

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

jmp dword SELECTOR_CODE:p_mode_start    ;刷新流水线

[bits 32]
p_mode_start:
    mov ax, SELECTOR_DATA
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, LOADER_STACK_TOP
    mov ax, SELECTOR_VIDEO
    mov gs, ax

    mov byte [gs:160], 'P'

    jmp $

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




; mov byte [gs:0x00], '2'
; mov byte [gs:0x01],0xA4     ;A表示绿色背景闪烁，4表示前景色为红色

; mov byte [gs:0x02], ' '
; mov byte [gs:0x03], 0x04

; mov byte [gs:0x04], 'L'
; mov byte [gs:0x05], 0xA4

; mov byte [gs:0x06], 'O'
; mov byte [gs:0x07], 0xA4

; mov byte [gs:0x08], 'A'
; mov byte [gs:0x09], 0xA4

; mov byte [gs:0x0a], 'D'
; mov byte [gs:0x0b], 0xA4

; mov byte [gs:0x0c], 'E'
; mov byte [gs:0x0d], 0xA4

; mov byte [gs:0x0e], 'R'
; mov byte [gs:0x0f], 0xA4

; jmp $

