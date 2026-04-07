#include "include.h"
#include "stdio.h"

extern struct task_struct *ready_queue_head;

struct task_struct *current_task = 0; 

int main()
{
	clear_screen(0x07);
	enable_cursor(0, 15);

	interrupt_init();

	print_memory_map();
	init_memory();
	init_paging();
    init_kheap();

    init_gdt();

    init_idle_process();

    fat32_init(0); 
    
    int shell_pid = sys_spawn("/SHELL.BIN", 0); 
    
    if (shell_pid > 0) {
        printk("Kernel spawned shell.bin with PID: %d\n", shell_pid);
    } else {
        printk("Failed to start shell! Error code: %d\n", shell_pid);
    }

    // 开启中断，调度开始
    printk("\nStarting OS...");
    __asm__ __volatile__("sti");

    // Idle 进程在此空转
    while(1) {
        __asm__ __volatile__("hlt");
    }
	return 0;
}


