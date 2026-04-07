/*
 * === EvanderOS - Global Descriptor Table (gdt.c) ===
 * EvanderOS - 全局描述符表 (gdt.c)
 *
 * Purpose/目的:
 *   - Initialize GDT for x86 segmentation / x86分段描述符的初始化
 *   - Define memory segments for kernel and user code/data (Ring 0 & Ring 3)
 *     批示定义内核和用户的代码/数据描述符 (Ring 0 & Ring 3)
 *   - Set up Task State Segment (TSS) for task switching / 为TSS设置，支持任务切换
 *   - Provide protection rings and privilege levels / 提供污会管理和权限优先圳
 *
 * GDT Structure/GDT结构:
 *   [0] Null (required by x86) - 空条目 (必须)
 *   [1] Kernel Code (Ring 0, 0x08) - 内核代码
 *   [2] Kernel Data (Ring 0, 0x10) - 内核数据
 *   [3] User Code (Ring 3, 0x1B) - 用户代码
 *   [4] User Data (Ring 3, 0x23) - 用户数据
 *   [5] TSS (Ring 0, 0x28) - 任务便段
 */
// gdt.c
#include "include.h"
#include "stdio.h"

// GDT table size: Null + Kernel Code + Kernel Data + User Code + User Data + TSS
// GDT表的整个大小: Null + 内核代码 + 内核数据 + 用户代码 + 用户数据 + TSS
#define GDT_SIZE 6 

struct gdt_entry gdt[GDT_SIZE];     // GDT table: holds 6 segment descriptors
                                    // GDT表: 存放6个SEG描述符
struct gdt_ptr gp;                 // GDT pointer: limit + base address (for lgdt instruction)
                                    // GDT指针: 上限 + 基址 (lgdt指令用)
extern struct tss_entry tss;        // TSS structure (defined in task.c)
                                    // TSS结构 (task.c定义)

// === Assembly functions: Reload GDT and load TSS ===
// 汇编函数: 重新加载 GDT 和加载 TSS
extern void gdt_flush(unsigned int);  // Reload GDT (in func.s)
                                       // 重新加载 GDT (func.s)
extern void tss_flush();               // Load TSS into TR register (in func.s)
                                       // 加载 TSS 到 TR 寄存器 (func.s)

/*
 * === gdt_set_gate() - Set up a single GDT descriptor ===
 * 设置一个污类描述符
 *
 * Parameters/参数:
 *   num: Index in GDT table (0-5) / GDT中的下标
 *   base: Base address of segment / 污的基址
 *   limit: Segment size limit (0xFFFFFFFF for 4GB) / 污的大小上限
 *   access: Access flags (DPL, Type, etc.) / 访问标志
 *   gran: Granularity flags (0xCF = 1-page granularity) / 粗度描述（页级）
 *
 * GDT Entry Format/GDT条目格式:
 *   [0:15]    Base Low (bits 0-15 of base address) / 基址低16位
 *   [16:23]   Base Middle (bits 16-23) / 基址中16位
 *   [24:31]   Base High (bits 24-31) / 基址高运位
 *   [0:15]    Limit Low (bits 0-15 of size) / 上限低16位
 *   [16:19]   Limit High & Granularity (bits 16-19) / 上限高位及粗度
 */
// Helper function: Set up a single GDT descriptor
// 辅助函数: 设置单个污描述符
void gdt_set_gate(int num, unsigned long base, unsigned long limit, unsigned char access, unsigned char gran) {
    // Extract and set base address fields (spread across 3 fields in descriptor)
    // 提取基址并设置 (分为3个字段)
    gdt[num].base_low = (base & 0xFFFF);                    // Bits 0-15 of base
    gdt[num].base_middle = (base >> 16) & 0xFF;             // Bits 16-23 of base
    gdt[num].base_high = (base >> 24) & 0xFF;               // Bits 24-31 of base

    // Extract and set limit fields (spread across 2 fields)
    // 提取遣限并设置 (分为2个字段)
    gdt[num].limit_low = (limit & 0xFFFF);                  // Bits 0-15 of limit
    gdt[num].granularity = ((limit >> 16) & 0x0F);          // Bits 16-19 of limit

    // Set granularity effects (0xCF = D bit 1MB granularity + reserved bits)
    // 设置粗度效果 (0xCF = D位 1MB粗度)
    gdt[num].granularity |= (gran & 0xF0);
    
    // Access byte: DPL, Type, Present bit, etc.
    // 访问字节: DPL, 类型, 存在位, 等
    gdt[num].access = access;
}

