/*
 * === EvanderOS - Task/Process Management (task.c) ===\n * EvanderOS - 任务/进程管理 (task.c)
 *
 * Purpose/目的:
 *   - Implement process scheduling and context switching
 *     实现进程调度和上下文切换
 *   - Manage task creation, execution, and termination
 *     管理任务创建、执行和终止
 *   - Provide idle process (PID 0) as CPU fallback
 *     提供空闲进程 (PID 0) 作为 CPU 备用
 *   - Implement process queuing with circular linked list
 *     实现进程队列 (循环链表)
 *   - Support task state management (RUNNING, READY, BLOCKED, DIED)
 *     支持任务状态管理 (RUNNING, READY, BLOCKED, DIED)
 *
 * Process Scheduling/进程调度:
 *   - Round-robin time-sharing (10 jiffies per slice)
 *     时间片轮转 (每10 jiffies一次时间片)
 *   - Task ready queue: Circular linked list of READY/RUNNING tasks
 *     任务就绪队列: READY/RUNNING 任务的循环链表
 *   - Idle task fallback: If all tasks blocked, run idle
 *     空闲任务备用: 如果所有任务都被阻塞,运行空闲任务
 */
#include "include.h"
#include "stdio.h"

extern struct task_struct *current_task; // Current running task / 当前运行的任务
extern void start_process(void);         // User process entry (asm) / 用户进程入口 (汇编)
struct task_struct *idle_task;           // Idle task (PID 0) / 空闲任务 (PID 0)

// === Global Process Management ===
// 全局进程管理 - Global PID counter: Increments for each new process
// 全局 PID 计数器 - Increments for each new process
static int pid_counter = 0;
// 全局 tss - Global TSS (Task State Segment for hardware task switching)
// 全局 tss
struct tss_entry tss;
// 全局变量：指向当前就绪队列的头（或者当前正在运行的任务）
// Ready queue head: Points to first task in circular linked list
struct task_struct *ready_queue_head = 0;

void init_idle_process() {
    // === Create idle task: kernel process with PID 0 ===
    // 创建空闲任务: PID 0 的内核进程
    // Purpose: Fallback task when all other processes blocked
    // 目的: 当所有其他进程阻塞时的备用任务
    
    unsigned int page = (unsigned int)vmalloc(1);
    idle_task = (struct task_struct*)page;
    memset(idle_task, 0, PAGE_SIZE);
    idle_task->pid = 0;                      // PID 0 reserved for idle / PID 0 保留给空闲
    idle_task->state = TASK_RUNNING;         // Idle runs immediately / 空闲立即运行
    idle_task->priority = 0;                 // Lowest priority / 最低优先级
    idle_task->ppid = -1;                    // No parent / 没有父进程
    idle_task->pde = get_cr3();              // Use kernel page directory / 使用内核页目录
    idle_task->user_vaddr_start = 0;         // No user space / 没有用户空间
    idle_task->user_pages_count = 0;
    idle_task->self_kstack = (unsigned int*)((unsigned int)idle_task + PAGE_SIZE);
    strcpy(idle_task->name, "Idle");
    current_task = idle_task;
    idle_task->next = idle_task;             // Circular linked list / 闭环链表
    ready_queue_head = idle_task;
}

// 供调度器调用：更新 TSS 中的内核栈指针
void update_tss_esp0(unsigned int esp) {
    tss.esp0 = esp;
}


// 辅助函数：将新任务加入就绪队列（尾插法，保持循环）
void list_append(struct task_struct* task) {
    if (ready_queue_head == 0) {
        // 队列为空，自己指向自己，形成闭环
        ready_queue_head = task;
        task->next = task;
    } else {
        // 寻找尾节点（即 next 指向 head 的那个节点）
        struct task_struct* tail = ready_queue_head;
        while (tail->next != ready_queue_head) {
            tail = tail->next;
        }
        // 插入到尾部
        tail->next = task;
        task->next = ready_queue_head;
    }
}

