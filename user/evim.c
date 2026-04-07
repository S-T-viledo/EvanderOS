
#include "userlib.h"

// ==========================================
// 1. Data Structure Design (Model)
// 数据结构设计 (Model)
// ==========================================
#define MAX_LINE_LEN 128

// Each line of text is a node in doubly linked list
// 每一行文本都是双向链表中的一个节点
struct Line {
    char text[MAX_LINE_LEN]; // Current line's text content / 当前行的文本内容
    int length;              // Current line's character count / 当前行的字符数
    struct Line* prev;       // Previous line / 上一行
    struct Line* next;       // Next line / 下一行
};

// --- Global Editor State ---
// 编辑器全局状态
struct Line* head_line = 0;  // First line of file / 文件的第一行
struct Line* curr_line = 0;  // Current line where cursor is (logical line) / 当前光标所在的行 (逻辑行)
int cursor_x = 0;            // Cursor position within current line (logical column) / 当前行内的光标位置 (逻辑列)

// --- Screen Viewport (Viewport) ---
// 屏幕视口 (Viewport)
int row_offset = 0;          // Which line of linked list is at top of screen? (for scrolling) / 屏幕顶端显示的是链表的第几行？(用于滚屏)
int screen_cursor_y = 0;     // Physical cursor Y coordinate on screen (0~23) / 物理光标在屏幕上的 Y 坐标 (0~23)

// --- State Machine ---
// 状态机
#define MODE_NORMAL 0
#define MODE_INSERT 1
int current_mode = MODE_NORMAL;

char current_filename[64] = {0}; // Current editing filename / 当前编辑的文件名
int is_modified = 0;             // Whether file has been modified / 是否被修改过
int col_offset = 0; // Horizontal viewport offset / 横向滑动视口偏移量

// Function declarations / 函数声明
void load_file(char* filename);
void save_file();
void move_left();
void move_right();
void move_up();
void move_down();
void page_up();
void page_down();
void adjust_col_offset();

// ==========================================
// 2. Memory and Linked List Operations
// 内存与链表操作
// ==========================================

// Create a new empty line node
// 创建一个新的空行节点
struct Line* create_empty_line() {
    struct Line* l = (struct Line*)malloc(sizeof(struct Line));
    if (!l) return 0;
    l->length = 0;
    l->text[0] = '\0';
    l->prev = 0;
    l->next = 0;
    return l;
}

// ==========================================
// 3. Screen Rendering Engine (View) - Flicker-Free Overwrite Version
// 屏幕渲染引擎 (View) - 无闪烁覆写版
// ==========================================

