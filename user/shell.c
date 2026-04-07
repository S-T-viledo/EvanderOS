#include "userlib.h"

// ==========================================
// Shell Command Parser and Built-in Commands
// Shell 命令解析器和内置命令
// ==========================================

void split_cmd(char *cmd_line, char **cmd, char **arg);
char* arg_process(char *arg);

// Function declarations for built-in commands
// 内置命令函数声明
int cmd_help(char *args);
int cmd_clear(char *arg);
int cmd_about(char *args);
int cmd_ls(char* args);
int cmd_cd(char *arg);
int cmd_mkdir(char *args);
int cmd_touch(char *arg);
int cmd_rm(char *arg);
int cmd_rmall(char *arg);
int cmd_echo(char *arg);
int cmd_cp(char *arg);
int evim(char *arg);
int easm(char *arg);
void get_input(char *buf);

// Path processing functions
// 路径处理函数
void get_abs_path(char* arg, char* abs_path);
void normalize_path(char* path);

// ==========================================
// Command Table Design (Built-in Commands Router)
// 内置命令与路由表设计 (Command Table)
// ==========================================

// Define function pointer type for command handlers
// 定义命令处理函数的指针类型
typedef int (*cmd_func_t)(char *args);

// Command structure definition
// 命令结构体定义
struct command {
    char *name;         // Command name (e.g., "help") / 命令名 (如 "help")
    cmd_func_t func;    // Corresponding handler function / 对应的处理函数
    char *description;  // Help description / 帮助描述
};

// Built-in commands table - add new commands here
// 内置命令表 - 在这里添加新命令
struct command builtin_cmds[] = {
    {"help" , cmd_help ,  "Show all built-in commands"},
    {"clear", cmd_clear, "Clear the screen"},
    {"about", cmd_about, "Show OS information"},
    {"ls"   , cmd_ls,    "list current or specific directory"},
    {"cd"   , cmd_cd,    "change current working directory"},
    {"mkdir", cmd_mkdir, "make a new dir in a piven path"},
    {"touch", cmd_touch, "create an empty new file"},
    {"rm"   , cmd_rm,    "delete a current file"},
    {"rmall", cmd_rmall, "recruisively delete every files and dirs"},
    {"echo" , cmd_echo,  "write into files or print on screen"},
    {"cp"   , cmd_cp,    "copy files"},
    {"evim" , evim,      "A vim like txt editor by Evander"},
    {"easm" , easm,      "Evander's assembler"},
    {0, 0, 0}                                               
    // Array terminator / 数组结束标志
};

// Global shell state: current working directory
// Shell 全局状态：当前工作目录
char current_dir[128] = "/";

int main() {
    char cmd_line[128];
    
    // Initialize shell display / 初始化 Shell 显示
    clear(0x07);
    set_cursor(0,0);
    set_color(MAKE_COLOR(VGA_COLOR_BLUE, VGA_COLOR_YELLOW));
    print(" ============================================================================== ");
    print("||-------           ****   Welcome to EvanderOS Shell!   ****          -------||");
    print(" ============================================================================== ");
    set_color(COLOR_DEFAULT);
    
    // Main shell loop - process commands indefinitely / 主 Shell 循环 - 无限处理命令
    while (1) {
        // Display prompt with current directory / 显示带当前目录的提示符
        set_color(MAKE_COLOR(VGA_COLOR_BLACK, VGA_COLOR_CYAN));
        print(" EvanderOS#");
        print(current_dir);
        print(": ");
        set_color(COLOR_DEFAULT);
        
        // Get user input / 获取用户输入
        get_input(cmd_line);
        
        // Skip empty commands / 跳过空命令
        if (cmd_line[0] == '\0') continue;

        // Parse command and arguments / 解析命令和参数
        char *cmd, *arg;
        split_cmd(cmd_line, &cmd, &arg);

        // Check if it's a built-in command / 检查是否为内置命令
        int is_builtin = 0;
        for (int i = 0; builtin_cmds[i].name != 0; i++) {
            if (strcmp(cmd, builtin_cmds[i].name) == 0) {
                // Execute built-in command and wait if it spawned a process / 执行内置命令，如果产生了进程则等待
                int ret_value = builtin_cmds[i].func(arg);
                if (ret_value > 0) waitpid(ret_value);
                is_builtin = 1;
                break;
            }
        }

        // If not built-in, try to execute as external program / 如果不是内置命令，尝试作为外部程序执行
        if (!is_builtin) {
            // Pass arguments to kernel for execution / 将参数传递给内核执行
            int child_pid = new_task(cmd, arg); 
            
            if (child_pid > 0) waitpid(child_pid); 
            else if (child_pid == -1) print(" Error: Command not found.\n");
            else print(" Error: Execution failed.\n");
        }
    }
    return 0;
}