// 根据 PID 查找任务
struct task_struct* find_task_by_pid(int pid) {
    if (!ready_queue_head) return 0;
    struct task_struct* cur = ready_queue_head;
    do {
        if (cur->pid == pid) return cur;
        cur = cur->next;
    } while (cur != ready_queue_head);
    return 0;
}

// 从循环链表中移除一个任务 (用于彻底销毁僵尸进程)
void list_remove(struct task_struct* task) {
    if (!ready_queue_head || !task) return;
    
    // 如果链表里只有它自己
    if (task->next == task) {
        ready_queue_head = 0;
        return;
    }
   // 寻找它的前一个节点
    struct task_struct* prev = ready_queue_head;
    while (prev->next != task) {
        prev = prev->next;
    }
    // 摘除节点
    prev->next = task->next;
    if (ready_queue_head == task) {
        ready_queue_head = task->next;
    }
}

void schedule() {
    // === Context Switch: Round-robin multi-tasking ===
    // 上下文切换: 时间片轮转多任务处理
    // Called every 10 jiffies (100ms) by timer interrupt
    // 由计时器中断每 10 个 jiffies (100ms) 调用一次
    
    __asm__ __volatile__("cli");  // Disable interrupts (atomic) / 禁用中断 (原子操作)

    if (!ready_queue_head) {
        __asm__ __volatile__("sti");
        return;  // No tasks to schedule / 没有任务可调度
    }

    struct task_struct *cur = current_task;  // Current running task / 当前任务
    
    // Only demote RUNNING to READY; preserve BLOCKED/DIED states
    // 只将 RUNNING 下降为 READY; 保留 BLOCKED/DIED 的状态
    // This ensures blocked processes remain suspended for I/O
    if (cur->state == TASK_RUNNING) {
        cur->state = TASK_READY;  // Time slice expired / 时间片过期
    }

    struct task_struct *next = cur->next;  // Next task in queue / 队列中的下一个任务

    // 核心修改：寻找下一个真正可以运行的任务 (跳过阻塞和死亡的进程)
    while (next->state != TASK_READY && next->state != TASK_RUNNING) {
        next = next->next;
        // 如果找了一整圈又回到了 cur，说明所有其他任务都挂起了
        if (next == cur) {
            break; 
        }
    }
    // 如果转了一圈发现连自己也不能运行了 (比如自己刚调用了 exit 变成 DIED)
    // 那就强行让 idle 任务来接管 CPU
    if (next->state != TASK_READY && next->state != TASK_RUNNING) {
        next = idle_task;
    }
    // 跳过无意义的 idle 切换 (保持你原来的逻辑)
    if (next->pid == 0 && next->next != next && next->next->state == TASK_READY) {
        next = next->next;
    }
    if (cur == next) {
        if (cur->state == TASK_READY) cur->state = TASK_RUNNING;
        __asm__ __volatile__("sti");
        return;
    }
    next->state = TASK_RUNNING;
    current_task = next;
    update_tss_esp0((unsigned int)next + PAGE_SIZE);
    if (get_cr3() != next->pde) {
        set_cr3(next->pde);
    }
    
    switch_to(cur, next);
    //__asm__ __volatile__("sti");
}

// ----------------------------------------------------------------
/* 2. 创建用户进程
高地址
+----------------------+  <-- (void*)task + PAGE_SIZE
| struct registers     |
|   ss, user_esp       |
|   eflags, cs, eip    |
|   err_code, int_num  |
|   eax...edi          |
|   ds, es, fs, gs     |
+----------------------+  <-- regs 指针位置 (start_process 本该从这里开始 pop)
| struct thread_stack  |
|   function_arg       |  <-- 4字节 (process_execute中未赋值，或者是0)
|   unused_ret_addr    |  <-- 4字节 (switch_to 的 ret 返回后，ESP 指向这里)
|   eip (start_process)|  <-- switch_to 的 ret 弹出了这个值
|   esi, edi, ebx, ebp |  <-- switch_to 弹出了这些
+----------------------+  <-- task->self_kstack (switch_to 开始时的 ESP)
低地址*/

