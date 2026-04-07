
#include "userlib.h"

static struct heap_block *uheap_head = 0;


/*------------------------------------------------------------------------------*/
/*--------------------------------系统调用封装------------------------------------*/
/*------------------------------------------------------------------------------*/
// No.0 print
void print(char *str) {
    int ret;
    __asm__ __volatile__("int $0x30" : "=b" (ret) : "a" (0), "b" (str) : "memory");
}

// No.1 exit task
void exit(int status) {
    int ret;
    __asm__ __volatile__("int $0x30" : "=a" (ret) : "a" (1), "b" (status) : "memory");
}

// No.2 wait son
int waitpid(int pid) {
    int ret;
    __asm__ __volatile__("int $0x30" : "=b" (ret) : "a" (2), "b" (pid) : "memory");
    return ret;
}

// No.3 get char from keyboard buffer(by kb interrupt)
char getchar() {
    int ret;
    __asm__ __volatile__("int $0x30" : "=b" (ret) : "a" (3) : "memory");
    return (char)ret;
}

// No.4 create a new task
int new_task(char *filepath, char *arg) {
    int ret;
    __asm__ __volatile__("int $0x30" : "=b" (ret) : "a" (4), "b" (filepath), "c" (arg) : "memory");
    return ret;
}

// No.5
void ls(char* path) {
    int ret;
    __asm__ __volatile__("int $0x30" : "=a" (ret) : "a" (5), "b" (path) : "memory");
}

// No.6
int mkdir(char* path) {
    int ret;
    __asm__ __volatile__("int $0x30" : "=b" (ret) : "a" (6), "b" (path) : "memory");
    return ret;
}

// No.7
int touch(char* path) {
    int ret;
    __asm__ __volatile__("int $0x30" : "=b" (ret) : "a" (7), "b" (path) : "memory");
    return ret;
}

// No.8 remove a file
int rm(char* path) {
    int ret;
    __asm__ __volatile__("int $0x30" : "=b" (ret) : "a" (8), "b" (path) : "memory");
    return ret;
}

// No.9 rm recruisive
int rmall(char* path) {
    int ret;
    __asm__ __volatile__("int $0x30" : "=b" (ret) : "a" (9), "b" (path) : "memory");
    return ret;
}

// No.10 
int get_file_size(char* path) {
    int ret;
    __asm__ __volatile__("int $0x30" : "=b" (ret) : "a" (10), "b" (path) : "memory");
    return ret;
}

// No.11 read file to buffer, return file size
int read_file(char* path, char* buffer) {
    int ret;
    __asm__ __volatile__("int $0x30" : "=b" (ret) : "a" (11), "b" (path), "c" (buffer) : "memory");
    return ret;
}

// No.12 覆盖写入文件：如果不存在会返回错误（需要先用 touch 创建）
int write_file(char* path, char* buffer, unsigned int size) {
    int ret;
    // 发起系统调用 6
    __asm__ __volatile__("int $0x30" : "=b" (ret) : "a" (12), "b" (path), "c" (buffer), "d" (size) : "memory");
    return ret;
}

// No.13 堆扩容（inc byte）
void* sbrk(int inc) {
    int ret;
    __asm__ __volatile__("int $0x30" : "=b" (ret) : "a" (13), "b" (inc) : "memory");
    return (void*)ret;
}

// No.14 只有A是B的父进程才能kill， 否则返回-2；找不到目标，或者目标已经死亡，或者企图谋杀 idle 进程返回-1
int kill(int pid) {
    int ret;
    __asm__ __volatile__("int $0x30" : "=b" (ret) : "a" (14), "b" (pid) : "memory");
    return ret;
}

// No.15 真正的清屏 (color 传 0x07 即可)
void clear(unsigned char color) {
    __asm__ __volatile__("int $0x30" : : "a" (15), "b" (color) : "memory");
}

// No.16 绝对光标定位 (x: 0~24, y: 0~79)
void set_cursor(unsigned int x, unsigned int y) {
    __asm__ __volatile__("int $0x30" : : "a" (16), "b" (x), "c" (y) : "memory");
}

// No.17 设置之后的打印颜色
void set_color(unsigned char color) {
    __asm__ __volatile__("int $0x30" : : "a" (17), "b" (color) : "memory");
}

// No.18 目录鉴定系统调用
int is_dir(char* path) {
    int ret;
    __asm__ __volatile__("int $0x30" : "=b" (ret) : "a" (18), "b" (path) : "memory");
    return ret;
}

/*--------------------------------------------------------------------------------*/
/*-----------------------------------字符串功能函数---------------------------------*/
/*--------------------------------------------------------------------------------*/

