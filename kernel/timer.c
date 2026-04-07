// timer.c - Timer and tick management / 定时器和时钟节拍管理
#include "include.h"
#include "stdio.h"

// Global variable recording system ticks since boot / 全局变量，记录系统启动以来经过了多少个 tick
unsigned int volatile jiffies = 0; 

// Initialize PIT (Programmable Interval Timer) / 初始化可编程中断定时器
void timer_init(unsigned int frequency) {
    // Calculate the initial counter value for the given frequency / 计算计数器的初值
    // For example: 1193182 / 100 = 11931 / 例如：1193182 / 100 = 11931
    unsigned int divisor = CLOCK_FREQ / frequency;

    // Write control byte to 0x43 / 写入控制字到 0x43：
    // 00(channel0) 11(read/write low byte then high byte) 011(mode3:square wave) 0(binary)
    // 00(通道0) 11(先读写低字节再读写高字节) 011(模式3:方波发生器) 0(二进制)
    // 00110110b = 0x36
    io_out8(PIT_CTRL, 0x36);

    // Write counter initial value to 0x40 - low byte / 写入计数器初值(0x40)-低字节
    io_out8(PIT_DATA, (unsigned char)(divisor & 0xFF));
    
    // Write counter initial value to 0x40 - high byte / 写入计数器初值(0x40)-高字节
    io_out8(PIT_DATA, (unsigned char)((divisor >> 8) & 0xFF));

    printk("PIT Timer Initialized. HZ=%d\n", frequency);
}