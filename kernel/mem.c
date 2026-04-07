/*
 * === EvanderOS - Memory Management (mem.c) ===
 * EvanderOS - 内存管理 (mem.c)
 *
 * Purpose/目的:
 *   - Implement physical memory allocation and deallocation
 *     实现物理内存的分配和释放
 *   - Manage page tables and virtual address mapping
 *     管理页表和虚拟地址映射
 *   - Provide heap memory management (sbrk-like interface)
 *     提供堆内存管理 (类似sbrk的接口)
 *   - Support memory copy, set, and comparison utilities
 *     支持内存复制、设置、比较等实用程序
 *
 * Key Concepts/关键概念:
 *   - Physical Page Allocator: Track usage via mem_map array (reference counting)
 *     物理页分配器: 通过mem_map数组跟踪使用情况 (引用计数)
 *   - Virtual Memory: Uses 3GB+ higher-half kernel with recursive paging
 *     虚拟内存: 使用3GB+高半区内核,支持递归分页
 *   - Kernel Heap: Managed via heap_curr_break (类似sbrk的边界)
 */
#include "include.h"
#include "stdio.h"


// 指向内存映射数组的指针
static unsigned char* mem_map = (unsigned char*)(PHY_MAP_BASE+VIRT_OFFSET);
// 记录总内存大小 (MB) - For statistics only / 仅用于统计
// static unsigned int total_mem_mb = 0;
// 记录实际可用的物理页总数 - Maximum physical pages available
// 记录实际可用的实际页总数 - 最大物理页数
static unsigned int total_phy_pages = 0;

static unsigned int heap_curr_vaddr = KERNEL_HEAP_START + 0x400000;  // Heap current virtual address
                                                                      // 堆当前虚地址
static heap_block_t *heap_head = 0; // 堆的头指针 - Head of heap block list
static unsigned int heap_curr_break = KERNEL_HEAP_START; // 当前堆的边界（类似于 brk） - Current heap break

extern struct task_struct *current_task;
// 声明在 start.s 中定义的临时页目录
extern unsigned int boot_page_directory;                             // Boot page directory (in start.s)
                                                                      // 启动页目录 (start.s定义)


/*-------------------------------------------------------------------------*/
//--------------------------------functions----------------------------------
/*-------------------------------------------------------------------------*/

/*将dst_起始的size个字节置为value*/
void memset(void *dst_, unsigned char value, unsigned int size)
{
  unsigned char *dst = (unsigned char *)dst_;
  while (size-- > 0)
    *dst++ = value;
}
/*将src_起始的size个字节复制到dst_*/
void memcpy(void *dst_, const void *src_, unsigned int size)
{
  unsigned char *dst = dst_;
  const unsigned char *src = src_;
  while (size-- > 0)
    *dst++ = *src++;
}

/*连续比较以地址a_和地址b_开头的size个字节，若相等则返回0，若a_大于b_，返回+1，否则返回-1*/
int memcmp(const void *a_, const void *b_, unsigned int size)
{
  const char *a = a_;
  const char *b = b_;
  while (size-- > 0)
  {
    if (*a != *b){return *a > *b ? 1 : -1;}
    a++;
    b++;
  }
  return 0;
}

/*将字符串从src_复制到dst_*/
char *strcpy(char *dst_, const char *src_)
{
  char *r = dst_; //用来返回目的字符串起始地址
  while ((*dst_++ = *src_++)); //字符串以\0结尾
  return r;
}

/*返回字符串长度*/
unsigned int strlen(const char *str)
{
  const char *p = str;
  while (*p++);
  return (p - str - 1); 
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static void flush_tlb(unsigned int vaddr) {
    __asm__ __volatile__("invlpg (%0)" :: "r"(vaddr) : "memory");
}
unsigned int get_cr2() {
    unsigned int val;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(val));
    return val;
}
unsigned int get_cr3() {
    unsigned int cr3;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}
