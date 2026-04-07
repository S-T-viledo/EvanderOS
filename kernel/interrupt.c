/*
 * === EvanderOS - Interrupt Management (interrupt.c) ===
 *
 * Purpose/目的:
 *   - Initialize IDT (Interrupt Descriptor Table)
 *     初始化 IDT (中断描述符表)
 *   - Set up PIC (Programmable Interrupt Controller)
 *     设置 PIC (可编程中断控制器)
 *   - Handle CPU exceptions (divide by zero, page fault, etc.)
 *     处理 CPU 异常 (除以零、页面错误等)
 *   - Process hardware interrupts (timer, keyboard)
 *     处理硬件中断 (计时器、键盘)
 *   - Route interrupts/exceptions to appropriate handlers
 *     将中断/异常路由到适当的处理程序
 *
 * Interrupt Vector Layout/中断向量布局:
 *   [0x00-0x1F]:   CPU Exceptions (fault, trap, abort)
 *                  CPU 异常 (故障、陷阱、中止)
 *   [0x20-0x27]:   Master PIC (timer, keyboard, cascade, etc.)
 *                   主 PIC (计时器、键盘、级联等)
 *   [0x28-0x2F]:   Slave PIC (mouse, FPU, etc.)
 *                  从 PIC (鼠标、FPU 等)
 *   [0x30]:        Software interrupt (syscall gate, DPL=3)
 *                  软件中断 (系统调用门, DPL=3)
 *
 * Key Handlers/关键处理程序:
 *   - 0x00-0x15: Exception handlers / 异常处理程序
 *   - 0x20:      Timer interrupt (10ms tick) / 计时器中断 (10ms 滴答)
 *   - 0x21:      Keyboard interrupt / 键盘中断
 *   - 0x30:      System call (user mode) / 系统调用 (用户模式)
 */
#include "include.h"
#include "stdio.h"

void common_interrupt_handler_c(struct registers *regs);
void default_handler(struct registers *regs);
void exception_handler(struct registers *regs);
void clock_int_handler(struct registers *regs);
void keyboard_int_handler(struct registers *regs);
extern void keyboard_handler_main();
extern void do_page_fault(unsigned int addr, unsigned int error_code);


extern unsigned int volatile jiffies; // 引用 timer.c 里的变量

extern interrupt_handler_t handler_entry_table[0x32];
isr_t interrupt_handler[0x32] = {0};

interrupt_gate idt[0x32];
idt_pointer idtptr;

static void set_int_gate_desc(struct interrupt_gate *gate, unsigned int offset, unsigned short selector, unsigned char attribute)
{
    gate->offset_low = offset & 0xffff;
    gate->offset_high = (offset >> 16) & 0xffff;
    gate->code_selector = selector;
    gate->attribute = attribute;
    gate->unused = 0;
}

void init_idt()
{
    for (int i = 0; i < 0x16; i++)
    {
        interrupt_handler[i] = exception_handler;
    }
    interrupt_handler[0x20] = clock_int_handler;
    interrupt_handler[0x21] = keyboard_int_handler;

    for (int i = 0; i < 0x30; i++)
    {
        set_int_gate_desc(&idt[i], (unsigned int)handler_entry_table[i], 0x08, (i == 0x30) ? 0b11101110 : 0b10001110);
    } // 0x30 for syscall, DPL = 3(0b11)
    // 手动覆盖 0x30 号中断，让它直接指向汇编函数
    // 0xEE = 1110 1110 (Present, DPL=3, System Gate)
    set_int_gate_desc(&idt[0x30], (unsigned int)syscall_handler_asm, 0x08, 0xEE);
    idtptr.limit = sizeof(idt) - 1;
    idtptr.base = (unsigned int)idt;
    load_idt();
}

// 初始化可编程中断控制器。
void init_pic(void) /* PIC的初始化 */
{
    io_out8(0x20, 0x11); /* 边沿触发、级联模式 */
    io_out8(0x21, 0x20); /* 设置起始中断向量号，IRQ0-7由INT20-27接收 */
    io_out8(0x21, 0x04); /* 设置IR2接从片 */
    io_out8(0x21, 0x01); /* 全嵌套、非缓冲、非自动结束中断、x86模式 */
    io_out8(0xa0, 0x11); /* 边沿触发、级联模式 */
    io_out8(0xa1, 0x28); /* 设置起始中断向量号，IRQ8-15由INT28-2f接收 */
    io_out8(0xa1, 0x02); /* 设置从片连接到主片的IR2引脚*/
    io_out8(0xa1, 0x01); /* 全嵌套、非缓冲、非自动结束中断、x86模式 */
    io_out8(0x21, 0xf8); // 允许PIC1、键盘、时钟(11111000)
    io_out8(0xa1, 0xef); // 允许鼠标中断(11101111)
    return;
}

