ENTRY(_start)

/* 定义虚拟地址偏移量 (3GB) */
offset = 0xC0000000;

/* 定义内核在物理内存中的加载地址 (1MB) */
phys = 0x00008000; 

/* 定义内核的虚拟起始地址 */
virt = phys + offset;

SECTIONS
{
    /* . = 代表当前的虚拟地址 (VMA) */
    . = virt;

    /* 代码段：物理地址 = 虚拟地址 - 3GB */
    .text ALIGN(4096) : AT(ADDR(.text) - offset) {
        *(.multiboot) 
        *(.text)
        *(.rodata*)   /* 别漏了只读数据段，比如字符串常量！ */
    }

    /* 数据段：同样按页对齐，物理地址绝对不会和上面冲突 */
    .data ALIGN(4096) : AT(ADDR(.data) - offset) {
        *(.data)
    }

    /* BSS段：存放未初始化的全局变量 */
    .bss ALIGN(4096) : AT(ADDR(.bss) - offset) {
        *(.bss)
        *(COMMON)
    }
}