/*
 * === EvanderOS - System Call Interface (syscall.c) ===
 * EvanderOS - 系统调用接口 (syscall.c)
 *
 * Purpose/目的:
 *   - Route user-mode system calls to kernel handlers
 *     将用户模式系统调用路由到内核处理程序
 *   - Provide 24+ system calls for user processes
 *     为用户进程提供24个以上的系统调用
 *   - Implement process creation, execution, and exit
 *     实现进程创建、执行和退出
 *   - Support file system operations (ls, mkdir, touch, rm, etc.)
 *     支持文件系统操作 (ls, mkdir, touch, rm 等)
 *   - Manage memory via sbrk (heap expansion)
 *     通过 sbrk 管理内存(堆扩展)
 *
 * System Call Convention/系统调用约定:
 *   Input: EAX = syscall number, EBX/ECX/EDX = arguments
 *          输入: EAX = 系统调用号, EBX/ECX/EDX = 参数
 *   Output: Result stored in EBX (if needed)
 *          输出: 结果存储在 EBX (如需要)
 */
#include "include.h"
#include "stdio.h"

extern struct task_struct *current_task;  // Current running task / 当前运行的任务
extern unsigned int jiffies;               // System tick counter / 系统滴答计数器

/*
 * === sys_sleep() - Sleep for specified milliseconds ===
 * Suspends current task until target jiffies reached
 * 将当前任务挂起,直到达到目标滴答数
 */
void sys_sleep(unsigned int ten_ms) {
    unsigned int target_ticks = jiffies + ten_ms; // 目标时间 - Target wake time
    
    while (jiffies < target_ticks) {
        __asm__ __volatile__("sti"); // 确保中断开启 - Ensure interrupts enabled
        __asm__ __volatile__("hlt"); // CPU 挂起,直到下一个硬件中断到来才醒来!极其省电!
                                     // CPU suspended until next hardware interrupt (power efficient!)
    }
}

/*
 * === syscall_router() - Main system call dispatcher ===
 * 系统调用路由函数 - Routes syscall number to appropriate handler
 * 
 * Input via registers/通过寄存器输入:
 *   EAX: System call number (0-24+) / 系统调用号 (0-24+)
 *   EBX: First argument (path string, PID, etc.) / 第一个参数
 *   ECX: Second argument (buffer, flags, etc.) / 第二个参数
 *   EDX: Third argument (size, mode, etc.) / 第三个参数
 *
 * System Calls/系统调用列表:
 *   0: printk - Print to console / 打印到控制台
 *   1: exit - Terminate process / 终止进程
 *   2: waitpid - Wait for child / 等待子进程
 *   3: kbd_read - Read keyboard / 读键盘
 *   4: spawn - Create new process / 创建新进程
 *   5: ls - List directory / 列目录
 *   6: mkdir - Create directory / 创建目录
 *   ... (more system calls) / (更多系统调用)
 */
// 系统调用路由函数
// Extracts syscall number and arguments from registers
void syscall_router(struct registers *regs) {
    // 获取系统调用号 - Get system call number
    // Schedule: printk("\n[Syscall] PID=%d NR=%d EBX=%x\n", 
    //        current_task->pid, regs->eax, regs->ebx);
    unsigned int syscall_nr = regs->eax;     // System call number / 系统调用号
    unsigned int arg1 = regs->ebx;            // First argument / 第一个参数
    unsigned int arg2 = regs->ecx;            // Second argument / 第二个参数
    unsigned int arg3 = regs->edx;            // Third argument / 第三个参数


    switch (syscall_nr) {
        case 0:
            printk((char*)arg1);
            break;
        case 1:
            sys_exit(arg1);
            break;
        case 2:
            regs->ebx = sys_waitpid(arg1);
            break;
        case 3:
            regs->ebx = kbd_read_char();
            break;
        case 4:
            regs->ebx = sys_spawn((char*)arg1, (char*)arg2); // EAX=4, EBX=路径字符串
            break;
        case 5:
            fat32_ls((char*)arg1);
            break;
        case 6:
            regs->ebx = fat32_mkdir((char*) arg1);
            break;
        case 7:
            regs->ebx = fat32_touch((char*)arg1);
            break;
        case 8:
            regs->ebx = fat32_rm((char*)arg1);
            break;
        case 9:
            regs->ebx = fat32_rmall((char*)arg1);
            break;
        case 10:
            regs->ebx = sys_file_size((char*)arg1);
            break;
        case 11:
            regs->ebx = sys_read_file_user((char*)arg1, (char*)arg2);
            break;
        case 12:
            regs->ebx = fat32_write_file((char*)arg1, (char*)arg2, arg3);
            break;
        case 13:
            regs->ebx = sys_sbrk(arg1);
            break;
        case 14:
            if(current_task->pid == find_task_by_pid((int)arg1)->ppid){regs->ebx = sys_kill((int)arg1);}
            else{regs->ebx = -2;}
            break;      // 只有A是B的父进程才能kill， 否则返回-2；找不到目标，或者目标已经死亡，或者企图谋杀 idle 进程返回-1
        case 15:
            console_clear((unsigned char)arg1);
            break;
        case 16:
            // 注意：userlib.c 中传的是 set_cursor(x, y) 
            // x 是hang(arg1)，y 是lie(arg2)。
            console_set_cursor(arg1, arg2); 
            break;
        case 17:
            console_set_color((unsigned char)arg1);
            break;
        case 18:
            regs->ebx = sys_is_dir((char*)arg1);
            break;
        case 19:
            sys_sleep(arg1);
            break;
        case 20:
            regs->ebx = getchar_nowait();
            break;
        case 21:
            disable_cursor();
            break;
        case 22:
            console_show_cursor();
            break;
        case 23:
            regs->ebx = sys_rand(arg1);
            break;
        case 24:
            sys_print_int(arg1);
            break;
        default:
            printk("Error: Unknown Syscall %d\n", syscall_nr);
            break;
    }
}