// 设置 CR3 寄存器 (切换页目录，会自动刷新 TLB)
void set_cr3(unsigned int pde_phy_addr) {
    __asm__ __volatile__("mov %0, %%cr3" :: "r"(pde_phy_addr));
}
/*-------------------------------------------------------------------------*/
/*-----------------------------------ARDS----------------------------------*/
/*-------------------------------------------------------------------------*/

void print_memory_map()
{
    // 1. 读取数量
    unsigned int *count_ptr = (unsigned int*)ARDS_COUNT_ADDR;
    unsigned int count = *count_ptr;
    
    // 2. 获取结构体数组指针
    struct ards_entry *entry = (struct ards_entry*)ARDS_BUFFER_ADDR;

    printk("\nMemory Map (Count: %d):\n", count);

    // 3. 遍历并打印
    for(int i = 0; i < count; i++) {
        char* type_str;
        switch(entry[i].type) {
            case 1: type_str = "Available"; break;
            case 2: type_str = "Reserved"; break;
            case 3: type_str = "ACPI Reclaim"; break;
            case 4: type_str = "ACPI NVS"; break;
            default: type_str = "Unknown"; break;
        }

        // 打印格式：Base(Hex) - Length(Hex) Type
        // 注意：你的 printk 可能只支持 32位 %x，所以这里分高低位打印
        // 如果你的内存小于 4GB，High 部分通常是 0
        printk("Base: %x%x Len: %x%x Type: %d (%s)\n", 
               entry[i].base_high, entry[i].base_low, 
               entry[i].length_high, entry[i].length_low, 
               entry[i].type, type_str);

    }
}

/*-----------------------------------------------------------------------*/
/*---------------------------------MEMMAP--------------------------------*/
/*-----------------------------------------------------------------------*/

// 增加引用计数 (例如 fork 时子进程共享父进程内存)
void page_get(void* paddr) {
    unsigned int page_idx = (unsigned int)paddr / PAGE_SIZE;
    if (page_idx < MAX_PHY_PAGES) {
        mem_map[page_idx]++; // 引用数 +1
    }
}

// 减少引用计数 (返回 0 表示页面仍被占用，返回 1 表示页面已真正释放)
// Decrement reference count; returns 1 if page is now free, 0 if still in use
int page_put(void* paddr) {
    unsigned int page_idx = (unsigned int)paddr / PAGE_SIZE;
    if (page_idx >= MAX_PHY_PAGES) return 0;

    // 保护：减到0就不减了，防止下溢 - Protect against underflow
    if (mem_map[page_idx] > 0) {
        mem_map[page_idx]--;
    }

    // 如果引用归零，说明没人用了，这页内存真正自由了
    if (mem_map[page_idx] == 0) {
        return 1;
    }
    return 0;
}
void* alloc_page() {
    // 必须扫描整个数组范围，不能只扫描到 total_phy_pages
    // 因为内存可能有空洞（比如中间有一块是显存），total 只是总数，不是最大下标
    for (unsigned int i = 0; i < MAX_PHY_PAGES; i++) {
        if (mem_map[i] == 0) {
            mem_map[i] = 1; 
            return (void*)(i * PAGE_SIZE);
        }
    }
    // 真正的内存耗尽
    printk("alloc_page: Out of Physical Memory!\n");
    return 0; 
}


void free_page(void* paddr) {
    // 减少一个引用。如果引用为0，它自然就变成了可被 alloc_page 搜索到的空闲页
    page_put(paddr);
}
/*---------------------------------------------------------------------*/
/*--------------------------------PAGING-------------------------------*/
/*---------------------------------------------------------------------*/

extern unsigned int boot_page_directory; 