void interrupt_init(void)
{
    __asm__ __volatile__("cli");
    init_pic();
    init_idt();
    timer_init(100);
    printk("Initialize interrupt done.\n");
}

static char *messages[] = {
    "#DE Divide Error\0",
    "#DB RESERVED\0",
    "--  NMI Interrupt\0",
    "#BP Breakpoint\0",
    "#OF Overflow\0",
    "#BR BOUND Range Exceeded\0",
    "#UD Invalid Opcode (Undefined Opcode)\0",
    "#NM Device Not Available (No Math Coprocessor)\0",
    "#DF Double Fault\0",
    "    Coprocessor Segment Overrun (reserved)\0",
    "#TS Invalid TSS\0",
    "#NP Segment Not Present\0",
    "#SS Stack-Segment Fault\0",
    "#GP General Protection\0",
    "#PF Page Fault\0",
    "--  (Intel reserved. Do not use.)\0",
    "#MF x87 FPU Floating-Point Error (Math Fault)\0",
    "#AC Alignment Check\0",
    "#MC Machine Check\0",
    "#XF SIMD Floating-Point Exception\0",
    "#VE Virtualization Exception\0",
    "#CP Control Protection Exception\0",
};

void default_handler(struct registers *regs)    {  
    (void)regs;
    printk("\nunknown interrupt\0");
}
void exception_handler(struct registers *regs)
{
    if (regs->int_num == 14)
    {
        unsigned int fault_addr = get_cr2();
        printk("\n[PANIC] PAGE FAULT at 0x%x\n", fault_addr);
        // 解析错误码 (err_code)
        // Bit 0 (P): 0 = 页面不存在, 1 = 权限保护错误
        // Bit 1 (W): 0 = 读操作, 1 = 写操作
        // Bit 2 (U): 0 = 内核态, 1 = 用户态
        if (!(regs->err_code & 1))  {printk("Reason: Page Not Present\n");}
        else                        {printk("Reason: Protection Violation\n");}
        if (regs->err_code & 2)
            printk("Action: Write\n");
        else
            printk("Action: Read\n");
        do_page_fault(fault_addr, regs->err_code);
        return;
    }
    else    {printk(messages[regs->int_num]);}

    printk("\ncpu error:%s, error code:%x", messages[regs->int_num], regs->int_num);
    while (1)   {__asm__ __volatile__("hlt");};
}
//                  always jumps here from asm when interrupt, then deals seperately
void common_interrupt_handler_c(struct registers *regs)
{
    int int_num = regs->int_num;

    // 检查是否有注册的中断处理函数
    if (interrupt_handler[int_num] != 0){interrupt_handler[int_num](regs);}
    else if (int_num < 0x16)            {exception_handler(regs);}
    else                                {default_handler(regs);}
    // 如果是硬件中断，发送EOI
    if (int_num >= 0x20 && int_num <= 0x2F && int_num != 0x27)
    {
        if (int_num >= 0x28)
        {
            io_out8(0xA0, 0x20); // 从PIC
        }
        io_out8(0x20, 0x20); // 主PIC
    }
}
void clock_int_handler(struct registers *regs)
{
    (void)regs;         // 消灭编译警告             
    // 1. 系统时钟计数 +1
    jiffies++;
     // 2. 发送中断结束信号 (EOI)
    // 注意：必须在切换任务之前发送 EOI！
    // 否则，如果切换走了很久没回来，PIC 会认为中断还没处理完，不再发送新的中断。
    io_out8(0x20, 0x20);
    // 3. 实现时间片轮转
    // 现在的任务是：current_task
    // 我们设定：每 10 个 tick (也就是 100ms) 切换一次
    if (jiffies % 10 == 0)
    {
        // printk("Tick! Switching...\n"); // 调试用，以后可以关掉
        schedule();
    }
}
void keyboard_int_handler(struct registers *regs)
{
    (void)regs;
    keyboard_handler_main();
}

