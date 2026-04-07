[bits 32]
global handler_entry_table
global syscall_handler_asm
extern syscall_router
extern common_interrupt_handler_c
; 通用的中断处理程序
%macro INTERRUPT_HANDLER 2
global interrupt_handler_%1
interrupt_handler_%1:
    %if %2 == 0
        push 0    ; 没有错误码的中断，压入0作为占位
    %endif
    push %1       ; 压入中断号
    jmp common_interrupt_handler ; 跳转到公共处理程序
%endmacro

extern idtptr
global load_idt
load_idt:
	lidt [idtptr]
    ret

; 公共中断处理程序
common_interrupt_handler:
    ; 保存所有寄存器
    pusha
    push ds
    push es
    push fs
    push gs
    
    ; 设置内核数据段
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; 调用C中断处理函数
    push esp     ; 传递寄存器状态指针
    call common_interrupt_handler_c
    add esp, 4   ; 清理栈
    
    ; 恢复寄存器
    pop gs
    pop fs
    pop es
    pop ds
    popa
    
    ; 清理错误码和中断号
    add esp, 8
    iret



syscall_handler_asm:
    ; 1. 压入中断号和错误码
    push 0          ; Err Code
    push 0x30       ; Int Num

    ; 2. 保存现场 (Push All)
    ; 为了匹配 struct registers，先 pusha，再 push 段寄存器
    pusha           ; Pushes EDI, ESI, EBP, SP, EBX, EDX, ECX, EAX
    push ds
    push es
    push fs
    push gs

    ; 3. 切换内核段寄存器
    mov ax, 0x10    ; Kernel Data Segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; 4. 调用 C 语言路由函数
    push esp        ; 传入 regs 指针
    call syscall_router
    add esp, 4      ; 平衡栈

    ; 5. 【关键】处理返回值
    ; 这里的偏移量变了！
    ; 4个段寄存器(16字节) + pusha中EAX前面有7个寄存器(28字节) = 44字节
    ; syscall中已通过栈传出返回值时去掉这句，防止被覆盖

    ; 6. 恢复现场
    pop gs
    pop fs
    pop es
    pop ds
    popa
    
    add esp, 8      ; 跳过 Int Num 和 Err Code
    iret


global start_process

start_process:
                ; 此时 ESP 正好指向 process_execute 中构造的 regs->gs
    add esp, 8

    pop gs
    pop fs
    pop es
    pop ds
    popa           ; 弹出 edi, esi, ebp, esp(忽略), ebx, edx, ecx, eax
    add esp, 8     ; 跳过 int_num 和 err_code (4 + 4 = 8 字节)
    iret           ; 弹出 CS, EIP, EFLAGS, SS, ESP -> 进入 Ring3

global ret_from_fork
ret_from_fork:
    ; 此时 ESP 指向子进程 PCB 页面中 regs 的开头 (即 gs)
    pop gs
    pop fs
    pop es
    pop ds
    popa           ; 弹出 edi...eax
    add esp, 8     ; 跳过 int_num 和 err_code
    iret           ; 完美返回用户态


INTERRUPT_HANDLER 0x00, 0; divide by zero
INTERRUPT_HANDLER 0x01, 0; debug
INTERRUPT_HANDLER 0x02, 0; non maskable interrupt
INTERRUPT_HANDLER 0x03, 0; breakpoint

INTERRUPT_HANDLER 0x04, 0; overflow
INTERRUPT_HANDLER 0x05, 0; bound range exceeded
INTERRUPT_HANDLER 0x06, 0; invalid opcode
INTERRUPT_HANDLER 0x07, 0; device not avilable

INTERRUPT_HANDLER 0x08, 1; double fault
INTERRUPT_HANDLER 0x09, 0; coprocessor segment overrun
INTERRUPT_HANDLER 0x0a, 1; invalid TSS
INTERRUPT_HANDLER 0x0b, 1; segment not present

INTERRUPT_HANDLER 0x0c, 1; stack segment fault
INTERRUPT_HANDLER 0x0d, 1; general protection fault
INTERRUPT_HANDLER 0x0e, 1; page fault
INTERRUPT_HANDLER 0x0f, 0; reserved

