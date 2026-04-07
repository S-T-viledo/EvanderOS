#include "stdio.h" 
#include "include.h"


static int shift_pressed = 0;
static int caps_lock = 0;
static int ext_code = 0; // 新增：用于记录是否收到了 0xE0 前缀(extend code)

char kbd_buffer[KBD_BUF_SIZE];
volatile int kbd_head = 0; // 写入指针
volatile int kbd_tail = 0; // 读取指针

// --------------------------------------------------------------------------
// (Scan Code Set 1)
// --------------------------------------------------------------------------

// 正常按键映射 (未按 Shift)
static char keymap_lower[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', /* 0x00 - 0x0E */
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', /* 0x0F - 0x1C */
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',         /* 0x1D - 0x29 */
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,            /* 0x2A - 0x36 */
    '*', 0, ' ', 0                                                           /* 0x37 - 0x3A */
};

// Shift 按下时的映射 (例如 1 -> !, a -> A)
static char keymap_upper[] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0
};

// --------------------------------------------------------------------------
// main handle function
// --------------------------------------------------------------------------
void keyboard_handler_main() {
    // 1. 从端口 0x60 读取扫描码
    unsigned char scancode = io_in8(0x60);
    unsigned int arrow_key = 0;

    // ----------------------------------------------------
    // 1. 处理扩展码前缀 (0xE0)
    // ----------------------------------------------------
    if (scancode == 0xE0) {
        ext_code = 1; // 标记下一个字节是扩展键
        return;       // 等待下一个中断
    }

    // ----------------------------------------------------
    // 2. 如果是扩展状态 (方向键等)
    // ----------------------------------------------------
    if (ext_code == 1) {
        // 只有在“按下”时处理 (最高位为0)
        if (!(scancode & 0x80)) {
            switch (scancode) {
                case 0x48: // Up
                    arrow_key = 19;
                    break;
                case 0x50: // Down                    
                    arrow_key = 20;
                    break;
                case 0x4B: // Left
                    arrow_key = 17;
                    break;
                case 0x4D: // Right
                    arrow_key = 18;
                    break;
                case 0x49: 
                    arrow_key = 21; 
                    break; // Page Up
                case 0x51: 
                    arrow_key = 22; 
                    break; // Page Down
                default:
                    break;
            }
        }
        // 处理完扩展键后，重置状态
        ext_code = 0;
        //return;
    }

    // 2. 处理特殊按键状态
    // 0x2A / 0x36 是左/右 Shift 的按下码
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return;
    }
    // 0xAA / 0xB6 是左/右 Shift 的断开码 (按下码 + 0x80)
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        return;
    }
    // Caps Lock (0x3A)
    if (scancode == 0x3A) {
        caps_lock = !caps_lock;
        return;
    }

    // 3. 判断是“按下”还是“松开”
    // 最高位为 1 (例如 0x9E) 代表松开按键，我们通常忽略普通键的松开，只处理按下
    if (scancode & 0x80) {
        return; 
    }

    // 4. 解析字符
    char ch = 0;
    
    // 确保扫描码在数组范围内
    if (scancode < sizeof(keymap_lower)) {
        // 确定使用哪个表
        // 逻辑：如果是字母，Caps Lock 和 Shift 会相互抵消
        // 如果是数字/符号，Caps Lock 无效，只看 Shift
        
        int is_letter = (keymap_lower[scancode] >= 'a' && keymap_lower[scancode] <= 'z');
        
        if (is_letter) {
            // 异或操作：Shift(1) ^ Caps(1) = 小写(0)
            if (shift_pressed ^ caps_lock) {
                ch = keymap_upper[scancode];
            } else {
                ch = keymap_lower[scancode];
            }
        } else {
            // 非字母字符只受 Shift 影响 (例如 1 变 !)
            if (shift_pressed) {
                ch = keymap_upper[scancode];
            } else {
                ch = keymap_lower[scancode];
            }
        }
    }
    switch (arrow_key)
    {
    case 17:
        ch = (char)arrow_key;
        break;
    case 18:
        ch = (char)arrow_key;
        break;
    default:
        break;
    }
    // 5. 输出字符
    if (ch != 0) {
        // 将字符存入缓冲区，不直接打印！
        kbd_buffer[kbd_head] = ch;
        kbd_head = (kbd_head + 1) % KBD_BUF_SIZE;
    }
}

// 3. 增加一个供系统调用读取的函数
char kbd_read_char() {
    // 如果缓冲区为空，阻塞当前进程，等待键盘中断唤醒
    while (kbd_head == kbd_tail) {
        // 1. 强制开启中断，允许键盘 IRQ1 进来！
        __asm__ __volatile__("sti"); 
        
        // 2. 让 CPU 挂起休眠，直到有硬件中断到来再唤醒它，避免 100% 占用死循环
        __asm__ __volatile__("hlt");
    }
    
    char c = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;

    return c;
}

// 内核空间：非阻塞获取按键
char getchar_nowait() {
    // 键盘缓冲区是由 head 和 tail 维护的
    if (kbd_head == kbd_tail) {
        return 0; // 缓冲区为空，立刻返回 0！绝对不准死循环！
    }
    // 取出按键并移动指针
    char c = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    return c;
}