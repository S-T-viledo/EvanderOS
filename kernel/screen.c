// kernel/screen.c - VGA screen management / VGA 屏幕管理
#include "include.h"

// Roll screen up by one line, fill last line with mode / 屏幕向上滚动一行，最后一行填充指定模式
// Parameters: mode = color/attribute value / 参数：mode = 颜色/属性值
void roll_screen(unsigned char mode) {
    volatile char* video_memory = (volatile char*)VIDEO_MEMORY_ADDR;
    // Copy lines 1-24 to lines 0-23 / 把第 1-24 行复制到第 0-23 行
    for (int i = 0; i < 80 * 24; i++) {
        video_memory[i * 2] = video_memory[(i + 80) * 2];
        video_memory[i * 2 + 1] = video_memory[(i + 80) * 2 + 1];
    }
    // Fill last line with spaces using specified mode / 用空格填充最后一行，使用指定的模式
    for (int i = 0; i < 80; i++) {
        video_memory[(80 * 24 + i) * 2] = ' ';
        video_memory[(80 * 24 + i) * 2 + 1] = mode;
    }
}

// Clear entire screen by filling with spaces and mode / 清屏：用空格和指定模式填充整个显存
// Parameters: mode = color/attribute value (e.g., 0x07 for gray on black) / 参数：mode = 颜色/属性(例如0x07黑底灰字)
void clear_screen(unsigned char mode) {
    volatile char* video_memory = (volatile char*)VIDEO_MEMORY_ADDR;
    for (int i = 0; i < 80 * 25; i++) {
        video_memory[i * 2] = ' ';      // Character / 字符
        video_memory[i * 2 + 1] = mode; // Attribute / 属性
    }
}

// Update VGA hardware cursor position / 更新 VGA 硬件光标位置
// Parameters: row = cursor row (0-24), col = cursor column (0-79) / 参数：row = 光标行(0-24), col = 光标列(0-79)
void update_cursor(unsigned short row, unsigned short col) {
    unsigned short position = (row * 80 + col);
    if (position > 1999) position = 1999;  // Clamp to max position / 限制在最大位置
    
    // Write cursor position high byte to register 0x0E / 向寄存器 0x0E 写入光标位置高字节
    io_out8(0x3D4, 0x0E);
    io_out8(0x3D5, (position >> 8) & 0xFF);
    // Write cursor position low byte to register 0x0F / 向寄存器 0x0F 写入光标位置低字节
    io_out8(0x3D4, 0x0F);
    io_out8(0x3D5, position & 0xFF);
}

// Enable cursor with specified scan line range / 启用光标，指定扫描线范围
// Parameters: cursor_start = start scan line, cursor_end = end scan line / 参数：cursor_start = 起始扫描线, cursor_end = 结束扫描线
void enable_cursor(unsigned char cursor_start, unsigned char cursor_end) {
    io_out8(0x3D4, 0x0A);  // Cursor start register / 光标开始寄存器
    io_out8(0x3D5, (io_in8(0x3D5) & 0xC0) | cursor_start);
    io_out8(0x3D4, 0x0B);  // Cursor end register / 光标结束寄存器
    io_out8(0x3D5, (io_in8(0x3D5) & 0xE0) | cursor_end);
}

// Disable cursor by setting bit 5 of cursor start register / 禁用光标(设置光标开始寄存器的第 5 位)
void disable_cursor() {
    io_out8(0x3D4, 0x0A);  
    io_out8(0x3D5, 0x20);   // Bit 5 = 1 means cursor disabled / 第 5 位=1 禁用光标
}

// Get current cursor position on screen / 获取当前光标在屏幕上的位置
unsigned short get_cursor_position() {
    unsigned short position = 0;
    io_out8(0x3D4, 0x0E);  // Read cursor position high byte / 读取光标位置高字节
    position |= io_in8(0x3D5) << 8;
    io_out8(0x3D4, 0x0F);  // Read cursor position low byte / 读取光标位置低字节
    position |= io_in8(0x3D5);
    return position;
}

// Restore hardware cursor for shell display / 恢复硬件光标(以防退回 Shell 时没有光标了)
void console_show_cursor() {
    io_out8(0x3D4, 0x0A);
    io_out8(0x3D5, (io_in8(0x3D5) & 0xC0) | 0); // 扫描线开始
    io_out8(0x3D4, 0x0B);
    io_out8(0x3D5, (io_in8(0x3D5) & 0xE0) | 15); // 扫描线结束
}