// Render entire screen with current viewport and cursor position
// 渲染整个屏幕，包含当前视口和光标位置
void render_screen() {
    struct Line* render_line = head_line;
    // Skip to the line that should be at the top of screen / 跳到应该在屏幕顶端的行
    for (int i = 0; i < row_offset && render_line != 0; i++) {
        render_line = render_line->next;
    }

    // Performance killer: build 80-character buffer for each line, output at once!
    // 性能绝杀：为每一行构建 80 字符的缓冲区，一次性输出！
    char line_buf[81]; 

    // 2. Text view area (lines 0 ~ 22) / 文本视图区 (0 ~ 22 行)
    for (int i = 0; i < 23; i++) {
        set_cursor(i, 0); 
        if (render_line != 0) {
            set_color(COLOR_DEFAULT);
            int print_len = 0;
            
            // 2D viewport magic: only display characters from col_offset onwards!
            // 二维视口魔法：只截取从 col_offset 开始的字符！
            if (render_line->length > col_offset) {
                for (int j = col_offset; j < render_line->length && print_len < 80; j++) {
                    line_buf[print_len++] = render_line->text[j];
                }
            }
            // Fill remaining space with spaces to clear artifacts / 空格补齐残影
            while (print_len < 80) line_buf[print_len++] = ' ';
            line_buf[80] = '\0';
            
            print(line_buf); // Reduced from 80 system calls to 1! / 把原本 80 次系统调用，浓缩成了 1 次！
            render_line = render_line->next;
        } else {
            set_color(COLOR_TILDE);
            line_buf[0] = '~';
            for (int j = 1; j < 80; j++) line_buf[j] = ' ';
            line_buf[80] = '\0';
            print(line_buf); 
        }
    }

    // 3. File info status bar (line 23) / 文件信息状态栏 (第 23 行)
    set_cursor(23, 0);
    set_color(COLOR_STATUSBAR);
    print(" FILE: ");
    print(current_filename);
    if (is_modified) print(" [+]");
    int len = 7 + strlen(current_filename) + (is_modified ? 4 : 0);
    for (int i = len; i < 80; i++) print(" ");

    // 4. Command and mode bar (line 24) / 命令与模式栏 (第 24 行)
    set_cursor(24, 0);
    set_color(COLOR_DEFAULT);
    for(int i = 0; i < 79; i++) print(" ");
    
    char* mode_str = (current_mode == MODE_NORMAL) ? "-- NORMAL --" : "-- INSERT --";
    int mode_len = strlen(mode_str);
    set_cursor(24, 79 - mode_len);
    
    if (current_mode == MODE_INSERT) set_color(COLOR_INSERT);
    else set_color(COLOR_DEFAULT); 
    print(mode_str);

    // 5. Reposition cursor / 光标归位
    set_color(COLOR_DEFAULT);
    // Physical screen coordinate = logical cursor - horizontal scroll offset!
    // 屏幕上的物理坐标 = 逻辑光标 - 横向滑动偏移量！
    set_cursor(screen_cursor_y, cursor_x - col_offset); 
}