/*
 * === init_gdt() - Initialize Global Descriptor Table ===
 * 初始化全局描述符表
 *
 * Steps/步骤:
 *   1. Set GDT base address and limit in GDT pointer struct
 *   2. Create segment descriptors (Null, Kernel Code/Data, User Code/Data, TSS)
 *   3. Flush GDT and TSS to hardware (lgdt, ltr instructions)
 *   4. Initialize TSS with kernel stack pointer
 *
 * Ring Privilege Levels/优先权限等级:
 *   Ring 0: Kernel mode (highest privilege) / 内核模式 (最高权限)
 *   Ring 3: User mode (lowest privilege) / 用户模式 (最低权限)
 */
// Main GDT initialization function
// 主要GDT初始化函数
void init_gdt() {
    // === 1. Set GDT pointer (base address and size limit) ===
    // 指针的场段：可以手动设置前置種子和上限
    gp.limit = (sizeof(struct gdt_entry) * GDT_SIZE) - 1;   // GDT size - 1 (in bytes)
    gp.base = (unsigned int)&gdt;                           // Address of GDT table

    // === 2. Create GDT Entries ===
    // 批示定义各个描述符
    
    // Entry 0: Null descriptor (mandatory by x86 spec)
    // 条目0: 空描述符 (x86规上必须)
    gdt_set_gate(0, 0, 0, 0, 0);

    // Entry 1: Kernel Code (Ring 0) - Selector 0x08
    // 条目1: 内核代码, DPL=0 (Ring 0), 0x9A=Executable+Readable
    // 0x9A = 10011010b: Present(1) DPL=0(01) Type=ExecutableReadable(1010)
    // 0xCF = 1100 1111b: Granularity=1 (4KB), D/B=1 (32-bit), reserved bits
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    // Entry 2: Kernel Data (Ring 0) - Selector 0x10
    // 条目2: 内核数据, DPL=0 (Ring 0), 0x92=Writable
    // 0x92 = 10010010b: Present(1) DPL=0(01) Type=WriteableData(0010)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // Entry 3: User Code (Ring 3) - Selector 0x1B (Index 3*8+3 RPL)
    // 条目3: 用户代码, DPL=3 (Ring 3), 0xFA=Executable+Readable
    // 0xFA = 11111010b: Present(1) DPL=3(11) Type=ExecutableReadable(1010)
    // RPL (Requested Privilege Level) = 3 (user)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    // Entry 4: User Data (Ring 3) - Selector 0x23 (Index 4*8+3 RPL)
    // 条目4: 用户数据, DPL=3 (Ring 3), 0xF2=Writable
    // 0xF2 = 11110010b: Present(1) DPL=3(11) Type=WriteableData(0010)
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    // Entry 5: TSS (Task State Segment) - Selector 0x28 (Index 5*8+0)
    // 条目5: 任务便段, DPL=0, 0x89=Available 32-bit TSS
    // 0x89 = 10001001b: Present(1) DPL=0(00) Type=Available TSS(1001)
    // Base=&tss, Limit=sizeof(tss), Granularity=0 (byte unit)
    gdt_set_gate(5, (unsigned long)&tss, sizeof(tss), 0x89, 0x00);

    // === 3. Load GDT and TSS ===
    // 加载 GDT 和 TSS 到硬件
    gdt_flush((unsigned int)&gp);     // Execute lgdt instruction (load GDT)
                                       // 执行 lgdt 指令 (存入 GDT)
    
    // === 4. Initialize TSS structure ===
    // 初始化 TSS 结构: 内核栈指针等
    // TSS holds kernel stack pointer (esp0) for privilege level transitions
    // TSS帮会偏存内核栈指针,供优先级切换时使用
    memset(&tss, 0, sizeof(tss));      // Clear TSS to zeros
    tss.ss0 = 0x10;                   // ss0: Kernel Data Segment Selector (0x10)
                                       // ss0: 内核数据段选择子
    tss.esp0 = 0;                     // esp0: Will be updated on each task switch
                                       // esp0: 每次任务切换时更新
    
    // === Load TSS into TR (Task Register) ===
    // 将 TSS 加载到 TR 寄存器 (ltr 指令)
    // Selector 0x28 = Index 5 * 8 (TSS descriptor), RPL=0 (kernel)
    tss_flush();                       // Execute ltr instruction (load TSS)
                                       // 执行 ltr 指令 (存入 TSS)
    
    printk("gdt initialized.\n");        
}