void init_paging() {
    printk("Setting up recursive paging in Higher Half...\n");

    // 获取页目录的虚拟地址 (汇编里分配的)
    unsigned int *page_directory = (unsigned int *)&boot_page_directory;

    // 获取页目录的物理地址 (因为存进页表项的必须是物理地址)
    unsigned int pde_phy = ((unsigned int)page_directory) - VIRT_OFFSET;

    // 注册递归映射 (第 1023 项)
    page_directory[1023] = pde_phy | PTE_P | PTE_W | PTE_U;

    // 刷新 TLB
    set_cr3(pde_phy);

    printk("Paging Enabled in 3GB+ Space.\n");
}
/*---------------------------------------------------------------------------*/
/*---------------------------vaddr_paddr_mapping-----------------------------*/
/*---------------------------------------------------------------------------*/

void map_page(unsigned int vaddr, unsigned int paddr, unsigned int flags) {
    // 1. 获取页目录和页表的索引
    unsigned int pd_idx = PD_INDEX(vaddr);
    unsigned int pt_idx = PT_INDEX(vaddr);

    // 2. 获取页目录的虚拟地址 (通过递归映射)
    unsigned int *pde_vaddr = GET_PDE_VADDR();

    // 3. 检查该页表是否存在 (检查 PDE 的 Present 位)
    if ((pde_vaddr[pd_idx] & PTE_P) == 0) {
        // 页表不存在！需要分配一个新的物理页作为页表
        void *new_pt_phy = alloc_page();
        if (new_pt_phy == 0) {
            printk("Panic: Out of memory for Page Table!\n");
            while(1);
        }

        // 将新页表挂载到页目录中
        // 注意：PDE 必须写入物理地址
        pde_vaddr[pd_idx] = (unsigned int)new_pt_phy | PTE_P | PTE_W | PTE_U;
        flush_tlb((unsigned int)GET_PT_VADDR(pd_idx));
        // 获取新页表的虚拟地址，并清零 (防止垃圾数据导致错误映射)
        // 通过递归映射，我们可以计算出指向这个新页表的虚拟地址
        unsigned int *pt_vaddr = GET_PT_VADDR(pd_idx);
        memset(pt_vaddr, 0, PAGE_SIZE);
    }

    // 4. 现在页表一定存在了，获取页表的虚拟地址
    unsigned int *pt_vaddr = GET_PT_VADDR(pd_idx);

    // 5. 填写 PTE (建立映射)
    pt_vaddr[pt_idx] = (paddr & 0xFFFFF000) | flags;

    // 6. 刷新 TLB，让 CPU 知道这个变化
    flush_tlb(vaddr);
    set_cr3(get_cr3());
}

// 检查页表是否为空
int is_page_table_empty(unsigned int *pt_vaddr) {
    for (int i = 0; i < 1024; i++) {
        if (pt_vaddr[i] & PTE_P) return 0;
    }
    return 1;
}

void unmap_page(unsigned int vaddr) {
    unsigned int pd_idx = PD_INDEX(vaddr);
    unsigned int pt_idx = PT_INDEX(vaddr);
    unsigned int *pde_vaddr = GET_PDE_VADDR();

    if (pde_vaddr[pd_idx] & PTE_P) {
        unsigned int *pt_vaddr = GET_PT_VADDR(pd_idx);
        pt_vaddr[pt_idx] = 0; // 清空 PTE
        

        if (is_page_table_empty(pt_vaddr)) {
        // 增加判断：如果是内核空间(>= 0xC0000000)，不要释放页表
            if (vaddr < 0xC0000000) { 
                unsigned int pt_phy_addr = pde_vaddr[pd_idx] & 0xFFFFF000;
                free_page((void*)pt_phy_addr);
                pde_vaddr[pd_idx] = 0;
                flush_tlb(vaddr); // PDE 变了，最好全刷或针对性刷新
            }
        }
        
        flush_tlb(vaddr);
        set_cr3(get_cr3());
    }
}
/*-----------------------------------------------------------------------*/
/*------------------------------vaddr_alloc------------------------------*/
/*-----------------------------------------------------------------------*/
// 分配连续的虚拟页面
// page_count: 需要几页
void* vmalloc(unsigned int page_count) {
    unsigned int vstart = heap_curr_vaddr;
    unsigned int vaddr = vstart;

    for (unsigned int i = 0; i < page_count; i++) {
        void* paddr = alloc_page();
        if (paddr == 0) {
            printk("vmalloc: Out of physical memory! Rolling back...\n");
            // 回滚：释放之前已经申请成功的 i 个页
            vfree((void*)vstart, i); 
            return 0; 
        }

        // 映射时如果分配页表失败，也要考虑回滚（这里 map_page 内部目前是 panic，未来可以改为返回错误码）
        map_page(vaddr, (unsigned int)paddr, PTE_P | PTE_W | PTE_U);
        vaddr += PAGE_SIZE;
    }

    heap_curr_vaddr += page_count * PAGE_SIZE; // 只有全部成功才移动堆指针
    return (void*)vstart;
}