// 增加字符串分割函数
void split_cmd(char *cmd_line, char **cmd, char **arg) {
    *cmd = cmd_line;
    *arg = 0; 
    while (*cmd_line != '\0') {
        if (*cmd_line == ' ') {
            *cmd_line = '\0'; // 截断字符串，变成前半部分
            cmd_line++;
            while (*cmd_line == ' ') cmd_line++; // 跳过多余空格
            if (*cmd_line != '\0') *arg = cmd_line; // 找到参数
            break;
        }
        cmd_line++;
    }
}
                                 


// ==========================================
// Built-in Command Implementations
// 内置命令的具体实现
// ==========================================

// Display help information for all built-in commands
// 显示所有内置命令的帮助信息
int cmd_help(char *args) {
    print(" EvanderOS Built-in Commands:\n");
    for (int i = 0; builtin_cmds[i].name != 0; i++) {
        print("  ");
        print(builtin_cmds[i].name);
        print(" - ");
        print(builtin_cmds[i].description);
        print("\n");
    }
    print("\n Or type a program path to execute (e.g., /BIN/LS.BIN)\n");
    return 0;
}

// Clear the screen
// 清屏
int cmd_clear(char *arg)
{
    clear(0x07);
    return 0;
}

// Show OS information
// 显示操作系统信息
int cmd_about(char *args) {
    print("\n EvanderOS v1.0\n");
    print(" A custom 32-bit operating system created from scratch.\n");
    return 0;
}

// List directory contents - spawns ls.bin process
// 列出目录内容 - 启动 ls.bin 进程
int cmd_ls(char *arg)
{
    char abs_path[128]; get_abs_path(arg, abs_path);
    return new_task("/BIN/LS.BIN", abs_path);
}

// Create directory - spawns mkdir.bin process
// 创建目录 - 启动 mkdir.bin 进程
int cmd_mkdir(char *arg)
{
    char abs_path[128]; get_abs_path(arg, abs_path);
    return new_task("/BIN/MKDIR.BIN", abs_path);
}

// Create empty file - spawns touch.bin process
// 创建空文件 - 启动 touch.bin 进程
int cmd_touch(char *arg)
{
    char abs_path[128]; get_abs_path(arg, abs_path);
    return new_task("/BIN/TOUCH.BIN", abs_path);
}

// Delete file - spawns rm.bin process
// 删除文件 - 启动 rm.bin 进程
int cmd_rm(char *arg)
{
    char abs_path[128]; get_abs_path(arg, abs_path);
    return new_task("/BIN/RM.BIN", abs_path);
}

// Recursively delete directory - spawns rmall.bin process
// 递归删除目录 - 启动 rmall.bin 进程
int cmd_rmall(char *arg)
{
    char abs_path[128]; get_abs_path(arg, abs_path);
    return new_task("/BIN/RMALL.BIN", abs_path);
}

// Echo command - spawns echo.bin process
// Echo 命令 - 启动 echo.bin 进程
int cmd_echo(char *arg)
{
    char abs_path[128]; get_abs_path(arg, abs_path);
    return new_task("/BIN/ECHO.BIN", abs_path);
}

// Copy files - spawns cp.bin process
// 复制文件 - 启动 cp.bin 进程
int cmd_cp(char *arg) {
    return new_task("/BIN/CP.BIN", arg_process(arg));
}

// Launch text editor - spawns evim.bin process
// 启动文本编辑器 - 启动 evim.bin 进程
int evim(char *arg)
{
    char abs_path[128]; get_abs_path(arg, abs_path);
    return new_task("/BIN/EVIM.BIN", abs_path);
}

