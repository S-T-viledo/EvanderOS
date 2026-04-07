OUTPUT_FORMAT("binary")

SECTIONS
{
    /* 设置程序的加载基址为 0x0 */
    . = 0x0;

    /* 1. 代码段 */
    .text : {
        *(.text)
    }

    /* 2. 只读数据段 (你的字符串就在这里) */
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    /* 3. 初始化数据段 */
    .data : {
        *(.data)
    }

    /* 4. 未初始化数据段 */
    .bss : {
        *(.bss)
        *(COMMON)
    }
}