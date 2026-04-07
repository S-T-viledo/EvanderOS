[bits 32]
; Assembly helper functions for I/O and task management / I/O 和仿b管理的汇编助手函数
section .text
global io_in8, io_in16, io_in32, io_out8, io_out16, io_out32

; Read 8-bit value from I/O port / 从 I/O 端口读取 8 位值
; unsigned char io_in8(unsigned short port) / unsigned char io_in8(unsigned short port)
io_in8:
	push ebp
	mov ebp, esp
	push edx
	mov dx, [ebp+8]  ; Get port parameter / 获取端口参数
	xor eax, eax
	in al, dx        ; Read 8-bit from port / 执行端口读取
	pop edx
	pop ebp
	ret

; Read 16-bit value from I/O port / 从 I/O 端口读取 16 位值
; unsigned short io_in16(unsigned short port)
io_in16:
	push ebp
	mov ebp, esp
	push edx
	mov dx, [ebp+8]  ; Get port parameter / 获取端口参数
	xor eax, eax
	in ax, dx        ; Read 16-bit from port / 执行端口读取
	pop edx
	pop ebp
	ret

io_in32:
	push ebp	
	mov ebp, esp
	push edx
	mov dx, [ebp+8]  ; Get port parameter / 获取端口参数
	in eax, dx       ; Read 32-bit from port / 执行端口读取
	pop edx
	pop ebp
	ret


; Write 8-bit value to I/O port / 向 I/O 端口写入 8 位值
; void io_out8(unsigned short port, unsigned char value)
io_out8:
	push ebp
	mov ebp, esp		
	push eax
	push edx
	mov dx, [ebp+8]   ; Get port parameter / 获取端口参数
	mov al, [ebp+12]  ; Get value parameter / 获取值参数
	out dx, al        ; Write to port / 向端口写入
	pop edx
	pop eax
	pop ebp
	ret


; Write 16-bit value to I/O port / 向 I/O 端口写入 16 位值
; void io_out16(unsigned short port, unsigned short value)
io_out16:
	push ebp
	mov ebp, esp		
	push eax
	push edx
	mov dx, [ebp+8]   ; Get port parameter / 获取端口参数
	mov ax, [ebp+12]  ; Get value parameter / 获取值参数
	out dx, ax        ; Write to port / 向端口写入
	pop edx
	pop eax
	pop ebp
	ret

; Write 32-bit value to I/O port / 向 I/O 端口写入 32 位值
; void io_out32(unsigned short port, unsigned int value)
io_out32:
	push ebp
	mov ebp, esp		
	push eax
	push edx
	mov dx, [ebp+8]   ; Get port parameter / 获取端口参数
	mov eax, [ebp+12] ; Get value parameter / 获取值参数
	out dx, eax       ; Write to port / 向端口写入
	pop edx
	pop eax
	pop ebp
	ret


; --------------------------------------------------------------------
; Task switching functions / 任务切换函数
; --------------------------------------------------------------------
global switch_to
global move_to_user_mode
; void switch_to(struct task_struct* cur, struct task_struct* next);
; Parameter cur is at [esp+4] in stack / 参数 cur 在栈中 [esp+4]
; Parameter next is at [esp+8] in stack / 参数 next 在栈中 [esp+8]
switch_to:
    ; 1. Save context of current task (cur) / 备份当前任务(cur)的上下文
    push esi
    push edi
    push ebx
    push ebp

    ; Get cur address (first parameter in stack) / 得到 cur 的地址(栈中第一个参数)
    mov eax, [esp + 20] ; 4 registers*4 + return address 4 = 20 / 4个寄存器*4 + 返回地址4 = 20
    ; Save current esp to cur->self_kstack / 保存当前的 esp 到 cur->self_kstack
    ; Note: first field of struct task_struct is self_kstack / 注意:struct task_struct 的第一个字段就是 self_kstack
    mov [eax], esp 

    ; 2. Restore context of next task (next) / 恢复下一个任务(next)的上下文
    ; Get next address (second parameter in stack) / 得到 next 的地址(栈中第二个参数)
    mov eax, [esp + 24]
    ; 从 next->self_kstack 恢复 esp
    mov esp, [eax]

    ; 3. 弹出 next 之前保存的寄存器
    pop ebp
    pop ebx
    pop edi
    pop esi

    ; 4. 返回
    ; 这里的 ret 会弹出栈顶的 EIP。
    ; 因为我们刚刚切换了 ESP，这个 EIP 是 next 线程上次暂停时的地址（或者我们伪造的函数地址）
    ret



; void move_to_user_mode(void (*function)(void), void* user_stack_top);
move_to_user_mode:
    ; 获取参数
    mov ebx, [esp + 4]    ; 参数1: 用户态要执行的函数地址 (function)
    mov ecx, [esp + 8]    ; 参数2: 用户态栈顶地址 (user_stack_top)

    ; --- 开始压栈 (模拟中断返回现场) ---
    
    ; 5. SS (用户数据段选择子)
    ; 你的 GDT 第 4 项是 User Data。Index=4, RPL=3 -> 4*8 | 3 = 0x23
    push 0x23
    
    ; 4. ESP (用户栈指针)
    push ecx
    
    ; 3. EFLAGS (标志寄存器)
    pushf           ; 先把当前的 EFLAGS 压进去
    pop eax         ; 弹出来改一下
    or eax, 0x200   ; 【重要】把 IF 位置 1 (开启中断！)
    push eax        ; 压回栈中
    
    ; 2. CS (用户代码段选择子)
    ; 你的 GDT 第 3 项是 User Code。Index=3, RPL=3 -> 3*8 | 3 = 0x1B
    push 0x1b
    
    ; 1. EIP (用户函数入口)
    push ebx
    
    ; --- 发射！ ---
    iret

;--------------------------------------------------------------------
;-------------------------------gdt----------------------------------
;--------------------------------------------------------------------
global gdt_flush
global tss_flush

gdt_flush:
    mov eax, [esp + 4]  ; 获取 GDT 指针参数
    lgdt [eax]          ; 加载 GDT

    ; 刷新段寄存器
    mov ax, 0x10        ; 内核数据段是第2个 (0x10)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; 远跳转刷新 CS
    jmp 0x08:.flush     ; 内核代码段是第1个 (0x08)
.flush:
    ret

tss_flush:
    mov ax, 0x28      ; TSS 描述符在 GDT 索引 5 (5 * 8 = 40 = 0x28)
    ltr ax            ; Load Task Register
    ret