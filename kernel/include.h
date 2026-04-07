#define uint8_t unsigned char
#define uint16_t unsigned short
#define uint32_t unsigned int
#define uint64_t unsigned long

#define int8_t char
#define int16_t short
#define int32_t int
#define int64_t long

/*----------------------------------------------------------------*/
/*--------------------------screen.c------------------------------*/
/*----------------------------------------------------------------*/
#define VIDEO_MEMORY_ADDR (0xb8000+0xc0000000)
void roll_screen(unsigned char mode);
void clear_screen(unsigned char mode);
void update_cursor(unsigned short row, unsigned short col);
void enable_cursor(unsigned char cursor_start, unsigned char cursor_end);
void disable_cursor();
unsigned short get_cursor_position() ;
void console_show_cursor();

/*---------------------------func.s-------------------------------*/
unsigned char io_in8(unsigned short port);
unsigned short io_in16(unsigned short port);
void io_out8(unsigned short port, unsigned char value);
void io_out16(unsigned short port, unsigned short value);

/*---------------------------timer.c------------------------------*/

#define PIT_CTRL 0x0043 // 控制端口
#define PIT_DATA 0x0040 // 数据端口 (通道0)
#define CLOCK_FREQ 1193182 // 也就是 1.19MHz，晶振频率
#define JIFFY_HZ 100 // 我们期望的频率：每秒 100 次中断 (也就是 10ms 一个时间片)

void timer_init();
/*-------------------------------------------------------------------*/
/*------------------------------disk.c-------------------------------*/
/*-------------------------------------------------------------------*/
// drive 参数：0 代表主盘(内核盘)，1 代表从盘(FAT32盘)
// sector_start: 起始扇区号(LBA)
// buffer: 数据加载到的内存地址
// sector_count: 要读取的扇区数量
void read_disk_sectors(unsigned char drive, unsigned int sector_start, void* buffer, unsigned int sector_count);
void write_disk_sectors(unsigned char drive, unsigned int sector_start, unsigned char*buffer, unsigned int sector_count);

/*-------------------------------------------------------------------*/
/*------------------------------idt.c--------------------------------*/
/*-------------------------------------------------------------------*/
typedef struct interrupt_gate
{
    //  low 32 bits
    unsigned short offset_low;             // offset填入具体中断函数的地址
    unsigned short code_selector;         // 代码段选择子
    //  high 32 bits
    unsigned char unused;
    unsigned char attribute;        // high to low: isPresent(1b), DPL(2bits), Segment(1b), TYPE(4bits)
    unsigned short offset_high;    
}interrupt_gate;

typedef struct idt_pointer
{
    unsigned short limit;
    unsigned int base;
}__attribute__((packed))idt_pointer;



/*----------------------------------------------------------------*/
/*-------------------------intterrupt.c---------------------------*/
/*----------------------------------------------------------------*/
#define KBD_BUF_SIZE 256
typedef void (*interrupt_handler_t)(void);

typedef struct registers {
    // 手动压入的寄存器（顺序与压栈顺序相反）
    unsigned int gs, fs, es, ds;
    unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax;
    
    // 中断号和错误码（在公共处理程序中压入）
    unsigned int int_num, err_code;
    
    // CPU自动压入的寄存器
    unsigned int eip, cs, eflags, user_esp, ss;
}registers;

typedef void (*isr_t)(struct registers *regs);
void timer_init();
void load_idt(void);
void init_handler_table(void);
void init_idt();
void interrupt_init(void);
/*-----------------------------------------------------------------*/
/*------------------------------gdt.c------------------------------*/
/*-----------------------------------------------------------------*/
struct gdt_entry {
    unsigned short limit_low;
    unsigned short base_low;
    unsigned char base_middle;
    unsigned char access;
    unsigned char granularity;
    unsigned char base_high;
} __attribute__((packed));

struct gdt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

void init_gdt();
void update_tss_esp0(unsigned int esp);

/*-----------------------------------------------------------------*/
//------------------------------mem.c-------------------------------
/*-----------------------------------------------------------------*/
#define VIRT_OFFSET 0xC0000000
#define PAGE_SIZE 4096
#define KERNEL_MAX_PHY_ADDR 0x400000