// Launch assembler - spawns easm.bin process
// 启动汇编器 - 启动 easm.bin 进程
int easm(char *arg)
{
    char abs_path[128];
    return new_task("/BIN/EASM.BIN", arg_process(arg));
}
// ==========================================
// Shell Core Logic - Input Handling
// Shell 核心逻辑 - 输入处理
// ==========================================

// Advanced input handler with cursor movement and editing capabilities
// 高级输入处理器，支持光标移动和编辑功能
void get_input(char *buf) {
    int len = 0;       // Total characters in buffer / 缓冲区的总字符数
    int cursor_x = 0;  // Current cursor position in buffer / 当前光标在缓冲区中的真实索引位置
    
    while (1) {
        char c = getchar();
        
        // 1. Enter key - finish input / 回车键 - 完成输入
        if (c == '\r' || c == '\n') {
            buf[len] = '\0';
            print("\n");
            break;
        } 
        
        // 2. Backspace key - delete character / 退格键 - 删除字符
        else if (c == '\b') { 
            if (cursor_x > 0) {
                // Shift characters after cursor left by one / 将光标后的字符全部前移一位
                for (int j = cursor_x - 1; j < len - 1; j++) {
                    buf[j] = buf[j + 1];
                }
                len--;
                cursor_x--;
                
                // Redraw screen after backspace / 退格后的屏幕重绘
                print("\b"); // Move cursor left / 光标真退一格
                
                // Print all characters after cursor position / 打印光标之后的所有字符（覆盖掉原来的）
                for (int j = cursor_x; j < len; j++) {
                    char str[2] = {buf[j], 0};
                    print(str);
                }
                // Print space to clear residual character / 打印一个空格清除最后一个残留的字符
                print(" ");
                
                // Move cursor back to correct position / 把光标退回到正确的位置！(退 len - cursor_x + 1 次)
                for (int j = 0; j <= len - cursor_x; j++) {
                    print("\b");
                }
            }
        } 
        
        // 3. Left arrow key - move cursor left / 左方向键 - 光标左移
        else if (c == 17) { 
            if (cursor_x > 0) {
                cursor_x--;
                print("\b"); // Cursor moves left / 依赖屏幕驱动支持 \b 仅为光标左移
            }
        }
        
        // 4. Right arrow key - move cursor right / 右方向键 - 光标右移
        else if (c == 18) {
            if (cursor_x < len) {
                // Print character at cursor to move cursor right / 打印当前光标下的字符，光标自然会向右走
                char str[2] = {buf[cursor_x], 0};
                print(str); 
                cursor_x++;
            }
        }
        
        // 5. Regular characters - insert mode / 普通字符（真正的插入模式！）
        else if (c >= 32 && c <= 126 && len < 127) {
            // Shift characters after cursor right by one to make space / 将光标后的字符全部后移一位，腾出空间！
            for (int j = len; j > cursor_x; j--) {
                buf[j] = buf[j - 1];
            }
            buf[cursor_x] = c; // Insert new character / 插入新字符
            len++;
            
            // Redraw screen after insertion / 插入后的屏幕重绘
            // Print new character and all shifted characters / 把新字符和它后面的被挤开的字符全部打印一遍
            for (int j = cursor_x; j < len; j++) {
                char str[2] = {buf[j], 0};
                print(str);
            }
            
            // Move cursor back to position after inserted character / 把光标倒退回到刚刚插入的新字符的下一个位置！
            for (int j = 0; j < len - cursor_x - 1; j++) {
                print("\b");
            }
            cursor_x++;
        }
    }
}







// ==========================================
// Path Resolution Engine - Path Processing
// 路径解析引擎 - 路径处理
// ==========================================