// 释放内存 (简单版本：只解绑，不回收虚拟地址空间)
// vaddr: 虚拟地址
// page_count: 大小
void vfree(void* vaddr, unsigned int page_count) {
    unsigned int vptr = (unsigned int)vaddr;
    
    for (unsigned int i = 0; i < page_count; i++) {
        // 1. 获取物理地址 (为了释放位图) -> 这里需要查页表获取物理地址，稍麻烦
        // 简单起见，我们先只 unmap。
        // 正规做法是：查 PTE -> 拿到 paddr -> free_page(paddr) -> unmap_page(vaddr)
        
        unsigned int pd_idx = PD_INDEX(vptr);
        unsigned int pt_idx = PT_INDEX(vptr);
        unsigned int *pt = GET_PT_VADDR(pd_idx);
        
        if (pt[pt_idx] & PTE_P) {
            unsigned int paddr = pt[pt_idx] & 0xFFFFF000;
            free_page((void*)paddr); // 归还物理页到位图
        }

        unmap_page(vptr);
        vptr += PAGE_SIZE;
    }
}

/*---------------------------------------------------------------------*/
//--------------------------kheap & kmalloc------------------------------
/*---------------------------------------------------------------------*/

void init_kheap() {
    heap_head = 0;
    heap_curr_break = KERNEL_HEAP_START;
    printk("Kernel Heap Initialized (Doubly Linked + Safety Check).\n");
}

// 内部函数：合并当前块与下一个块
static void merge_next(heap_block_t *node) {
    if (!node || !node->next) return;
    
    // 如果下一个块也是空闲的
    if (node->next->is_free) {
        // 1. 大小相加：当前大小 + 下个头大小 + 下个数据大小
        node->size += HEAP_BLOCK_SIZE + node->next->size;
        
        // 2. 链表跨越：当前的 next 指向 下下个
        node->next = node->next->next;
        
        // 3. 反向链接：如果下下个存在，让它的 prev 指向当前
        if (node->next) {
            node->next->prev = node;
        }
    }
}

// 扩容堆
static heap_block_t* heap_grow(unsigned int size) {
    unsigned int needed_total = size + HEAP_BLOCK_SIZE;
    unsigned int pages_needed = (needed_total + PAGE_SIZE - 1) / PAGE_SIZE;
    
    unsigned int start_vaddr = heap_curr_break;

    for (unsigned int i = 0; i < pages_needed; i++) {
        void* paddr = alloc_page();
        if (!paddr) return 0;
        map_page(heap_curr_break, (unsigned int)paddr, PTE_P | PTE_W | PTE_U);
        heap_curr_break += PAGE_SIZE;
    }

    heap_block_t *new_block = (heap_block_t*)start_vaddr;
    new_block->size = (pages_needed * PAGE_SIZE) - HEAP_BLOCK_SIZE;
    new_block->magic = HEAP_MAGIC; // 设置魔数
    new_block->is_free = 1;
    new_block->next = 0;
    new_block->prev = 0; // 稍后在 kmalloc 里连接

    return new_block;
}