// ARDS 相关定义 (对应 Bootloader 中的 0x1000)
#define ARDS_COUNT_ADDR (0x1000+0xc0000000)
#define ARDS_BUFFER_ADDR (0x1004+0xc0000000)

// mem_map(4K -> 1Byte)
#define PHY_MAP_BASE 0x200000

// 最大的物理内存页数 (4GB / 4KB)
#define MAX_PHY_PAGES 1048576 
// 分页相关定义 ---
#define PAGE_DIR_ADDR (0x2000+0xc0000000)   // 页目录物理地址
#define PAGE_TABLE_ADDR (0x3000+0xc0000000) // 第一个页表的物理地址
// heap
#define KERNEL_HEAP_START 0xC0300000  //from 3GB
#define HEAP_BLOCK_SIZE sizeof(heap_block_t)
#define HEAP_MAGIC 0x12345678  // 你的内核专属防伪标识

// 页表项属性
#define PTE_P 0x1   // Present: 1=存在
#define PTE_W 0x2   // Write: 1=可读写, 0=只读
#define PTE_U 0x4   // User: 1=用户级, 0=内核级
#define PTE_PWT 0x08 // Write-Through
#define PTE_PCD 0x10 // Cache-Disable
#define PTE_A   0x20 // Accessed
#define PTE_D   0x40 // Dirty
#define PTE_PAT 0x80 // Page Attribute Table
#define PTE_G   0x100 // Global

// 页错误 (Page Fault) 错误码位掩码
#define PF_P 0x1  // Present: 1=保护违规(页面存在), 0=页面不存在
#define PF_W 0x2  // Write:   1=写操作, 0=读操作
#define PF_U 0x4  // User:    1=用户态, 0=内核态

// 递归映射后的特殊虚拟地址基址
#define PT_BASE_VADDR 0xFFC00000 

// 找一个不常用的虚拟地址作为内核临时映射区
#define TEMP_PAGE_ADDR 0xFFBFF000
#define TEMP_PAGE_HELPER 0xFFBFE000

// 获取第 table_idx 个页表的虚拟地址
// 例如：Get_PT_VAddr(0) 就能拿到管理 0-4MB 空间的那个页表的虚拟地址
#define GET_PT_VADDR(table_idx) (unsigned int*)(PT_BASE_VADDR + ((table_idx) << 12))

// 获取页目录表本身的虚拟地址
// 0xFFC00000 + 0x3FF000 = 0xFFFFF000
#define GET_PDE_VADDR() (unsigned int*)(0xFFFFF000)

// 将虚拟地址拆解为 PDE 索引和 PTE 索引的宏
#define PD_INDEX(vaddr) ((vaddr) >> 22)
#define PT_INDEX(vaddr) (((vaddr) >> 12) & 0x3FF)

struct ards_entry {
    unsigned int base_low;
    unsigned int base_high;
    unsigned int length_low;
    unsigned int length_high;
    unsigned int type;
} __attribute__((packed));

typedef struct heap_block {
    struct heap_block *prev; // 指向前一个块
    struct heap_block *next; // 指向后一个块
    unsigned int size;       // 数据区大小
    unsigned int magic;      // 只有等于 HEAP_MAGIC 才是合法的
    unsigned int is_free;    // 1=空闲, 0=占用 (用int保证对齐)
} heap_block_t;

// 函数声明
void memset(void *dst_, unsigned char value, unsigned int size);
void memcpy(void *dst_, const void *src_, unsigned int size);
int memcmp(const void *a_, const void *b_, unsigned int size);
char *strcpy(char *dst_, const char *src_);
unsigned int strlen(const char *str);
int strcmp(const char *s1, const char *s2);

void print_memory_map();
void init_memory();

void page_get(void* paddr);
int page_put(void* paddr);
void* alloc_page() ;
void free_page(void* paddr);
unsigned int get_cr2();
unsigned int get_cr3();
void set_cr3(unsigned int pde_phy_addr);

//unsigned int copy_pde();
unsigned int create_kernel_pde();
void free_pde_and_tables(unsigned int pde_phy);

void init_paging();
void map_page(unsigned int vaddr, unsigned int paddr, unsigned int flags);
void unmap_page(unsigned int vaddr);
void* vmalloc(unsigned int page_count);
void vfree(void* vaddr, unsigned int page_count);

