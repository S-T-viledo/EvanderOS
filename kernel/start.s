;
; === EvanderOS - Kernel Bootstrap Entry Point (start.s) ===
; EvanderOS - 内核引导程序入口点 (start.s)
;
; Purpose/目的:
;   - Initialize paging and virtual memory management / 初始化分页和虚拟内存管理
;   - Set up initial page directory and page tables / 建立初始页目录和页表
;   - Perform higher-half kernel relocation / 执行高半区内核重定位
;   - Set up kernel stack and enter C language kernel / 设置内核栈并进入C语言内核
;
; Key Concepts/关键概念:
;   - Identity Mapping: Maps physical 0-4MB to virtual 0-4MB (CPU transition safety)
;     恒等映射：把物理 0-4MB 映射到虚拟 0-4MB (CPU切换时确保不迷路)
;   - Higher-Half Mapping: Maps physical 0-4MB to virtual 0xC0000000-0xC0400000 (kernel home)
;     高半映射：把物理 0-4MB 映射到虚拟 0xC0000000-0xC0400000 (内核真实家园)
;
[bits 32]
global _start
global boot_page_directory
extern main

; --- 在 BSS 段预留临时的页目录和页表 ---
section .bss
align 4096
boot_page_directory:             ; Boot page directory (4KB, 1024 entries)
    resb 4096                    ; 启动页目录 (4KB,1024项)
boot_page_table1:                ; Boot page table (4KB, 1024 entries, maps 0-4MB physical RAM)
    resb 4096                    ; 启动页表 (4KB,1024项,映射物理RAM 0-4MB)

section .text
_start:                          ; Kernel entry point / 内核入口点
    ; === Step 1: Define Physical Address Calculation Macro ===
    ; 步骤 1: 定义物理地址计算宏 (把虚拟地址减去 3GB)
    ; Virtual addresses in C code are in the 3GB+ range (0xC0000000+)
    ; But early boot must use physical addresses before paging is enabled
    ; C代码中的虚拟地址在3GB+范围内,但引导阶段必须用物理地址(分页未开启)
    %define VIRT_OFFSET 0xC0000000
    %define PHY(addr) (addr - VIRT_OFFSET)  ; Convert virtual to physical / 虚拟->物理

    ; === Step 2: Clear Page Directory ===
    ; 步骤 2: 彻底清空页目录，扫除一切垃圾数据！
    ; Clear all 1024 PDE entries (32-bit each) = 4096 bytes
    ; 清空所有 1024 个 PDE 表项(每个32位) = 4096字节
    mov edi, PHY(boot_page_directory)  ; EDI = physical address of page directory
    mov ecx, 1024                      ; ECX = number of entries to clear
    xor eax, eax                       ; EAX = 0 (clear value)
    cld                                ; Clear direction flag (EDI increments upward)
                                       ; 清除方向标志位(EDI向上递增)
    rep stosd                          ; Repeat stosd 1024 times (zero out all PDE entries)
                                       ; 重复stosd 1024次 (清零所有PDE表项)

    ; === Step 3: Initialize Boot Page Table ===
    ; 步骤 3: 初始化启动页表
    ; Maps physical 0-4MB (1024 pages) with kernel-level read/write permissions
    ; 映射物理 0-4MB (1024页),内核级读写权限
    mov edi, PHY(boot_page_table1)     ; EDI = physical address of page table
    mov eax, 0x00000003                ; EAX = 0x003: PTE_P(1) | PTE_W(2) = 3 (kernel RW)
                                       ; 属性：存在位(1) | 可写位(2) = 3 (内核级读写)
    mov ecx, 1024                      ; ECX = 1024 page table entries