// Normalize messy paths like /BIN/../TEST/./DIR into clean absolute paths
// 将凌乱的路径 (如 /BIN/../TEST/./DIR) 变成干净的最简路径
void normalize_path(char* path) {
    char stack[16][32]; // Path stack: max 16 levels, 32 chars each / 路径栈：最多支持16层目录，每层32个字符
    int top = 0;
    int i = 0;

    while (path[i] != '\0') {
        while (path[i] == '/') i++; // Skip redundant slashes / 跳过多余的斜杠
        if (path[i] == '\0') break;

        char part[32];
        int p = 0;
        // Extract directory name / 切割出一个目录名
        while (path[i] != '/' && path[i] != '\0') {
            part[p++] = path[i++];
        }
        part[p] = '\0';

        if (strcmp(part, ".") == 0) {
            continue; // Current directory, ignore / 当前目录，忽略
        } else if (strcmp(part, "..") == 0) {
            if (top > 0) top--; // Pop stack: go to parent / 弹栈：回到上一级
        } else {
            strcpy(stack[top++], part); // Push stack: enter next level / 压栈：进入下一级
        }
    }

    // Reconstruct clean absolute path / 重构干净的绝对路径
    path[0] = '/';
    path[1] = '\0';
    for (int k = 0; k < top; k++) {
        if (k > 0) strcat(path, "/");
        strcat(path, stack[k]);
    }
}

// Convert user input arg combined with current directory to absolute path
// 把用户输入的 arg，结合当前工作目录，转换为绝对路径
void get_abs_path(char* arg, char* abs_path) {
    if (arg == 0 || arg[0] == '\0') {
        strcpy(abs_path, current_dir); // No argument, default to current directory / 没传参数，默认当前目录
        return;
    }
    if (arg[0] == '/') {
        strcpy(abs_path, arg); // Already absolute path / 本身就是绝对路径
    } else {
        strcpy(abs_path, current_dir); // Relative path, prepend current directory / 相对路径，拼接到当前目录后面
        if (abs_path[strlen(abs_path) - 1] != '/') strcat(abs_path, "/");
        strcat(abs_path, arg);
    }
    normalize_path(abs_path);
}

// Convert arguments like (SRC.ASM DST.BIN) to absolute paths (SRC.ASM /USER/DST.BIN)
// 把由两个相对路径组成的参数变成两个绝对路径的新参数，eg (SRC.ASM DST.BIN)->(/USER/SRC.ASM /USER/DST.BIN)
char* arg_process(char *arg) {
    if (arg == 0 || arg[0] == '\0') {
        print(" Usage: keyword <source> <destination>\n");
        return 0; // Return 0 means parsing failed / 返回 0 代表解析失败
    }

    // 1. Split arguments (extract src and dst) / 拆分参数 (提取 src 和 dst)
    char src[64] = {0};
    char dst[64] = {0};
    int i = 0, j = 0;
    
    while (arg[i] == ' ') i++;
    while (arg[i] != ' ' && arg[i] != '\0') src[j++] = arg[i++];
    src[j] = '\0';
    
    while (arg[i] == ' ') i++;
    j = 0;
    while (arg[i] != ' ' && arg[i] != '\0') dst[j++] = arg[i++];
    dst[j] = '\0';
    
    if (src[0] == '\0' || dst[0] == '\0') {
        print(" Error: Missing source or destination.\n");
        return 0;
    }

    // 2. Convert to absolute paths / 转换为绝对路径
    char abs_src[128];
    char abs_dst[128];
    get_abs_path(src, abs_src);
    get_abs_path(dst, abs_dst);

    static char new_arg[256]; 
    
    // 3. Reconstruct arguments / 重新拼接
    strcpy(new_arg, abs_src);
    strcat(new_arg, " ");
    strcat(new_arg, abs_dst);
    
    return new_arg; // Now it's safe to return this pointer! / 现在返回这个指针就绝对安全了！
}



// Change directory command implementation
// 改变目录命令实现
int cmd_cd(char *arg) {
    char target_path[128];
    get_abs_path(arg, target_path);

    // Root directory always exists, allow directly / 根目录绝对存在，直接放行
    if (target_path[0] == '/' && target_path[1] == '\0') {
        strcpy(current_dir, target_path);
    } else {
        // Let kernel verify: is this really a directory? / 让内核做检验：它真的是个文件夹吗？
        if (is_dir(target_path)) {
            strcpy(current_dir, target_path);
        } else {
            print(" cd: not a directory or doesn't exist: ");
            print(arg);
            print("\n");
        }
    }
    return 0;
}