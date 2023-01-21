;主引导程序
%include "boot.inc"
SECTION MBR vstart=0x7c00
    mov ax,cs
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov fs,ax
    mov sp,0x7c00
    mov ax,0xb800
    mov gs,ax

; 清屏利用0x06号功能,上卷全部行,则可清屏。
; INT 0X10 功能号:0x06
; AH 功能号=0x06
; AL = 上卷的行数(0,表示全部)
; BH = 上卷行属性
; (CL,CH) = 窗口左上角的(X,Y)位置
; (DL,DL) = 窗口右下角的(X,Y)位置
; 无返回值
    mov ax, 0x600
    mov bx, 0x700
    mov cx, 0            ;左上角：（0,0）
    mov dx, 0x184f       ;右下角（80,25）

    int 0x10

    mov byte [gs:0x00], '1'
    mov byte [gs:0x01],0xA4     ;A表示绿色背景闪烁，4表示前景色为红色

    mov byte [gs:0x02], ' '
    mov byte [gs:0x03], 0x04

    mov byte [gs:0x04], 'M'
    mov byte [gs:0x05], 0xA4

    mov byte [gs:0x06], 'B'
    mov byte [gs:0x07], 0xA4

    mov byte [gs:0x08], 'R'
    mov byte [gs:0x09], 0xA4

    mov eax,LOADER_START_SECTOR     ;起始扇区LBA地址
    mov bx,LOADER_BASE_ADDR         ;写入的地址
    mov cx,4                        ;待读入的扇区数
    call rd_disk_m_16

    jmp LOADER_BASE_ADDR + 0x300

    ;--------------------------------------------
    ;函数：读取硬盘n个盘区
    ;--------------------------------------------
rd_disk_m_16:
    ;eax:LBA扇区号
    ;bx=将数据写入的内存地址
    ;cx=读入扇区数
    mov esi, eax  ;备份eax
    mov di,cx     ;备份cx

    ;设置要读取的扇区数
    mov dx,0x1f2
    mov cl,al
    out dx,al

    mov eax,esi

    ;存入LBA地址
    mov dx,0x1f3
    out dx,al

    mov cl,8
    shr eax,cl
    mov dx,0x1f4
    out dx,al

    mov dx,0x1f5
    shr eax,cl
    out dx,al

    mov dx,0x1f6
    shr eax,cl
    and al,0x0f
    or al,0xe0
    out dx,al

    mov dx,0x1f7
    mov al,0x20
    out dx,al

;检测硬盘状态
.not_ready:
    nop
    in al,dx
    and al,0x88 ;第三位为1代表硬盘控制器已准备好数据传输
                ;第7位为1代表硬盘忙
    cmp al,0x08
    jnz .not_ready
    ;读数据
    mov ax,di
    mov dx,256
    mul dx
    mov cx,ax

    mov dx,0x1f0

.go_on_read:
    in ax,dx
    mov [bx],ax
    add bx,2
    loop .go_on_read
    ret

    ; jmp $

times 510-($-$$) db 0
db 0x55, 0xaa


; ; 下面3行获取光标位置
;     mov ah, 3
;     mov bh, 0

;     int 0x10

; ; 打印字符串
;     mov ax, message
;     mov bp,ax       ;es:bp为串首地址,es此时同cs一致

; ; 光标位置要用到dx寄存器中内容，cx中光标位置可忽略
;     mov cx, 5
;     mov ax, 0x1301

;     mov bx, 0x2
    
;     int 0x10

;     jmp $

;     message db "1 MBR"
;     times 510-($-$$) db 0
;     db 0x55, 0xaa