INTERRUPT_HANDLER 0x10, 0; x87 floating point exception
INTERRUPT_HANDLER 0x11, 1; alignment check
INTERRUPT_HANDLER 0x12, 0; machine check
INTERRUPT_HANDLER 0x13, 0; SIMD Floating - Point Exception

INTERRUPT_HANDLER 0x14, 0; Virtualization Exception
INTERRUPT_HANDLER 0x15, 1; Control Protection Exception
INTERRUPT_HANDLER 0x16, 0; reserved
INTERRUPT_HANDLER 0x17, 0; reserved

INTERRUPT_HANDLER 0x18, 0; reserved
INTERRUPT_HANDLER 0x19, 0; reserved
INTERRUPT_HANDLER 0x1a, 0; reserved
INTERRUPT_HANDLER 0x1b, 0; reserved

INTERRUPT_HANDLER 0x1c, 0; reserved
INTERRUPT_HANDLER 0x1d, 0; reserved
INTERRUPT_HANDLER 0x1e, 0; reserved
INTERRUPT_HANDLER 0x1f, 0; reserved

INTERRUPT_HANDLER 0x20, 0; clock 时钟中断
INTERRUPT_HANDLER 0x21, 0; keyboard 键盘中断
INTERRUPT_HANDLER 0x22, 0; cascade 级联 8259
INTERRUPT_HANDLER 0x23, 0; com2 串口2
INTERRUPT_HANDLER 0x24, 0; com1 串口1
INTERRUPT_HANDLER 0x25, 0; sb16 声霸卡
INTERRUPT_HANDLER 0x26, 0; floppy 软盘
INTERRUPT_HANDLER 0x27, 0
INTERRUPT_HANDLER 0x28, 0; rtc 实时时钟
INTERRUPT_HANDLER 0x29, 0
INTERRUPT_HANDLER 0x2a, 0
INTERRUPT_HANDLER 0x2b, 0; nic 网卡
INTERRUPT_HANDLER 0x2c, 0
INTERRUPT_HANDLER 0x2d, 0
INTERRUPT_HANDLER 0x2e, 0; harddisk1 硬盘主通道
INTERRUPT_HANDLER 0x2f, 0; harddisk2 硬盘从通道
INTERRUPT_HANDLER 0x30, 0; syscall
INTERRUPT_HANDLER 0x31, 0; debug



handler_entry_table:
    dd interrupt_handler_0x00
    dd interrupt_handler_0x01
    dd interrupt_handler_0x02
    dd interrupt_handler_0x03
    dd interrupt_handler_0x04
    dd interrupt_handler_0x05
    dd interrupt_handler_0x06
    dd interrupt_handler_0x07
    dd interrupt_handler_0x08
    dd interrupt_handler_0x09
    dd interrupt_handler_0x0a
    dd interrupt_handler_0x0b
    dd interrupt_handler_0x0c
    dd interrupt_handler_0x0d
    dd interrupt_handler_0x0e
    dd interrupt_handler_0x0f
    dd interrupt_handler_0x10
    dd interrupt_handler_0x11
    dd interrupt_handler_0x12
    dd interrupt_handler_0x13
    dd interrupt_handler_0x14
    dd interrupt_handler_0x15
    dd interrupt_handler_0x16
    dd interrupt_handler_0x17
    dd interrupt_handler_0x18
    dd interrupt_handler_0x19
    dd interrupt_handler_0x1a
    dd interrupt_handler_0x1b
    dd interrupt_handler_0x1c
    dd interrupt_handler_0x1d
    dd interrupt_handler_0x1e
    dd interrupt_handler_0x1f
    dd interrupt_handler_0x20
    dd interrupt_handler_0x21
    dd interrupt_handler_0x22
    dd interrupt_handler_0x23
    dd interrupt_handler_0x24
    dd interrupt_handler_0x25
    dd interrupt_handler_0x26
    dd interrupt_handler_0x27
    dd interrupt_handler_0x28
    dd interrupt_handler_0x29
    dd interrupt_handler_0x2a
    dd interrupt_handler_0x2b
    dd interrupt_handler_0x2c
    dd interrupt_handler_0x2d
    dd interrupt_handler_0x2e
    dd interrupt_handler_0x2f
    dd interrupt_handler_0x30
    dd interrupt_handler_0x31
