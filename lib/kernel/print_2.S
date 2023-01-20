TI_GDT equ 0
RPL0 equ 0
SELECTOR_VIDEO equ (0x0003<<3)+TI_GDT+RPL0

[bits 32]
section .data
put_int_buffer dq 0         ;定义8字节缓冲区用于数字到字符的转换

section .text
;--------------put char----------------------
;把站中的一个字符写入光标所在处
global put_char
put_char:
    pushad      ;备份32位寄存器环境
    ;需要保证gs中为正确的是频段选择子
    ;为保险起见,每次打印时都为gs赋值
    mov ax,SELECTOR_VIDEO       ;不能直接吧立即数送入段寄存器
    mov gs,ax

;---------获取前光标位置-----------------
;共0-65525个端口(intel系统),为节省端口提供两个寄存器一个负责提供索引,一个负责输入输出来定位所有的寄存器
    ;先获得高8位
    mov dx,0x03d4       ;索引寄存器
    mov al,0x0e         ;用于提供光标位置的高8位
    out dx,al
    mov dx,0x03d5       ;通过读写数据端口0x03d5来获得或设置光标位置
    in al,dx            ;得到光标位置高8位

    ;再获取低8位
    mov dx,0x03d4
    mov al,0x0f
    out dx,al
    mov dx,0x03d5
    in al,dx

    ;将光标存入bx
    mov bx,ax
    ;下面这行是在栈中获取带打印的字符
    mov ecx,[esp+36]        ;pushad 压入4*8字节
                            ;加入主调函数4字节的返回地址
                            ;cr是0x0d,lf是0x0a
    cmp cl,0xd
    jz .is_carriage_return
    cmp cl,0xa
    jz .is_line_feed

    cmp cl,0x8
    jz .is_backspace
    jmp .put_other

.is_backspace:
    dec bx
    shl bx,1        ;光标左移1位等于乘2
                    ;表示光标对应显存中的偏移字节
    mov byte [gs:bx],0x20       ;将待删除的字节补为0或空格皆可
    inc bx
    mov byte [gs:bx],0x07
    shr bx,1
    jmp .set_cursor

.put_other:
    shl bx,1
    mov [gs:bx],cl          ;ASCII字符本身
    inc bx
    mov byte [gs:bx],0x07   ;字符属性
    shr bx,1                ;回复老的光标值
    inc bx                  ;下一个光标值
    cmp bx,2000
    jl .set_cursor          ;若光标值小于2000,表示未写到显存的最后,
                            ;设置新的光标值.若超出屏幕字数大小2000
                            ;则换行处理
.is_line_feed:
.is_carriage_return:
    ;如果是CR,只要把光标移到行首就行
    xor dx,dx               ;dx是被数的高16位,清零
    mov ax,bx               ;ax是被除数的低16位
    mov si,80               ;把\r \n都处理为\n
    div si
    sub bx,dx               ;光标值减去80的余数便是取整

.is_carriage_return_end:    ;回车CR处理结束
    add bx,80
    cmp bx,2000
.is_line_feed_end:          ;若是LF(\n),将光标移+80即可
    jl .set_cursor

;屏幕行范围是0-24,滚屏
.roll_screen:               ;若超出屏幕大小,则滚屏
    cld
    mov ecx,960             ;2000-80=1920个字符要搬运,共1920*2=3840字节
                            ;一次搬4bytes,共960次
    mov esi,0xc00b80a0      ;第一行行首
    mov edi,0xc00b8000
    rep movsd

;将最后一行填充为空白
    mov ebx,3840            ;偏移量
    mov ecx,80

.cls:
    mov word [gs:ebx],0x0720;0x0720是黑底白字的空格
    add ebx,2
    loop .cls
    mov bx,1920

.set_cursor:
    ;先设置高8位
    mov dx,0x03d4       ;索引寄存器
    mov al,0x0e         ;用于提供光标位置的高8位
    out dx,al
    mov dx,0x03d5       ;通过读写数据端口0x03d5来获得或设置光标位置
    mov al,bh
    out dx,al

    ;再设置低8位
    mov dx,0x03d4
    mov al,0x0f
    out dx,al
    mov dx,0x03d5
    mov al,bl
    out dx,al
.put_char_done:
    popad
    ret

global put_str
put_str:
    push ebx
    push ecx
    xor ecx,ecx
    mov ebx,[esp+12]    ;从站中得到待打印的字符串地址
.goon:
    mov cl,[ebx]
    cmp cl,0            ;如果处理到了字符串尾,跳到结束处返回
    jz .str_over
    push ecx
    call put_char
    add esp,4
    inc ebx
    jmp .goon
.str_over:
    pop ecx
    pop ebx
    ret

;------------将小端字节序的数字变成对应的ASCII后,倒置-------------------
global put_int
put_int:
    pushad
    mov ebp,esp
    mov eax,[ebp+4*9]       ;call的返回地址占4字节+pushad的32字节
    mov edx,eax
    mov edi,7               ;制定put_int_buf中的初始偏移量
    mov ecx,8               ;32位数字中,十六进制的位数是8个
    mov ebx,put_int_buffer

;将32位数字从低位到高位逐个处理
.16based_4bits:
    and edx,0x0000000F
    cmp edx,9
    jg .is_A2F
    add edx,'0'
    jmp .store 
.is_A2F:
    sub edx,10
    add edx,'A'
.store:
    mov [ebx+edi],dl
    dec edi
    shr eax,4
    mov edx,edx
    1oop .16based_4bits

;把高位连续的字符去掉,比如000123变为123
.ready_to_print:
    inc edi
.skip_prefix_0:
    cmp edi,8
    je .full0
.go_on_skip:
    mov cl,[put_int_buffer+edi]
    inc edi 
    cmp cl,'0' 
    je .skip_prefix_0
    dec edi
    jmp .put_each_num

.full0:
    mov cl,'0'
.put_each_num:
    push ecx
    call put_char
    add esp,4
    inc edi
    mov cl,[put_int_buffer+edi]
    cmp edi,8
    jl .put_each_num
    popad
    ret