int process_execute(void* code, unsigned int size, char* arg) {
    void* pcb_page = (void*)vmalloc(1); 
    if (!pcb_page) return -1;
    
    struct task_struct* task = (struct task_struct*)pcb_page;
    memset(task, 0, PAGE_SIZE);
    task->pid = ++pid_counter; 
    task->ppid = (current_task) ? current_task->pid : -1;
    task->state = TASK_READY; 
    task->priority = 10;
    strcpy(task->name, "UserProg");

    task->pde = create_kernel_pde(); 
    if (!task->pde) { vfree(pcb_page, 1); return -1; } 

    // 【修改点 1】：专门给用户栈多分配 1 页空间！
    unsigned int code_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE + 8;
    if (code_pages == 0) code_pages = 1;
    unsigned int user_pages = code_pages + 1; // 最后一页专门作为用户栈！

    if (user_pages > 256) {
        printk("PANIC: File too large, user_pages > 256!\n");
        return -1;
    }
    
    task->user_vaddr_start = USER_PROG_ENTRY;
    task->user_pages_count = user_pages;

    // 堆的初始边界，就紧挨着用户栈的顶部！
    task->user_brk = USER_PROG_ENTRY + user_pages * PAGE_SIZE;

    unsigned int old_cr3 = get_cr3();
    void* allocated_phy_pages[256]; 
    int success_count = 0;
    int alloc_failed = 0;

    unsigned int arg_vaddr_in_child = 0; // 记录参数在子进程中的虚拟地址

    for (int i = 0; i < user_pages; i++) {
        void* paddr = alloc_page();
        if (!paddr) { alloc_failed = 1; break; }
        
        allocated_phy_pages[i] = paddr;
        success_count++;

        map_page(TEMP_PAGE_HELPER, (unsigned int)paddr, PTE_P | PTE_W);
        memset((void*)TEMP_PAGE_HELPER, 0, PAGE_SIZE);

        // 【修改点 2】：区分代码页和栈页
        if (i < code_pages) {
            // 拷贝代码
            unsigned int chunk_size = (size > PAGE_SIZE) ? PAGE_SIZE : size;
            if (size > 0) {
                memcpy((void*)TEMP_PAGE_HELPER, code + i * PAGE_SIZE, chunk_size);
                size -= chunk_size;
            }
        } else {
            // 这是最后一页（用户栈页）！执行“走私魔法”
            if (arg != 0 && arg[0] != '\0') {
                int arg_len = strlen(arg) + 1; // 包含末尾的 '\0'
                unsigned int offset = PAGE_SIZE - arg_len; // 放在栈底最深处
                
                // 拷贝字符串到物理内存中
                memcpy((void*)(TEMP_PAGE_HELPER + offset), arg, arg_len);
                
                // 计算出这个字符串在子进程运行时的虚拟地址
                arg_vaddr_in_child = USER_PROG_ENTRY + i * PAGE_SIZE + offset;
            }
        }
        __asm__ __volatile__("cli");
        set_cr3(task->pde);

        map_page(task->user_vaddr_start + i * PAGE_SIZE, (unsigned int)paddr, PTE_P | PTE_W | PTE_U);
        set_cr3(old_cr3);
        
        __asm__ __volatile__("sti");
    }
    
    if (alloc_failed) {
        for (int j = 0; j < success_count; j++) free_page(allocated_phy_pages[j]);
        free_pde_and_tables((unsigned int)task->pde);
        vfree(pcb_page, 1);
        return -1;
    }
    
    unsigned int stack_top = (unsigned int)task + PAGE_SIZE;
    stack_top -= sizeof(struct registers);
    struct registers* regs = (struct registers*)stack_top;

    regs->cs = 0x1b; 
    regs->ds = regs->es = regs->fs = regs->gs = 0x23; 
    regs->ss = 0x23; 
    regs->eflags = 0x202; 
    regs->eip = USER_PROG_ENTRY;    
    regs->eax = regs->ecx = regs->edx = 0;
    regs->esi = regs->edi = regs->ebp = 0;
    
    // 【修改点 3】：把参数地址塞给子进程的 EBX
    if (arg_vaddr_in_child != 0) {
        regs->ebx = arg_vaddr_in_child; // EBX 装载参数地址！
        //regs->user_esp = arg_vaddr_in_child; // 用户栈顶指针刚好压在字符串上面
        
        // 往下退 16 字节，并强制 16 字节对齐，防止越界读取参数字符串
        regs->user_esp = (arg_vaddr_in_child - 16) & ~0xF;
    } else {
        regs->ebx = 0;
        //regs->user_esp = USER_PROG_ENTRY + user_pages * PAGE_SIZE; 
        regs->user_esp = (USER_PROG_ENTRY + user_pages * PAGE_SIZE - 16) & ~0xF;
    }
    
    stack_top -= sizeof(struct thread_stack);
    struct thread_stack* kstack = (struct thread_stack*)stack_top;
    kstack->eip = (void*)start_process; 
    kstack->ebp = kstack->ebx = kstack->edi = kstack->esi = 0;

    task->self_kstack = (unsigned int*)stack_top;
    task->state = TASK_READY;
    list_append(task); 
    return task->pid;
}

