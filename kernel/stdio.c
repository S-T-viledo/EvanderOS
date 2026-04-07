/*
 * === EvanderOS - Console Output (stdio.c) ===
 * EvanderOS - 控制台输出 (stdio.c)
 *
 * Purpose/目的:
 *   - Implement VGA text-mode console output (80x25)
 *     实现 VGA 文本模式控制台输出 (80x25)
 *   - Manage cursor position and color attributes
 *     管理光标位置和颜色属性
 *   - Provide formatted printing (printk) function
 *     提供格式化打印 (printk) 函数
 *   - Support system calls for screen manipulation
 *     支持系统调用以操纵屏幕
 *
 * VGA Memory Layout/VGA内存布局:
 *   - Address: 0xB8000 (80*25*2 = 4000 bytes for video buffer)
 *     地址: 0xB8000 (80*25*2 = 4000字节用于视频缓冲区)
 *   - Each character cell: 2 bytes (character + color attribute)
 *     每个字符单元: 2 字节 (字符 + 颜色属性)
 *   - Color format: [High 4 bits: background] [Low 4 bits: foreground]
 *     颜色格式: [高4位: 背景] [低4位: 前景]
 */
// kernel/stdio.c
#include <stdarg.h>
#include "include.h"
#include "stdio.h"

static void console_putc(char c);        // Print single character / 打印单个字符
static void console_puts(const char* str); // Print string / 打印字符串
static void itoa(char *buf, int base, int d); // Integer to ASCII / 整数转ASCII

// 全局画笔颜色，默认 0x07 (黑底灰白字) - Global pen color: 0x07 = black background, white foreground
// 全局画笔颜色,默认 0x07 (黑底灰白字)
static unsigned char current_color = 0x07;
// 全局光标位置记录 - Global cursor position tracking
// 全局光标位置记录
static int cursor_row = 0;
static int cursor_col = 0;

void init_console() {
    // Initialize console by getting current cursor position from hardware
    // 通过从硬件获取当前光标位置来初始化控制台
    unsigned short pos = get_cursor_position();
    cursor_row = pos / 80;   // Convert absolute position to row
    cursor_col = pos % 80;   // Convert absolute position to column
}

// ==========================================
// === Exposed APIs for User System Calls ===
// === 为用户系统调用暴露的高级 API   ===
// ==========================================

// console_set_color(): Set text color for subsequent output
// 为后续输出设置文本颜色 (高4位:背景, 低4位:前景)
void console_set_color(unsigned char color) {
    current_color = color;
}
// console_clear(): Clear screen completely + reset cursor to home position
// 完全清屏 + 将光标重置到主位置
void console_clear(unsigned char mode) {
    clear_screen(mode);
    cursor_row = 0;
    cursor_col = 0;
    update_cursor(0, 0);
}

// console_set_cursor(): Position cursor at row/col + sync with hardware
// 将光标定位到行/列 + 与硬件同步
void console_set_cursor(int row, int col) {
    cursor_row = row;
    cursor_col = col;
    update_cursor(cursor_row, cursor_col);
}

// 供内部调用的标准输出
static void console_putc(char c) {
    volatile char* video_memory = (volatile char*)VIDEO_MEMORY_ADDR; 
    
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\b') { 
        if (cursor_col > 0) {
            cursor_col--;
        } else if (cursor_row > 0) { 
            cursor_row--;
            cursor_col = 79;
        }
    } else if (c == '\r') {
        cursor_col = 0;
    } else {
        unsigned int pos = (cursor_row * 80 + cursor_col) * 2;
        video_memory[pos] = c;
        video_memory[pos + 1] = current_color;
        cursor_col++;
    }

    if (cursor_col >= 80) {
        cursor_col = 0;
        cursor_row++;
    }
    if (cursor_row >= 25) {
        roll_screen(0x07);
        cursor_row = 24;
    }
    update_cursor(cursor_row, cursor_col);
}

static void console_puts(const char* str) {
    while (*str) console_putc(*str++);
}

static void itoa(char *buf, int base, int d) {
    char *p = buf; char *p1, *p2; unsigned long ud = d; int divisor = 10;
    if (base == 10 && d < 0) { *p++ = '-'; buf++; ud = -d; } 
    else if (base == 16) divisor = 16;
    do {
        int remainder = ud % divisor;
        *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    } while (ud /= divisor);
    *p = 0;
    p1 = buf; p2 = p - 1;
    while (p1 < p2) { char tmp = *p1; *p1 = *p2; *p2 = tmp; p1++; p2--; }
}

void printk(const char* format, ...) {
    va_list args;
    va_start(args, format);
    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'c': console_putc((char)va_arg(args, int)); break;
                case 's': console_puts(va_arg(args, char*)); break;
                case 'd': { char buf[16]; itoa(buf, 10, va_arg(args, int)); console_puts(buf); break; }
                case 'x': { char buf[16]; console_puts("0x"); itoa(buf, 16, va_arg(args, int)); console_puts(buf); break; }
                case '%': console_putc('%'); break;
                default: console_putc('%'); console_putc(*format); break;
            }
        } else {
            console_putc(*format);
        }
        format++;
    }
    va_end(args);
}