// ==========================================
// 4. Main Control Loop (Controller)
// 主控循环 (Controller)
// ==========================================
int main(char *arg) {
    // 1. Parse filename to edit / 解析要编辑的文件名
    if (arg != 0 && arg[0] != '\0') {
        strcpy(current_filename, arg);
        
    } else {
        strcpy(current_filename, "UNNAMED");
    }

    // 2. Initialize first line / 初始化第一行
    head_line = create_empty_line();
    curr_line = head_line;

    load_file(current_filename);
    // 3. Initial render / 首次渲染
    render_screen();

    // 4. Editor main loop / 编辑器主循环
    while (1) {
        char c = getchar();
        
        // ==========================================
        // 0. Global Physical Arrow Key Interception (Universal for both modes)
        // 全局物理方向键拦截 (双模式通用)
        // ==========================================
        if (c == 17)      { move_left(); }
        else if (c == 18) { move_right(); }
        else if (c == 19) { move_up(); }
        else if (c == 20) { move_down(); }
        
        // ==========================================
        // 1. NORMAL Mode (Command Mode)
        // NORMAL 模式 (指令模式)
        // ==========================================
        else if (current_mode == MODE_NORMAL) {
            
            // [h, j, k, l] Movement / 移动
            if (c == 'h')      { move_left(); }
            else if (c == 'l') { move_right(); }
            else if (c == 'k') { move_up(); }
            else if (c == 'j') { move_down(); }
            
            // [i] Enter insert mode / 进入插入模式
            else if (c == 'i') {
                current_mode = MODE_INSERT;
            }
            
            // [Shift+Z] Save and exit / 保存并退出
            else if (c == 'Z') {
                if (is_modified) save_file();
                break;
            }
            
            // [q or Q] Exit without saving (with safety confirmation!)
            // 不保存退出 (带防呆确认机制！)
            else if (c == 'q' || c == 'Q') {
                if (!is_modified) {
                    break; // Not modified, exit cleanly / 没修改过，直接痛快退出
                } else {
                    // Intercept! Force display warning on line 24 / 拦截！破坏当前界面，强行在第 24 行画出警告
                    set_cursor(24, 0);
                    set_color(COLOR_STATUSBAR);
                    print(" WARNING: Unsaved changes! Quit anyway? (y/n) ");
                    
                    // Start sub-state machine, wait for user response / 开启子状态机，死等用户回答
                    while (1) {
                        char confirm = getchar();
                        if (confirm == 'y' || confirm == 'Y') {
                            set_color(COLOR_DEFAULT);
                            clear(COLOR_DEFAULT);
                            set_cursor(0, 0);
                            exit(0); // Confirm discard, end process directly! / 确认放弃，直接结束进程！
                        } else if (confirm == 'n' || confirm == 'N' || confirm == 27) { // 27=ESC
                            break; // Cancel exit. After breaking sub-loop, render_screen will be called to perfectly cover the warning!
                            // 放弃退出。跳出子循环后，外层紧接着会调用 render_screen，把警告完美覆盖掉！
                        }
                    }
                }
            }
        } 
        // ==========================================
        // 2. INSERT Mode (Input Mode)
        // INSERT 模式 (输入模式)
        // ==========================================
        else if (current_mode == MODE_INSERT) {
            
            // [ESC] Exit insert mode / 退出插入模式
            if (c == 27) { 
                current_mode = MODE_NORMAL;
                if (cursor_x > 0 && cursor_x == curr_line->length) cursor_x--;
            }
            
            // [Backspace] Delete character (most complex: cross-line merging)
            // 退格键 (最复杂的逻辑：跨行合并)
            else if (c == '\b') {
                if (cursor_x > 0) {
                    // 1. In-line backspace: shift characters after cursor left / 行内退格：把光标后面的字往前平移一格
                    for (int i = cursor_x - 1; i < curr_line->length; i++) {
                        curr_line->text[i] = curr_line->text[i+1];
                    }
                    curr_line->length--;
                    cursor_x--;
                    is_modified = 1;
                } 
                else if (curr_line->prev != 0) {
                    // 2. Line-start backspace: merge current line with previous line!
                    // 行首退格：当前行与上一行合并！
                    struct Line* prev_line = curr_line->prev;
                    int old_len = prev_line->length;
                    
                    // Append current line's text to previous line's end / 把当前行的字拼接到上一行末尾
                    for(int i = 0; i < curr_line->length; i++) {
                        if (prev_line->length < MAX_LINE_LEN - 1) {
                            prev_line->text[prev_line->length++] = curr_line->text[i];
                        }
                    }
                    prev_line->text[prev_line->length] = '\0';
                    
                    // Remove current line from doubly linked list / 把当前行从双向链表中摘除
                    prev_line->next = curr_line->next;
                    if (curr_line->next) curr_line->next->prev = prev_line;
                    
                    free(curr_line); // Free memory / 释放内存
                    
                    // Reposition cursor / 光标归位
                    curr_line = prev_line;
                    cursor_x = old_len;
                    if (screen_cursor_y > 0) screen_cursor_y--;
                    else row_offset--;
                    
                    is_modified = 1;
                }
            }
            
            // [Enter] New line (cross-line splitting)
            // 回车键 (跨行断裂)
            else if (c == '\n' || c == '\r') {
                struct Line* new_line = create_empty_line();
                
                // Move text after cursor to new line / 把光标后面的字，全部搬运到新行去
                int j = 0;
                for (int i = cursor_x; i < curr_line->length; i++) {
                    new_line->text[j++] = curr_line->text[i];
                }
                new_line->length = j;
                new_line->text[j] = '\0';
                
                // Truncate current line / 截断当前行
                curr_line->length = cursor_x;
                curr_line->text[cursor_x] = '\0';
                
                // Insert new line into doubly linked list / 把新行插入到双向链表中
                new_line->next = curr_line->next;
                if (curr_line->next) curr_line->next->prev = new_line;
                curr_line->next = new_line;
                new_line->prev = curr_line;
                
                // Move cursor to beginning of new line / 光标跳到新行的开头
                curr_line = new_line;
                cursor_x = 0;
                if (screen_cursor_y < 22) screen_cursor_y++;
                else row_offset++;
                
                is_modified = 1;
            }
            
            // Tab key converts to 4 spaces (Soft Tabs)
            // Tab 键转化为 4 个空格 (Soft Tabs)
            else if (c == '\t') {
                // Insert 4 spaces consecutively / 连续插入 4 个空格
                for (int k = 0; k < 4; k++) {
                    if (curr_line->length < MAX_LINE_LEN - 1) {
                        for (int i = curr_line->length; i > cursor_x; i--) {
                            curr_line->text[i] = curr_line->text[i-1];
                        }
                        curr_line->text[cursor_x] = ' ';
                        curr_line->length++;
                        curr_line->text[curr_line->length] = '\0';
                        cursor_x++;
                        is_modified = 1;
                    }
                }
            }
            
            // Normal typing (insert character)
            // 正常打字 (插入字符)
            else if (c >= 32 && c <= 126) {
                if (curr_line->length < MAX_LINE_LEN - 1) {
                    // 1. Shift characters after cursor right to make space / 把光标后面的字往后推一格，腾出空间
                    for (int i = curr_line->length; i > cursor_x; i--) {
                        curr_line->text[i] = curr_line->text[i-1];
                    }
                    // 2. Insert new character / 塞入新字符
                    curr_line->text[cursor_x] = c;
                    curr_line->length++;
                    curr_line->text[curr_line->length] = '\0';
                    cursor_x++;
                    is_modified = 1;
                }
            }
        }
        
        // After processing each key, refresh entire screen / 每次处理完按键，全量刷新屏幕
        render_screen();
    }
    // Cleanup when exiting editor / 退出编辑器时的清理工作
    clear(COLOR_DEFAULT);
    set_cursor(0, 0);
    set_color(COLOR_DEFAULT);
    exit(0);
    return 0;
}