/*将dst_起始的size个字节置为value*/
void memset(void *dst_, unsigned char value, unsigned int size)
{
  unsigned char *dst = (unsigned char *)dst_;
  while (size-- > 0)
    *dst++ = value;
}
/*将src_起始的size个字节复制到dst_*/
void memcpy(void *dst_, const void *src_, unsigned int size)
{
  unsigned char *dst = dst_;
  const unsigned char *src = src_;
  while (size-- > 0)
    *dst++ = *src++;
}

/*连续比较以地址a_和地址b_开头的size个字节，若相等则返回0，若a_大于b_，返回+1，否则返回-1*/
int memcmp(const void *a_, const void *b_, unsigned int size)
{
  const char *a = a_;
  const char *b = b_;
  while (size-- > 0)
  {
    if (*a != *b){return *a > *b ? 1 : -1;}
    a++;
    b++;
  }
  return 0;
}

/*将字符串从src_复制到dst_*/
char *strcpy(char *dst_, const char *src_)
{
  char *r = dst_; //用来返回目的字符串起始地址
  while ((*dst_++ = *src_++)); //字符串以\0结尾
  return r;
}

/*返回字符串长度*/
unsigned int strlen(const char *str)
{
  const char *p = str;
  while (*p++);
  return (p - str - 1); //为啥书中要多减个1呢？因为此时的指针p指向了字符串结尾0后的那个字节。
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

/*将字符串 src_ 追加到 dst_ 的末尾*/
char *strcat(char *dst_, const char *src_) {
    char *r = dst_;
    while (*dst_) dst_++; // 找到目标字符串的 \0
    while ((*dst_++ = *src_++)); // 拷贝直到 \0
    return r;
}



void* malloc(unsigned int size) {
    if (size == 0) return 0;
    
    // 强制 4 字节对齐
    unsigned int aligned_size = (size + 3) & ~3;

    // 第一次调用 malloc，初始化链表头
    if (!uheap_head) {
        void* base = sbrk(aligned_size + BLOCK_SIZE);
        if (base == (void*)-1) return 0;
        
        uheap_head = (struct heap_block*)base;
        uheap_head->magic = HEAP_MAGIC;
        uheap_head->size = aligned_size;
        uheap_head->is_free = 0;
        uheap_head->next = 0;
        uheap_head->prev = 0;
        return (void*)((unsigned int)uheap_head + BLOCK_SIZE);
    }

    struct heap_block *curr = uheap_head;
    struct heap_block *last = 0;

    // 遍历链表，寻找被 free 掉的、大小合适的空闲块
    while (curr) {
        if (curr->is_free && curr->size >= aligned_size) {
            curr->is_free = 0; // 找到就复用它
            return (void*)((unsigned int)curr + BLOCK_SIZE);
        }
        last = curr;
        curr = curr->next;
    }

    // 没有空闲块，直接呼叫内核 sbrk 扩容！
    void* new_space = sbrk(aligned_size + BLOCK_SIZE);
    if (new_space == (void*)-1) return 0;
    
    struct heap_block *new_block = (struct heap_block*)new_space;
    new_block->magic = HEAP_MAGIC;
    new_block->size = aligned_size;
    new_block->is_free = 0;
    new_block->next = 0;
    new_block->prev = last;
    last->next = new_block;

    return (void*)((unsigned int)new_block + BLOCK_SIZE);
}


// 内部工具函数：向后吞并（将当前节点与它的 next 节点合并）
static void merge_next(struct heap_block *node) {
    if (!node || !node->next) return;
    
    // 如果下一个块也是空闲的
    if (node->next->is_free) {
        // 1. 大小相加：当前块大小 + 块头信息大小(BLOCK_SIZE) + 下一块大小
        node->size += BLOCK_SIZE + node->next->size;
        
        // 2. 链表跨越：当前的 next 指向 下下个
        node->next = node->next->next;
        
        // 3. 反向链接：如果下下个存在，让它的 prev 指向当前节点
        if (node->next) {
            node->next->prev = node;
        }
    }
}

// 释放内存主函数
void free(void* ptr) {
    if (!ptr) return;
    
    // 退回到块头的起始位置
    struct heap_block *block = (struct heap_block*)((unsigned int)ptr - BLOCK_SIZE);
    
    // 【核心防御】：必须检查魔数，防止野指针或重复 free 导致链表彻底崩溃！
    if (block->magic != HEAP_MAGIC) {
        // 遇到了野指针，直接静默返回（在标准 C 库里这里通常会抛出 core dump）
        return; 
    }

    // 1. 标记为空闲
    block->is_free = 1; 

    // 2. 向后合并 (Merge Next)
    // 如果紧挨在后面的邻居是空闲的，吃掉它！
    if (block->next && block->next->is_free) {
        merge_next(block);
    }

    // 3. 向前合并 (Merge Prev)
    // 如果前面的邻居也是空闲的，让它把我们现在这个块吃掉！
    if (block->prev && block->prev->is_free) {
        merge_next(block->prev);
    }
}