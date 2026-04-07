#define HEAP_MAGIC 0x12345678
#define BLOCK_SIZE sizeof(struct heap_block) // sizeof(struct heap_block)

// --- VGA 16 色调色板 ---
#define VGA_COLOR_BLACK         0x00
#define VGA_COLOR_BLUE          0x01
#define VGA_COLOR_GREEN         0x02
#define VGA_COLOR_CYAN          0x03
#define VGA_COLOR_RED           0x04
#define VGA_COLOR_MAGENTA       0x05
#define VGA_COLOR_BROWN         0x06
#define VGA_COLOR_LIGHT_GRAY    0x07
#define VGA_COLOR_DARK_GRAY     0x08
#define VGA_COLOR_LIGHT_BLUE    0x09
#define VGA_COLOR_LIGHT_GREEN   0x0A
#define VGA_COLOR_LIGHT_CYAN    0x0B
#define VGA_COLOR_LIGHT_RED     0x0C
#define VGA_COLOR_LIGHT_MAGENTA 0x0D
#define VGA_COLOR_YELLOW        0x0E
#define VGA_COLOR_WHITE         0x0F

// 魔法宏：组合背景色和前景色
#define MAKE_COLOR(bg, fg) ((unsigned char)(((bg) << 4) | (fg)))

// Evim 专属 UI 配色方案
#define COLOR_DEFAULT   MAKE_COLOR(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GRAY) // 0x07: 黑底灰字(普通文本)
#define COLOR_TILDE     MAKE_COLOR(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_BLUE) // 0x09: 黑底亮蓝字(波浪号)
#define COLOR_STATUSBAR MAKE_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK) // 0x70: 灰底黑字(底部状态栏)
#define COLOR_INSERT    MAKE_COLOR(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK) // 0xB0: 亮青底黑字(插入模式专属底栏)
#define COLOR_VISUAL    MAKE_COLOR(VGA_COLOR_DARK_GRAY, VGA_COLOR_WHITE)  // 0x8F: 深灰底白字(未来选中高亮用)

struct heap_block {
    unsigned int magic;
    unsigned int size;
    int is_free;
    struct heap_block *next;
    struct heap_block *prev;
};


// No.0 print
void print(char *str);
// No.1 exit task
void exit(int status);
// No.2 wait son
int waitpid(int pid);
// No.3 get char from keyboard buffer(by kb interrupt)
char getchar();
// No.4 create a new task
int new_task(char *filepath, char *arg);
// No.5
void ls(char* path);
// No.6
int mkdir(char* path);
// No.7
int touch(char* path);
// No.8 remove a file
int rm(char* path);
// No.9 rm recruisive
int rmall(char* path);
// No.10 
int get_file_size(char* path) ;
// No.11 read file to buffer, return file size
int read_file(char* path, char* buffer);
// No.12 覆盖写入文件：如果不存在会返回错误（需要先用 touch 创建）
int write_file(char* path, char* buffer, unsigned int size);
// No.13 堆扩容（inc byte）
void* sbrk(int inc) ;
// No.14 只有A是B的父进程才能kill， 否则返回-2；找不到目标，或者目标已经死亡，或者企图谋杀 idle 进程返回-1
int kill(int pid);
// No.15 清屏（0x07白底黑字）
void clear(unsigned char color);
// No.16 绝对光标定位 (x: 0~24, y: 0~79)
void set_cursor(unsigned int row, unsigned int col);
// No.17 设置之后的打印颜色
void set_color(unsigned char color);
// No.18 目录鉴定系统调用
int is_dir(char* path);

/*将dst_起始的size个字节置为value*/
void memset(void *dst_, unsigned char value, unsigned int size);
/*将src_起始的size个字节复制到dst_*/
void memcpy(void *dst_, const void *src_, unsigned int size);
/*连续比较以地址a_和地址b_开头的size个字节，若相等则返回0，若a_大于b_，返回+1，否则返回-1*/
int memcmp(const void *a_, const void *b_, unsigned int size);
/*将字符串从src_复制到dst_*/
char *strcpy(char *dst_, const char *src_);
/*返回字符串长度*/
unsigned int strlen(const char *str);
int strcmp(const char *s1, const char *s2);
/*将字符串 src_ 追加到 dst_ 的末尾*/
char *strcat(char *dst_, const char *src_);

void* malloc(unsigned int size);
// 释放内存主函数
void free(void* ptr);