// ==========================================
// Cursor Movement Core Algorithm (Compatible with Normal and Insert boundary)
// 光标移动核心算法 (兼容 Normal 和 Insert 边界)
// ==========================================


void move_left() {
    if (cursor_x > 0) {
        cursor_x--;
    } 
    // If at line start, continue left to jump to previous line's end
    // 如果在行首继续按左，跳回上一行的末尾
    else if (curr_line->prev != 0) { 
        curr_line = curr_line->prev;
        
        // Scroll up / 视图上滚
        if (screen_cursor_y > 0) screen_cursor_y--;
        else row_offset--;
        
        // Position cursor at previous line's end (different boundaries for Normal vs Insert)
        // 光标定位到上一行的末尾 (Normal 和 Insert 的边界差异)
        int max_x = (current_mode == MODE_NORMAL && curr_line->length > 0) ? curr_line->length - 1 : curr_line->length;
        cursor_x = max_x;
    }
    adjust_col_offset();
}

void move_right() {
    int max_x = (current_mode == MODE_NORMAL && curr_line->length > 0) ? curr_line->length - 1 : curr_line->length;
    
    if (cursor_x < max_x) {
        cursor_x++;
    } 
    // If at line end, continue right to jump to next line's beginning!
    // 如果在行尾继续按右，跳到下一行的开头！
    else if (curr_line->next != 0) {
        curr_line = curr_line->next;
        
        // Scroll down / 视图下滚
        if (screen_cursor_y < 22) screen_cursor_y++;
        else row_offset++;
        
        // Position cursor at next line's beginning / 光标定位到下一行的行首
        cursor_x = 0;
    }
    adjust_col_offset();
}
void move_up() {
    if (curr_line->prev != 0) {
        curr_line = curr_line->prev;
        if (screen_cursor_y > 0) screen_cursor_y--;
        else row_offset--;
        
        // X-axis clamping after cross-line movement / 跨行后的 X 轴约束 (Clamping)
        int max_x = (current_mode == MODE_NORMAL && curr_line->length > 0) ? curr_line->length - 1 : curr_line->length;
        if (cursor_x > max_x) cursor_x = max_x;
    }
    adjust_col_offset();
}