void sys_exit(int status) {
    struct task_struct* cur = current_task;
    
    // 1. 标记为死亡并保存退出码
    cur->state = TASK_DIED;
    cur->status = status;

    // 2. 释放物理内存 (使用你 mem.c 里写好的牛逼函数)
    free_pde_and_tables(cur->pde);

    // 3. 唤醒所有正在等待我的进程
    // (因为没有父子关系了，我们就遍历所有人，谁在等我，我就唤醒谁)
    struct task_struct* temp = ready_queue_head;
    if (temp) {
        do {
            if (temp->state == TASK_BLOCKED && temp->waitpid == cur->pid) {
                temp->state = TASK_READY;
            }
            temp = temp->next;
        } while (temp != ready_queue_head);
    }

    // 4. 彻底让出 CPU
    schedule(); 
}

int sys_waitpid(int pid) {
    struct task_struct* cur = current_task;
    
    while (1) {
        struct task_struct* target = find_task_by_pid(pid);
        
        // 如果找不到，说明没这个进程
        if (!target) return -1; 
        
        // 如果目标进程已经死亡，收尸并返回状态
        if (target->state == TASK_DIED) {
            int exit_status = target->status;
            list_remove(target);        // 从链表摘除
            vfree((void*)target, 1);   // 释放 PCB 和内核栈所在的 4KB
            return exit_status;
        }
        
        // 目标还活着，阻塞我自己，并登记我在等谁
        cur->state = TASK_BLOCKED;
        cur->waitpid = pid;
        schedule(); 
    }
}

// 根据 PID 强制终止一个进程
int sys_kill(int pid) {
    struct task_struct* target = find_task_by_pid(pid);
    
    // 找不到目标，或者目标已经死亡，或者企图谋杀 idle 进程 (PID 0)
    if (!target || target->state == TASK_DIED || target->pid == 0) {
        return -1; 
    }

    // 如果进程企图自杀，直接走自己退出的流程
    if (target == current_task) {
        sys_exit(-1);
        return 0; // 不会执行到这里
    }

    // --- 开始隔空处决 ---

    // 1. 标记为死亡，并设置异常退出码
    target->state = TASK_DIED;
    target->status = -1;

    // 2. 释放目标进程的物理页和页表
    // (借助 mem.c 里的函数，因为它用的是内核公共 TEMP 区，所以可以安全地跨进程释放)
    free_pde_and_tables(target->pde);

    // 3. 唤醒所有正在等待该进程死掉的人 (比如 Shell 调用了 waitpid)
    struct task_struct* temp = ready_queue_head;
    if (temp) {
        do {
            if (temp->state == TASK_BLOCKED && temp->waitpid == target->pid) {
                temp->state = TASK_READY;
            }
            temp = temp->next;
        } while (temp != ready_queue_head);
    }

    return 0; // 成功击杀
}