void* kmalloc(unsigned int size) {
    if (size == 0) return 0;
    unsigned int aligned_size = (size + 3) & ~3; // 4字节对齐

    if (!heap_head) {
        heap_head = heap_grow(aligned_size);
        if(!heap_head) return 0;
    }

    heap_block_t *curr = heap_head;
    heap_block_t *last_node = 0; // 记录最后一个节点，方便追加

    // --- 查找算法 (First Fit) ---
    while (curr) {
        // 安全检查：每次访问都检查魔数
        if (curr->magic != HEAP_MAGIC) {
            printk("Heap Corruption Detected at %x!\n", curr);
            while(1); // Panic
        }

        if (curr->is_free && curr->size >= aligned_size) {
            // --- 切割逻辑 (Split) ---
            // 只有剩余空间足够容纳一个新头+4字节数据时才切割
            if (curr->size >= aligned_size + HEAP_BLOCK_SIZE + 4) {
                heap_block_t *new_next = (heap_block_t*)((unsigned int)curr + HEAP_BLOCK_SIZE + aligned_size);
                
                // 初始化新分割出的块
                new_next->magic = HEAP_MAGIC;
                new_next->is_free = 1;
                new_next->size = curr->size - aligned_size - HEAP_BLOCK_SIZE;
                
                // 插入双向链表
                new_next->next = curr->next;
                new_next->prev = curr;
                
                if (curr->next) {
                    curr->next->prev = new_next;
                }
                curr->next = new_next;
                
                // 更新当前块大小
                curr->size = aligned_size;
            }
            
            curr->is_free = 0;
            return (void*)((unsigned int)curr + HEAP_BLOCK_SIZE);
        }
        
        last_node = curr;
        curr = curr->next;
    }

    // --- 没找到，扩容 ---
    heap_block_t *new_chunk = heap_grow(aligned_size);
    if (!new_chunk) return 0;

    // 链接到链表末尾
    if (last_node) {
        last_node->next = new_chunk;
        new_chunk->prev = last_node;
    }
    
    // 递归调用自己来分配（逻辑最简单）
    return kmalloc(size);
}

void kfree(void* ptr) {
    if (!ptr) return;

    // 1. 获取 Header
    heap_block_t *block = (heap_block_t*)((unsigned int)ptr - HEAP_BLOCK_SIZE);
    
    // 2. 安全检查：检测溢出
    if (block->magic != HEAP_MAGIC) {
        printk("Heap Corruption in kfree(%x)!\n", ptr);
        printk("Expected: %x, Found: %x\n", HEAP_MAGIC, block->magic);
        while(1); // Panic
    }

    // 3. 标记为空闲
    block->is_free = 1;

    // 4. 向后合并 (Merge Next)
    // 如果后面紧挨着一个空闲块，吃掉它
    if (block->next && block->next->is_free) {
        merge_next(block);
    }

    // 5. 向前合并 (Merge Prev) - 双向链表的威力！
    // 如果前面有一个空闲块，让前面那个块吃掉我
    if (block->prev && block->prev->is_free) {
        merge_next(block->prev);
    }
}


/*----------------------------------------------------------------------*/
//---------------------------copy page table------------------------------
/*----------------------------------------------------------------------*/


// 创建一个只包含内核映射的干净 PDE
unsigned int create_kernel_pde() {
    unsigned int pde_phy = (unsigned int)alloc_page();
    if (!pde_phy) return 0;

    map_page(TEMP_PAGE_ADDR, pde_phy, PTE_P | PTE_W);
    unsigned int* pde_vaddr = (unsigned int*)TEMP_PAGE_ADDR;

    // 【核心修复】：0~767 (0~3GB) 完全清空！绝不给用户留任何内核映射！
    memset(pde_vaddr, 0, 768 * 4);

    // 复制内核空间 (768~1023) -> 这才是内核真正的家！
    unsigned int* kernel_pde = GET_PDE_VADDR();
    for (int i = 768; i < 1024; i++) {
        pde_vaddr[i] = kernel_pde[i];
    }
    
    // 递归映射
    pde_vaddr[1023] = pde_phy | PTE_P | PTE_W | PTE_U;

    unmap_page(TEMP_PAGE_ADDR);
    return pde_phy;
}