// 参数 inc: 要增加的堆字节数
int sys_sbrk(int inc) {
    struct task_struct* cur = current_task;
    unsigned int old_brk = cur->user_brk;
    
    if (inc == 0) return old_brk; 
    
    unsigned int new_brk = old_brk + inc;
    unsigned int allocated_end = cur->user_vaddr_start + cur->user_pages_count * PAGE_SIZE;
    
    if (new_brk > allocated_end) {
        unsigned int bytes_needed = new_brk - allocated_end;
        unsigned int pages_needed = (bytes_needed + PAGE_SIZE - 1) / PAGE_SIZE; 
        
        for (unsigned int i = 0; i < pages_needed; i++) {
            void* paddr = alloc_page();
            
            // 内存耗尽时的完美回滚
            if (!paddr) {
                printk("sbrk: Out of physical memory! Rolling back %d pages...\n", i);
                // 把前面成功申请的 i 个页退回去！
                if (i > 0) {
                    vfree((void*)allocated_end, i);
                }
                return -1; // 返回申请失败
            }
            
            // PTE_P(1) | PTE_W(2) | PTE_U(4) = 7
            map_page(allocated_end + i * PAGE_SIZE, (unsigned int)paddr, 7);
        }
        cur->user_pages_count += pages_needed; 
    }
    
    cur->user_brk = new_brk;
    return old_brk; 
}

// 完美串联所有OS关键点
int sys_spawn(char* filepath, char* arg) {
    struct fat32_dir_entry entry;
    
    // 1. 查找文件并获取完整属性
    if (fat32_stat(filepath, &entry) == 0) {
        return -1; // 文件未找到，或者路径错误
    }

    // 2. 绝对不能允许执行一个文件夹！
    if (entry.attr & 0x10) {
        printk("Execution error: '%s' is a directory.\n", filepath);
        return -2; 
    }

    unsigned int file_size = entry.file_size;
    unsigned int start_cluster = (entry.cluster_high << 16) | entry.cluster_low;

    if (start_cluster == 0 || file_size == 0) {
        return -3; // 空文件
    }

    // 3. 计算需要几个 4KB 物理页，并申请临时内核内存
    unsigned int pages_needed = (file_size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (pages_needed == 0) pages_needed = 1; 
    
    void* temp_buffer = vmalloc(pages_needed);
    if (!temp_buffer) return -4; 

    // 4. 将文件完整读入临时内存
    fat32_read_file(start_cluster, temp_buffer);
    // 5. 调用 process_execute 创建进程，走私参数 arg
    int new_pid = process_execute(temp_buffer, file_size, arg);

    // 6. 释放临时内存
    vfree(temp_buffer, pages_needed);

    return new_pid;
}

// 接口 1：获取文件大小 (10号)
int sys_file_size(char* path) {
    struct fat32_dir_entry entry;
    if (fat32_stat(path, &entry) == 0) return -1;
    if (entry.attr & 0x10) return -1; // 是文件夹
    return entry.file_size;
}

// 接口 2：读取文件到用户态 (11号)
int sys_read_file_user(char* path, char* buffer) {
    struct fat32_dir_entry entry;
    if (fat32_stat(path, &entry) == 0) return -1;
    unsigned int start_cluster = (entry.cluster_high << 16) | entry.cluster_low;
    if (start_cluster >= 2 && entry.file_size > 0) {
        fat32_read_file(start_cluster, buffer);
    }
    return entry.file_size;
}

// 接口 3：判断给定路径是否为文件夹 (18号)
int sys_is_dir(char* path) {
    struct fat32_dir_entry entry;
    if (fat32_stat(path, &entry) == 0) return 0; // 根本找不到
    if (entry.attr & 0x10) return 1;             // 0x10 代表目录属性
    return 0;                                    // 存在，但是个普通文件
}


// 23 号：生成伪随机数 (简单的线性同余发生器 LCG)
unsigned int sys_rand(unsigned int max) {
    static unsigned int seed = 12345;
    // 使用硬件时钟 system_ticks 增加不可预测性
    seed = (seed * 1103515245 + 12345 + jiffies) & 0x7FFFFFFF;
    if (max == 0) return 0;
    return seed % max;
}

// 24 号：在当前光标处打印一个正整数
void sys_print_int(unsigned int num) {
    char buf[16];
    int i = 0;
    if (num == 0) { 
        printk("0"); // 利用你原有的 printk
        return; 
    }
    // 把数字从后往前拆分成字符
    while (num > 0) {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }
    // 倒序打印出来
    while (i > 0) {
        char str[2] = {buf[--i], 0};
        printk(str); 
    }
}