void init_kheap();
void* kmalloc(unsigned int size);
void kfree(void* ptr);



/*--------------------------------------------------------------------*/
//-------------------------------task.c--------------------------------
/*--------------------------------------------------------------------*/

#define USER_PROG_ENTRY 0     //  进程隔离，加载便利，虚拟内存的精髓

// 定义一个简单的寄存器结构，用于操作栈
struct thread_stack {
    unsigned int ebp;
    unsigned int ebx;
    unsigned int edi;
    unsigned int esi;

    void (*eip)(void*); // 第一次执行时，EIP 指向线程函数
    void *unused_ret_addr; // 调用函数后的返回地址（通常没什么用，因为线程不该返回）
    void *function_arg; // 线程函数的参数
};

enum task_state {
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_DIED
};

struct task_struct {
    unsigned int *self_kstack; // 必须是第一个成员
    enum task_state state;
    char name[16];
    unsigned int priority;
    unsigned int pid;
    
    unsigned int ppid;
    unsigned int pde;          // CR3 物理地址
    int status;
    int waitpid;

    unsigned int user_vaddr_start; // 用户数据/代码的起始虚拟地址 (比如 0x0)
    unsigned int user_pages_count; // 占用了多少连续页

    // 记录用户态堆的最高边界 (Break)
    unsigned int user_brk;
    
    struct task_struct *next;
}__attribute__((packed));

// 任务状态段结构 (Task State Segment)
// 这是一个硬件规定的结构，不能乱改顺序
struct tss_entry {
    unsigned int prev_tss;   // 上一个 TSS 指针 (硬件切换用，我们不用)
    unsigned int esp0;       // Ring 0 栈顶指针 (最重要！！！)
    unsigned int ss0;        // Ring 0 栈段选择子 (最重要！！！)
    unsigned int esp1;
    unsigned int ss1;
    unsigned int esp2;
    unsigned int ss2;
    unsigned int cr3;
    unsigned int eip;
    unsigned int eflags;
    unsigned int eax;
    unsigned int ecx;
    unsigned int edx;
    unsigned int ebx;
    unsigned int esp;
    unsigned int ebp;
    unsigned int esi;
    unsigned int edi;
    unsigned int es;
    unsigned int cs;
    unsigned int ss;
    unsigned int ds;
    unsigned int fs;
    unsigned int gs;
    unsigned int ldt;
    unsigned short trap;
    unsigned short iomap_base;
} __attribute__((packed));


void update_tss_esp0(unsigned int esp);
void schedule();
void start_process();
void switch_to(struct task_struct* cur, struct task_struct* next); // in func.s
struct task_struct* find_task_by_pid(int pid);
void list_append(struct task_struct* task);
void list_remove(struct task_struct* task);

void init_idle_process();
int process_execute(void* code, unsigned int size, char* arg); //  create a new user task;
int sys_waitpid(int pid);
void sys_exit(int status);
int sys_kill(int pid);

/*-----------------------------------------------------------------------*/
//------------------------------syscall.c----------------------------------
/*-----------------------------------------------------------------------*/
void syscall_handler_asm(void);

void syscall_router(struct registers *regs);

int sys_file_size(char* path);
int sys_read_file_user(char* path, char* buffer);
void sys_sleep(unsigned int ms);
char kbd_read_char();   // 阻塞获得键盘输入
char getchar_nowait();  // 非阻塞获得键盘输入
int sys_spawn(char* filepath, char* arg);
void fat32_ls(char* path);
int sys_sbrk(int inc);
int sys_is_dir(char* path);
// 23 号：生成伪随机数 (简单的线性同余发生器 LCG)
unsigned int sys_rand(unsigned int max);
// 24 号：在当前光标处打印一个正整数
void sys_print_int(unsigned int num);

/*-----------------------------------------------------------------------*/
/*-------------------------------fat32.c---------------------------------*/
/*-----------------------------------------------------------------------*/
// 属性掩码
#define FAT_ATTR_READ_ONLY 0x01
#define FAT_ATTR_HIDDEN    0x02
#define FAT_ATTR_SYSTEM    0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE   0x20
#define FAT_ATTR_LONG_NAME 0x0F // 长文件名标志