// 释放 PDE 及其管理的所有 用户态页表
void free_pde_and_tables(unsigned int pde_phy) {
    map_page(TEMP_PAGE_ADDR, pde_phy, PTE_P | PTE_W);
    unsigned int* pde_vaddr = (unsigned int*)TEMP_PAGE_ADDR;

    // 遍历纯用户空间 (0~767)
    for (int i = 0; i < 768; i++) {
        if (pde_vaddr[i] & PTE_P) {
            unsigned int pt_phy = pde_vaddr[i] & 0xFFFFF000;
            
            map_page(TEMP_PAGE_HELPER, pt_phy, PTE_P | PTE_W);
            unsigned int* pt_vaddr = (unsigned int*)TEMP_PAGE_HELPER;
            
            for (int j = 0; j < 1024; j++) {
                if (pt_vaddr[j] & PTE_P) {
                    unsigned int frame_phy = pt_vaddr[j] & 0xFFFFF000;
                    // 【核心修复】：拆掉了原来的限制锁！
                    // 因为这里面绝对不可能出现 < PHY_MAP_BASE 的内核核心内存了！
                    free_page((void*)frame_phy); 
                }
            }
            unmap_page(TEMP_PAGE_HELPER); 
            free_page((void*)pt_phy);
        }
    }
    
    unmap_page(TEMP_PAGE_ADDR); 
    free_page((void*)pde_phy);  
}
/*-------------------------------------------------------------------------------*/
//----------------------------copy on write handler-------------------------------
/*-------------------------------------------------------------------------------*/
// 处理写时复制 (Copy On Write)
// vaddr: 导致错误的虚拟地址 (这页内存目前是只读的)
void handle_cow(unsigned int vaddr) {
    // 1. 获取页表项
    unsigned int pd_idx = PD_INDEX(vaddr);
    unsigned int pt_idx = PT_INDEX(vaddr);
    unsigned int *pt_vaddr = GET_PT_VADDR(pd_idx);
    unsigned int pte = pt_vaddr[pt_idx];

    // 2. 获取物理地址和引用计数
    unsigned int paddr = pte & 0xFFFFF000;
    unsigned int page_idx = paddr / PAGE_SIZE;

    // 3. 判断引用计数
    // 如果引用只有 1 (只有我自己用)，说明我是最后一个持有者
    if (mem_map[page_idx] == 1) {
        // 直接恢复写权限即可，不用复制
        pt_vaddr[pt_idx] |= PTE_W;
    } 
    else {
        // 引用 > 1 (有别人也在用，比如父进程) -> 必须复制！
        
        // A. 申请新物理页
        unsigned int new_paddr = (unsigned int)alloc_page();
        if (!new_paddr) {
            printk("OOM in COW!\n"); 
            while(1); // 内存耗尽，卡死 (未来应该杀进程)
        }

        // B. 复制数据
        // 源数据在哪里？就在 vaddr！因为它虽然只读，但可读。
        // 目标在哪里？new_paddr。我们需要临时映射它才能写。
        
        // 借用 TEMP_PAGE_HELPER 来映射新物理页
        map_page(TEMP_PAGE_HELPER, new_paddr, PTE_P | PTE_W);
        
        // 拷贝 4KB 数据
        // 注意：拷贝时要按页对齐，vaddr 可能指向页中间
        unsigned int src_page_start = vaddr & 0xFFFFF000;
        memcpy((void*)TEMP_PAGE_HELPER, (void*)src_page_start, PAGE_SIZE);
        
        // 解除临时映射
        unmap_page(TEMP_PAGE_HELPER);

        // C. 减少旧物理页引用
        mem_map[page_idx]--;

        // D. 更新页表指向新物理页，并赋予写权限
        // 注意保留原有的属性 (User, Present 等)，只改地址和 Write 位
        pt_vaddr[pt_idx] = new_paddr | (pte & 0xFFF) | PTE_W;
    }

    // 4. 刷新 TLB
    // 因为我们修改了 PTE 权限，必须刷新，否则 CPU 缓存里还是“只读”
    flush_tlb(vaddr);
}