.fill_table:                           ; Loop label for filling page table
    mov [edi], eax                     ; Store PTE with physical address and flags
    add eax, 4096                      ; Next physical page (+4KB) / 下一个物理页
    add edi, 4                         ; Next PTE entry (+4 bytes) / 下一个表项
    loop .fill_table                   ; Continue loop / 循环

    ; === Step 4: Set Up Dual Page Directory Mapping (Core Magic!) ===
    ; 步骤 4: 建立页目录双重映射 (核心魔法!)
    ; Strategy: Map same physical page table at TWO locations
    ;   - Low address (0x00000000): For hardware transition before paging
    ;   - High address (0xC0000000): For C kernel code running in higher-half
    ; 策略：同一物理页表映射到两个位置
    ;   - 低地址(0x00000000)：确保硬件切换时不迷路
    ;   - 高地址(0xC0000000)：C内核代码运行在高半区
    mov edi, PHY(boot_page_directory)  ; EDI = physical address of page directory
    mov eax, PHY(boot_page_table1)     ; EAX = physical address of page table
    or eax, 0x00000003                 ; Add flags: Present(1) | Writable(2) = 3
                                       ; 添加标志位：存在(1) | 可写(2) = 3

    ; === Identity Mapping: PDE[0] - Maps physical 0-4MB to virtual 0-4MB ===
    ; 恒等映射：PDE[0] - 把物理 0-4MB 映射到虚拟 0-4MB
    ; Purpose: Keep CPU running during early paging transition
    ; 目的：在分页启动期间保证CPU继续运行，不致迷路
    mov [edi + 0], eax                 ; PDE entry 0 (virtual address 0x00000000)

    ; === Higher-Half Mapping: PDE[768] - Maps physical 0-4MB to virtual 3GB+ ===
    ; 高半映射：PDE[768] - 把物理 0-4MB 映射到虚拟 3GB+
    ; Purpose: This is where the kernel truly lives in virtual memory
    ; 目的：内核在虚拟地址空间中的真实家园
    ; Formula: PDE index = 0xC0000000 / 0x400000 = 768
    ;         计算：PDE索引 = 0xC0000000 / 0x400000 = 768
    mov [edi + 768 * 4], eax           ; PDE entry 768 (virtual address 0xC0000000+)

    ; === Step 5: Load Page Directory Address into CR3 ===
    ; 步骤 5: 将页目录物理地址加载到 CR3 寄存器
    ; CR3 = Control Register 3: Hardware Page Directory Base Register
    ; CR3 is the CPU register that points to the page directory
    ; CR3 = 控制寄存器3：硬件页目录基址寄存器,CPU根据它找到页目录
    mov eax, PHY(boot_page_directory)
    mov cr3, eax                       ; CR3 now points to page directory
                                       ; CR3 现在指向页目录

    ; === Step 6: Enable Paging (Set CR0.PG Bit) ===
    ; 步骤 6: 开启分页 (设置 CR0.PG 位)
    ; CR0.PG (bit 31): Paging Enable - 0=disabled, 1=enabled
    ; After this, CPU will use paging for ALL memory addresses
    ; CR0.PG (第31位)：分页启用 - 0=禁用, 1=启用
    ; 执行后,CPU将对所有内存地址使用分页
    mov eax, cr0                       ; Read current CR0 value
    or eax, 0x80000000                ; Set bit 31 (PG flag) / 设置第31位(PG标志)
    mov cr0, eax                       ; Write back to CR0, paging now ENABLED!
                                       ; 写回CR0,分页现在启用!

    ; === Step 7: Jump to Higher-Half Virtual Address Space ===
    ; 步骤 7: 跳入高半区虚拟地址空间
    ; After paging is enabled, physical 0xC0000000 maps to our page table
    ; This jump forces EIP to be in the 3GB+ range (0xC0000000+)
    ; 分页启用后,物理0-4MB被映射到虚拟0xC0000000+
    ; 此跳转强制EIP进入3GB+范围
    lea eax, [higher_half]             ; Load address of higher_half label
    jmp eax                            ; Absolute jump! EIP now in 0xC0000000+ range
                                       ; 绝对跳转! EIP现在在0xC0000000+范围!

higher_half:
    ; === Higher-Half Kernel Code Zone ===
    ; We are now safely in the 3GB+ virtual address space!
    ; 我们现在已安全进入3GB+虚拟地址空间!
    ; From here on, all memory references use virtual addresses in 0xC0000000+
    ; 从此开始,所有内存引用都使用虚拟地址 (0xC0000000+)

    ; Optional: Can now safely remove identity mapping (commented out)
    ; 可选：现在可以安全地移除恒等映射 (如下注释)
    ; mov dword [boot_page_directory + 0], 0  ; Remove identity mapping
    ; mov eax, cr3          ; Flush TLB / 刷新TLB
    ; mov cr3, eax

    ; === Step 8: Initialize Kernel Stack ===
    ; 步骤 8: 初始化内核栈
    ; The stack is now in the 3GB+ virtual address space
    ; 栈现在在3GB+虚拟地址空间中
    ; ESP points to top of 16KB kernel stack (grows downward on x86)
    ; ESP指向16KB内核栈顶部 (x86栈向下生长)
    mov esp, stack_top                 ; Set kernel stack pointer / 设置内核栈指针

    ; === Step 9: Enter C Language Kernel ===
    ; 步骤 9: 进入C语言内核
    ; Call the C language main() function in kernel.c
    ; All initialization continues from there
    ; 调用C语言的main()函数 (kernel.c)
    ; 所有初始化从那里继续
    call main                          ; Call C kernel / 调用C内核
    
    ; === Infinite Loop: Prevent System Crash ===
    ; 无限循环：防止系统崩溃
    ; If main() ever returns (it shouldn't!), halt the CPU
    ; 如果main()返回 (不应该!),则停止CPU
    jmp $                              ; Infinite loop / 死循环防崩溃

section .bss              ; BSS segment (uninitialized data)
align 16                 ; 16-byte alignment / 16字节对齐
stack_bottom:
    resb 16384            ; 16KB kernel initial stack / 16KB 内核初始栈
                          ; Total size: 16384 bytes (0x4000 bytes)
                          ; 总大小: 16384字节 (0x4000字节)
stack_top:               ; Kernel stack top (grows downward on x86)
                         ; 内核栈顶 (x86栈向下生长)
