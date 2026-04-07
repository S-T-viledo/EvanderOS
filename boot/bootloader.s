[org 0x7c00]  ; Bootloader loaded at 0x7C00 by BIOS / 引导加载程序由 BIOS 加载到 0x7C00
[bits 16]      ; 16-bit real mode / 16位实模式

jmp short start  ; Jump to start / 跳转到开始
nop
times 87 db 0    ; Reserved for FAT32 info / 用于填充FAT32信息的保留空间
start:
	cli						; Clear interrupt / 清除中断
							; Initialize segments / 初始化段寄存器
	mov ax, 0
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov fs, ax
	mov ax, 0xb800
	mov gs, ax

	mov sp, 0x7c00  		; Set stack pointer / 设置栈指针
	sti						; Enable interrupt / 启用中断

; show_nothing:  ; Commented out debug code / 注释掉的调试代码
;     MOV AL,0x13
;     MOV AH,0x00
;     INT 0x10
; 	mov al, 0x44
; 	mov ah, 0x0e
; 	int 0x10
; 	mov ax, 0x0003
; 	int 0x10

	; Load kernel sectors / 加载内核扇区
	mov eax, 0x08   ; Start sector / 起始扇区
	mov bx, 0x8000  ; Load address / 加载地址
	mov cx, 0x40    ; Number of sectors / 扇区数量
read_disk:	
	call read_disk_one_sector
	inc eax
	add bx, 512
	loop read_disk

	call mem_detect  ; Detect memory / 检测内存
		
	; Enable A20 / 启用 A20
	in al,0x92
	or al,0x02
	out 0x92,al

    cli
    
    lgdt [gdt_descriptor]  ; Load GDT / 加载 GDT
    
	; Enable protected mode / 启用保护模式
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax

    ; Far jump to 32-bit code, flush pipeline / 远跳转到32位代码，清空流水线
    jmp 0x08:protected_mode_entry


; GDT definition / GDT 定义
gdt_start:
    dq 0x0
gdt_code:
    dw 0xFFFF
    dw 0x0
    db 0x0
    db 10011011b
    db 11001111b
    db 0x0
gdt_data:
    dw 0xFFFF
    dw 0x0
    db 0x0
    db 10010011b
    db 11001111b
    db 0x0
gdt_end:
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start



; Read one sector from disk / 从磁盘读取一个扇区
; Only works for one sector at a time / 每次只读取一个扇区
read_disk_one_sector:						; EAX: start sector / EAX: 起始扇区								
	pushad
	mov esi, eax				; CX: number of sectors / CX: 扇区数量
	mov di, cx					; BX: load address / BX: 加载地址

	; Set up disk registers / 设置磁盘寄存器
	mov dx, 0x01f2
	mov al, cl
	out dx, al

	mov eax, esi

	inc dx
	out dx, al

	shr eax, 8
	inc dx
	out dx, al

	shr eax, 8
	inc dx
	out dx, al
	
	shr eax, 8
	and al, 0x0f

	or al, 0xe0
	inc dx
	out dx, al

	inc dx
	mov al, 0x20
	out dx, al

.not_ready:
	nop
	nop
	nop
	in al, dx
	
	and al, 0x88
	cmp al, 0x08
	jnz .not_ready

ready:
	mov cx, 256
	mov dx, 0x01f0
.continue_read:
	in ax, dx
	mov [bx], ax
	add bx, 2
	loop .continue_read

	popad
	ret


; ----------------------------------------------------------------------
; Memory detection function (using BIOS int 15h, EAX=0xE820)
; 内存检测函数 (使用 BIOS int 15h, EAX=0xE820)
; Results stored at:
;   [0x1000]: Number of ARDS structures (4 bytes)
;   [0x1004]: Buffer start address
; ----------------------------------------------------------------------
mem_detect:
    ; Initialize counter, write directly to physical memory 0x1000 / 初始化计数器，直接写入物理内存 0x1000
    xor eax, eax
    mov [0x1000], eax

    mov ebx, 0
    mov di, 0x1004          ; Buffer start address / 缓冲区起始地址
.loop:
    mov eax, 0xe820
    mov ecx, 20             ; ARDS structure size / ARDS 结构大小
    mov edx, 0x534D4150     ; 'SMAP'
    int 0x15
    jc .fail                ; CF=1 indicates error / CF=1 表示出错

    add di, 20              ; Point to next structure / 指向下一个结构体
    inc dword [0x1000]      ; Counter +1 / 计数器 +1

    cmp ebx, 0              ; EBX=0 means detection finished / EBX=0 表示检测结束
    jne .loop
    
    jmp .ok                 ; Detection successful, skip error handling / 检测成功，跳过错误处理

.fail:
    ; Simple error handling: infinite loop or print error message / 简单错误处理：死循环或打印错误信息
    call print_str
    hlt

.ok:
    ret


	
; Print string using BIOS int 10h, AH=0x13 / 使用 BIOS int 10h, AH=0x13 打印字符串
;	AH=0x13 display character interrupt, AL=display mode / AH=0x13 显示字符中断，AL=显示输出方式
;	BH=page number, BL=attribute / BH=页码，BL=属性
;	ES:BP=string address / ES:BP=字符串地址
;	CX=string length / CX=字符串长度
;	DH,DL=row,col / DH,DL=行列

print_str:
	mov ax, mem_error_msg
	mov bp, ax
	mov ax, 0x1301
	mov bx, 0x0007
	mov cx, 22
	mov dx, 0x0001
	int 0x10


mem_error_msg: db 'memory detect error!', 0


[bits 32]  ; Switch to 32-bit protected mode / 切换到32位保护模式
protected_mode_entry:
	cli
    ; 设置数据段寄存器
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ; 设置堆栈指针
    mov esp, 0x7000
	
	jmp 0x0008:0x8000


times 510-($-$$) db 0
dw 0xaa55
