#ifndef STDIO_H
#define STDIO_H

void init_console();
void console_set_color(unsigned char color);
void console_clear(unsigned char mode);
void console_set_cursor(int row, int col);
void printk(const char* format, ...);

#endif