// FAT32 引导扇区 (BIOS Parameter Block)
struct fat32_bpb {
    unsigned char  jmp_boot[3];          // 跳转指令
    unsigned char  oem_name[8];          // OEM 名称 (如 "MSWIN4.1")
    unsigned short bytes_per_sector;     // 每扇区字节数 (通常 512)
    unsigned char  sectors_per_cluster;  // 每簇扇区数 (如 1, 2, 4, 8)
    unsigned short reserved_sector_count;// 保留扇区数 (包含引导扇区，FAT表从这里之后开始)
    unsigned char  table_count;          // FAT 表个数 (通常为 2)
    unsigned short root_entry_count;     // FAT32 必须为 0
    unsigned short total_sectors_16;     // FAT32 必须为 0
    unsigned char  media_type;           // 媒体描述符
    unsigned short table_size_16;        // FAT32 必须为 0
    unsigned short sectors_per_track;    // 每磁道扇区数
    unsigned short head_side_count;      // 磁头数
    unsigned int   hidden_sector_count;  // 隐藏扇区数 (分区前的 LBA 数)
    unsigned int   total_sectors_32;     // 总扇区数

    // FAT32 扩展字段
    unsigned int   table_size_32;        // 每个 FAT 表占用的扇区数
    unsigned short ext_flags;            // 扩展标志
    unsigned short fat_version;          // 版本号
    unsigned int   root_cluster;         // 根目录所在的起始簇号 (通常是 2)
    unsigned short fat_info;             // FSINFO 扇区号 (通常是 1)
    unsigned short backup_boot_sector;   // 备份引导扇区的位置 (通常是 6)
    unsigned char  reserved_0[12];       // 保留
    unsigned char  drive_number;         // 驱动器号
    unsigned char  reserved_1;           // 保留 (NT 标志)
    unsigned char  boot_signature;       // 扩展引导签名 (0x29)
    unsigned int   volume_id;            // 卷序列号
    unsigned char  volume_label[11];     // 卷标 (如 "NO NAME    ")
    unsigned char  file_system_type[8];  // 文件系统类型，填 "FAT32   "
} __attribute__((packed));

// FAT32 标准 8.3 目录项 (共 32 字节)
struct fat32_dir_entry {
    unsigned char  name[11];             // 文件名 8 字节 + 扩展名 3 字节
    unsigned char  attr;                 // 属性 (目录, 只读, 隐藏等)
    unsigned char  nt_reserved;          // 保留给 Windows NT
    unsigned char  creation_time_tenths; // 创建时间 (十分之一秒)
    unsigned short creation_time;        // 创建时间
    unsigned short creation_date;        // 创建日期
    unsigned short last_access_date;     // 最后访问日期
    unsigned short cluster_high;         // 起始簇号的高 16 位
    unsigned short write_time;           // 最后修改时间
    unsigned short write_date;           // 最后修改日期
    unsigned short cluster_low;          // 起始簇号的低 16 位
    unsigned int   file_size;            // 文件大小 (字节)
} __attribute__((packed));

unsigned int cluster_to_sector(unsigned int cluster);
void make_fat_name(char* src, unsigned char* dest);
void fat32_init(unsigned int partition_lba);

void format_name(unsigned char* fat_name, char* dest);
unsigned int fat32_find_file(const char* target_name);
unsigned int get_next_cluster(unsigned int cluster);
void fat32_read_file(unsigned int start_cluster, void* buffer);

//unsigned int fat32_get_file_info(char* filepath, unsigned int* out_size);
int get_next_path_part(char **path, char *part);

int fat32_stat(char* filepath, struct fat32_dir_entry* out_entry);
unsigned int fat32_allocate_cluster();
int fat32_add_dir_entry(unsigned int parent_cluster, struct fat32_dir_entry* new_entry);
int fat32_mkdir(char* path);
int fat32_touch(char* path);
void fat32_free_chain(unsigned int start_cluster);
int fat32_rm(char* path);
void fat32_rmall_cluster(unsigned int start_cluster);
int fat32_rmall(char* path);

void fat32_set_fat_entry(unsigned int cluster, unsigned int value);
int fat32_write_file(char* path, char* buffer, unsigned int size);