// 缺页中断主处理函数
void do_page_fault(unsigned int addr, unsigned int error_code) {
    // 情况 1: 页面存在(P=1) + 写操作(W=1) -> 可能是 COW
    if ((error_code & PF_P) && (error_code & PF_W)) {
        handle_cow(addr);
        return;
    }

    // 判断是谁犯的错 (Bit 2: 1=用户态, 0=内核态)
    if (error_code & PF_U) {
        printk("\n[Segmentation Fault] Process %d accessed 0x%x illegally!\n", 
               current_task->pid, addr);
        // 优雅地处决当前进程：释放它的内存、唤醒它的父进程，并让出 CPU
        sys_exit(-1); 
        // sys_exit 内部调用了 schedule()，所以永远不会 return 回去执行触发异常的那条指令
    } 
    else {
        // 内核态发生了缺页！这是致命错误 (Kernel Panic)
        printk("\n[KERNEL PANIC] Page Fault at 0x%x\n", addr);
        printk("Error Code: 0x%x (P:%d W:%d U:%d)\n", 
               error_code, 
               error_code & PF_P ? 1 : 0,
               error_code & PF_W ? 1 : 0,
               error_code & PF_U ? 1 : 0);
        printk("Process: %d\n", current_task->pid);
        
        while(1) {
            __asm__ __volatile__("hlt");
        }
    }
    // 情况 2: 页面不存在(P=0) -> 可能是栈增长或动态分配 (暂不支持，直接报错)
    // 情况 3: 权限违规 (用户态访问内核)
    
}
/*-------------------------------------------------------------------------------*/
/*---------------------------------init_memory-----------------------------------*/
/*-------------------------------------------------------------------------------*/

void init_memory() {
    // 1. 初始化位图：默认全部标记为“占用”(100)
    for (int i = 0; i < MAX_PHY_PAGES; i++) {
        mem_map[i] = 100; 
    }
    // 2. 读取 ARDS 信息，释放可用内存
    unsigned int *count_ptr = (unsigned int*)ARDS_COUNT_ADDR;
    unsigned int count = *count_ptr;
    struct ards_entry *entry = (struct ards_entry*)ARDS_BUFFER_ADDR;

    for (unsigned int i = 0; i < count; i++) {
        if (entry[i].type == 1) { 
            unsigned int base = entry[i].base_low;
            unsigned int length = entry[i].length_low;
            
            unsigned int start_page = base / PAGE_SIZE;
            unsigned int end_page = (base + length) / PAGE_SIZE;

            if (end_page > MAX_PHY_PAGES) end_page = MAX_PHY_PAGES;

            for (unsigned int j = start_page; j < end_page; j++) {
                mem_map[j] = 0; 
                // total_phy_pages++; // 建议移到后面统计，避免逻辑混乱
            }
        }
    }
    // 3. 保留内核占用的低端内存 (0 ~ 0x20000)
    
    unsigned int kernel_pages = KERNEL_MAX_PHY_ADDR / PAGE_SIZE;
    for (unsigned int i = 0; i < kernel_pages; i++) {
        mem_map[i] = 100; 
    }
    // 4. 保留 mem_map 数组本身占用的物理内存！
    unsigned int map_start_page = PHY_MAP_BASE / PAGE_SIZE;
    // 计算占用多少个物理页（向上取整）
    unsigned int map_pages = (MAX_PHY_PAGES + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (unsigned int i = map_start_page; i < map_start_page + map_pages; i++) {
        if (i < MAX_PHY_PAGES) {
            mem_map[i] = 100;
        }
    }
    
    // 5. 重新统计一下实际可用的页数
    total_phy_pages = 0;
    for(int i=0; i<MAX_PHY_PAGES; i++) {
        if(mem_map[i] == 0) total_phy_pages++;
    }

    printk("Memory Init Done. Available Pages: %d\n", total_phy_pages);
}