void move_down() {
    if (curr_line->next != 0) {
        curr_line = curr_line->next;
        if (screen_cursor_y < 22) screen_cursor_y++;
        else row_offset++;
        
        // X-axis clamping after cross-line movement / 跨行后的 X 轴约束 (Clamping)
        int max_x = (current_mode == MODE_NORMAL && curr_line->length > 0) ? curr_line->length - 1 : curr_line->length;
        if (cursor_x > max_x) cursor_x = max_x;
    }
    adjust_col_offset();
}


void page_up() {
    for(int i = 0; i < 20; i++) move_up();
}

void page_down() {
    for(int i = 0; i < 20; i++) move_down();
}
// ==========================================
// Horizontal Viewport Following Algorithm
// 视口横向跟随算法
// ==========================================
void adjust_col_offset() {
    if (cursor_x < col_offset) {
        col_offset = cursor_x; // Scroll left / 往左滑
    } else if (cursor_x >= col_offset + 80) {
        col_offset = cursor_x - 79; // Scroll right / 往右滑
    }
}

// ==========================================
// 2.5 File System Operation Engine
// 文件系统操作引擎
// ==========================================

// Load file content into doubly linked list
// 将文件内容加载到双向链表中
void load_file(char* filename) {
    int size = get_file_size(filename);
    
    // If file doesn't exist or is empty, create an empty line
    // 如果文件不存在，或者是个空文件，直接创建一个空行
    if (size <= 0) {
        head_line = create_empty_line();
        curr_line = head_line;
        return;
    }

    // Allocate buffer to read entire file / 申请一块内存把文件全部读进来
    char* buf = (char*)malloc(size);
    read_file(filename, buf);

    head_line = create_empty_line();
    struct Line* current = head_line;

    // Parse 1D string into 2D doubly linked list line by line
    // 逐字符解析，把一维的字符串切分成二维的双向链表
    for (int i = 0; i < size; i++) {
        if (buf[i] == '\n') {
            // Create new line and link when encountering newline / 遇到换行符，创建新行并链接
            struct Line* next_line = create_empty_line();
            current->next = next_line;
            next_line->prev = current;
            current = next_line;
        } else if (buf[i] != '\r') {
            // Normal character, insert into current line (prevent overflow)
            // 正常字符，塞入当前行 (注意防止越界)
            if (current->length < MAX_LINE_LEN - 1) {
                current->text[current->length++] = buf[i];
                current->text[current->length] = '\0'; // Maintain C string terminator / 保持C字符串结尾
            }
        }
    }
    free(buf);
    curr_line = head_line;
}

// Save doubly linked list content back to file
// 将双向链表内容保存回文件
void save_file() {
    // 1. Traverse linked list, calculate total buffer size needed / 遍历链表，计算总共需要多大的 Buffer
    int total_size = 0;
    struct Line* p = head_line;
    while (p) {
        total_size += p->length + 1; // +1 for '\n' / +1 是为了给 '\n' 留位置
        p = p->next;
    }

    // 2. Allocate buffer and concatenate strings / 申请 Buffer 并拼接字符串
    char* buf = (char*)malloc(total_size);
    int offset = 0;
    p = head_line;
    while (p) {
        memcpy(buf + offset, p->text, p->length);
        offset += p->length;
        buf[offset++] = '\n'; // Add newline after each line / 每行末尾加上换行符
        p = p->next;
    }

    // 3. Write to disk / 写入硬盘
    // If file doesn't exist, write_file will fail
    // So if it fails, we touch to create it, then write again
    // 如果文件不存在，write_file 会失败
    // 所以如果失败了，我们就先 touch 创建它，再写一次
    int ret = write_file(current_filename, buf, total_size);
    if (ret < 0) {
        touch(current_filename);
        write_file(current_filename, buf, total_size);
    }
    
    free(buf);
    is_modified = 0; // Clear modification flag / 清除修改标记
}