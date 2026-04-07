#include "userlib.h"

// ---------------------------------------------------------
// 1. Basic Tools and Configuration
// 基础工具与配置
// ---------------------------------------------------------
// Load address for .BIN files / 加载 .BIN 文件时的基址

#define LOAD_ADDR 0x0

// Custom string search function / 自定义字符串查找函数
char* my_strchr(char* str, char c) {
    while (*str) { if (*str == c) return str; str++; }
    return 0;
}

// Smart number parser: precisely distinguish decimal and hexadecimal
// 智能字符串转数字：精准区分 10 进制和 16 进制
int atoi_hex(const char *str) {
    int res = 0;
    
    // 1. Check for 0x prefix for hexadecimal / 判断是否带有 0x 前缀的十六进制
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        str += 2; // Skip 0x / 跳过 0x
        while ((*str >= '0' && *str <= '9') || 
               (*str >= 'A' && *str <= 'F') || 
               (*str >= 'a' && *str <= 'f')) {
            int val;
            if (*str >= 'a') val = *str - 'a' + 10;
            else if (*str >= 'A') val = *str - 'A' + 10;
            else val = *str - '0';
            
            res = res * 16 + val;
            str++;
        }
        return res;
    }
    
    // 2. Otherwise, it's decimal / 否则，就是普通的十进制
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res;
}

// Check if string represents a number / 检查字符串是否表示数字
int is_number(const char* str) {
    return (str[0] >= '0' && str[0] <= '9');
}

// Get register ID from register name / 从寄存器名获取寄存器ID
int get_reg_id(const char* reg) {
    if (strcmp(reg, "EAX") == 0) return 0;
    if (strcmp(reg, "ECX") == 0) return 1;
    if (strcmp(reg, "EDX") == 0) return 2;
    if (strcmp(reg, "EBX") == 0) return 3;
    if (strcmp(reg, "ESP") == 0) return 4;
    if (strcmp(reg, "EBP") == 0) return 5;
    if (strcmp(reg, "ESI") == 0) return 6;
    if (strcmp(reg, "EDI") == 0) return 7;
    return -1;
}

// ---------------------------------------------------------
// 2. Symbol Table Management
// 符号表管理
// ---------------------------------------------------------
// Symbol structure / 符号结构体
struct Symbol {
    char name[32];
    int address;
};

// Global symbol table / 全局符号表
struct Symbol sym_table[64];
int sym_count = 0;

// Add symbol to table / 添加符号到表中
void add_symbol(char* name, int addr) {
    strcpy(sym_table[sym_count].name, name);
    sym_table[sym_count].address = addr;
    sym_count++;
}

// Get symbol address by name / 通过名称获取符号地址
int get_symbol_addr(char* name) {
    for (int i = 0; i < sym_count; i++) {
        if (strcmp(sym_table[i].name, name) == 0) return sym_table[i].address;
    }
    return 0; // Not found / 找不到
}

// ---------------------------------------------------------
// 3. Lexical Analysis (Lexer)
// 词法分析 (Lexer)
// ---------------------------------------------------------
// Parse instruction into mnemonic and operands
// 将指令解析为助记符和操作数
void parse_instruction(char *line, char *mnemonic, char *op1, char *op2) {
    mnemonic[0] = op1[0] = op2[0] = '\0';
    int state = 0, i = 0, j = 0;
    
    while (line[i] != '\0' && line[i] != ';') {
        if (line[i] == ' ' || line[i] == '\t' || line[i] == ',') {
            if (j > 0) {
                if (state == 0) mnemonic[j] = '\0';
                else if (state == 1) op1[j] = '\0';
                else if (state == 2) op2[j] = '\0';
                state++; j = 0;
            }
        } else {
            char c = line[i];
            if (c >= 'a' && c <= 'z') c -= 32; // Convert to uppercase / 转大写
            
            // j < 15 boundary check to prevent stack overflow!
            // j < 15 边界检查，防止栈溢出！
            if (j < 15) {
                if (state == 0) mnemonic[j++] = c;
                else if (state == 1) op1[j++] = c;
                else if (state == 2) op2[j++] = c;
            }
        }
        i++;
    }
    if (j > 0) {
        if (state == 0) mnemonic[j] = '\0';
        else if (state == 1) op1[j] = '\0';
        else if (state == 2) op2[j] = '\0';
    }
}

// Check if operand is [REG] memory addressing format
// 检查操作数是否是 [REG] 内存寻址格式
int parse_mem_operand(char *op, int *reg_id) {
    int len = strlen(op);
    if (len >= 3 && op[0] == '[' && op[len - 1] == ']') {
        char inner[16];
        for(int i = 0; i < len - 2; i++) inner[i] = op[i + 1];
        inner[len - 2] = '\0';
        *reg_id = get_reg_id(inner);
        return (*reg_id != -1);
    }
    return 0;
}


// Process DB "String", 0 (supports escape characters)
// 处理 DB "String", 0 (支持转义字符)
int process_db(char *line, unsigned char *buf) {
    int size = 0;
    char *p = my_strchr(line, '"');
    if (p) {
        p++; // 跳过左引号
        while (*p && *p != '"') {
            // 拦截并翻译转义字符
            if (*p == '\\' && *(p+1) != '\0') {
                p++; // 跳过反斜杠，看下一个字母
                char escaped_char;
                if (*p == 'n') escaped_char = '\n';      // 翻译为 0x0A
                else if (*p == 'r') escaped_char = '\r'; // 翻译为 0x0D
                else if (*p == 't') escaped_char = '\t'; // 翻译为 0x09
                else if (*p == '"') escaped_char = '"';  // 允许转义双引号
                else escaped_char = *p; // 不认识的转义，原样保留
                
                if (buf) buf[size] = escaped_char;
                size++;
            } else {
                // 普通字符，直接抄录
                if (buf) buf[size] = *p;
                size++;
            }
            p++;
        }
        p++; // 跳过右引号
    } else {
        p = line; 
    }
    
    // 检查后面有没有 ", 0"
    while (*p) {
        if (*p == '0' && *(p-1) != 'x' && *(p-1) != '\\') { 
            if (buf) buf[size] = 0;
            size++; break;
        }
        p++;
    }
    return size;
}

// Extract and skip labels tool
// 提取并跳过标签的工具
// Return value: real code part after colon / 返回值：冒号后面的真正代码部分
char* handle_label(char *line, int current_offset, int add_sym) {
    // 1. 先保护现场，防止查找到注释里的冒号
    char *comment = my_strchr(line, ';');
    if (comment) {
        *comment = '\0'; // 暂时把分号变结尾，屏蔽注释
    }

    char *colon = my_strchr(line, ':');
    char *rest = line;

    if (colon) {
        if (add_sym) {
            *colon = '\0';
            char label_name[32];
            int idx = 0;
            // 提取前面的标签名
            for(int k=0; line[k] != '\0'; k++) {
                if(line[k] != ' ') {
                    char c = line[k];
                    label_name[idx++] = (c >= 'a' && c <= 'z') ? c - 32 : c;
                }
            }
            label_name[idx] = '\0';
            add_symbol(label_name, current_offset);
            *colon = ':'; 
        }
        rest = colon + 1;
    }

    // 2. 恢复注释，不破坏原字符串
    if (comment) {
        *comment = ';';
    }

    while (*rest == ' ' || *rest == '\t') rest++;
    return rest;
}
// ---------------------------------------------------------
// 4. Core Assembly Engine (Main)
// 核心汇编引擎 (Main)
// ---------------------------------------------------------
int main(char *arg) {
    if (arg == 0 || arg[0] == '\0') { 
        print(" Usage: easm /IN.ASM [/OUT.BIN]\n"); 
        exit(1); 
    }

    // --- Dynamically parse input and output filenames --- / --- 动态解析输入和输出文件名 ---
    char in_file[64] = {0};
    char out_file[64] = "/OUT.BIN"; // Default output path / 默认输出路径

    char *space = my_strchr(arg, ' ');
    if (space) {
        *space = '\0'; // Truncate string / 截断字符串
        strcpy(in_file, arg);
        strcpy(out_file, space + 1); // Part after space is output file / 空格后面的是输出文件
    } else {
        strcpy(in_file, arg);
    }

    // Read input file / 读入文件时使用 in_file
    int size = get_file_size(in_file);
    if (size <= 0) { print(" Error: File not found.\n"); exit(1); }
    char *source_code = (char *)malloc(size + 1);
    read_file(in_file, source_code);
    source_code[size] = '\0';

    // ==========================================
    // Zero-copy line parser: directly modify original string to generate line pointer array
    // 零拷贝行解析器：直接在原字符串上修改，生成行指针数组
    // ==========================================
    char *lines[1024]; // We only store 1024 pointers, not data, saves memory / 我们只存 1024 个指针，不存数据，省内存
    int line_count = 0;
    
    char *p = source_code;
    while (*p != '\0') {
        // Skip leading whitespace / 跳过行首空白符
        while (*p == ' ' || *p == '\n' || *p == '\r') p++;
        if (*p == '\0') break;
        
        // Record current line's starting pointer / 记录当前行的起始指针
        lines[line_count++] = p;
        if (line_count >= 1024) {
            print(" Error: Source file too large (>1024 lines)!\n");
            exit(1);
        }
        
        // Go forward until newline / 往后找，直到遇到换行符
        while (*p != '\n' && *p != '\r' && *p != '\0') p++;
        
        // Directly modify newline to '\0', truncate string / 把换行符直接修改为 '\0'，截断字符串
        if (*p == '\r' && *(p+1) == '\n') {
            *p = '\0';
            p += 2;
        } else if (*p != '\0') {
            *p = '\0';
            p++;
        }
    }
    // =====================================
    // PASS 1: Calculate instruction lengths, collect symbols
    // PASS 1: 计算指令长度，收集符号
    // =====================================
    int current_offset = 0;
    for (int i = 0; i < line_count; i++) {
        char *code_part = handle_label(lines[i], current_offset, 1);
        
        char mnemonic[16], op1[16], op2[16];
        parse_instruction(code_part, mnemonic, op1, op2);
        
        if (mnemonic[0] == '\0') continue; // Skip empty lines / 空行跳过
        
        if (strcmp(mnemonic, "MOV") == 0) {
            int mem_reg;
            if (get_reg_id(op1) != -1 && get_reg_id(op2) != -1) current_offset += 2;
            else if (get_reg_id(op1) != -1 && parse_mem_operand(op2, &mem_reg)) current_offset += 2;
            else if (parse_mem_operand(op1, &mem_reg) && get_reg_id(op2) != -1) current_offset += 2;
            else if (get_reg_id(op1) != -1) current_offset += 5; 
        } 
        else if (strcmp(mnemonic, "ADD") == 0 || strcmp(mnemonic, "SUB") == 0 || strcmp(mnemonic, "CMP") == 0) {
            if (get_reg_id(op1) != -1 && get_reg_id(op2) != -1) current_offset += 2;
            else if (get_reg_id(op1) != -1) current_offset += 6; 
        }
        else if (strcmp(mnemonic, "INT") == 0) current_offset += 2;
        else if (strcmp(mnemonic, "DB") == 0)  current_offset += process_db(code_part, 0);
        else if (strcmp(mnemonic, "INC") == 0 || strcmp(mnemonic, "DEC") == 0) current_offset += 1; 
        else if (strcmp(mnemonic, "JMP") == 0 || strcmp(mnemonic, "CALL") == 0) {
            current_offset += 5;
        } 
        else if (strcmp(mnemonic, "JE") == 0 || strcmp(mnemonic, "JNE") == 0 || 
                 strcmp(mnemonic, "JZ") == 0 || strcmp(mnemonic, "JNZ") == 0) {
            current_offset += 6; 
        } 
        else if (strcmp(mnemonic, "LOOP") == 0) {
            current_offset += 2; 
        }
        else if (strcmp(mnemonic, "PUSH") == 0 || strcmp(mnemonic, "POP") == 0 || strcmp(mnemonic, "RET") == 0) {
            current_offset += 1; 
        }
    }
    
    // =====================================
    // PASS 2: Generate machine code
    // PASS 2: 生成机器码
    // =====================================
    unsigned char *bin_buffer = (unsigned char *)malloc(4096);
    int bin_size = 0;

    for (int i = 0; i < line_count; i++) {
        char *code_part = handle_label(lines[i], 0, 0);
        
        char mnemonic[16], op1[16], op2[16];
        parse_instruction(code_part, mnemonic, op1, op2);
        if (mnemonic[0] == '\0') continue;

        // Generate x86 machine code for each instruction / 为每条指令生成 x86 机器码
        if (strcmp(mnemonic, "MOV") == 0) {
            int reg1 = get_reg_id(op1);
            int reg2 = get_reg_id(op2);
            int mem_reg;
            
            if (reg1 != -1 && reg2 != -1) {
                // MOV reg, reg / MOV 寄存器, 寄存器
                bin_buffer[bin_size++] = 0x89; 
                bin_buffer[bin_size++] = 0xC0 | (reg2 << 3) | reg1; 
            } 
            else if (reg1 != -1 && parse_mem_operand(op2, &mem_reg)) {
                // MOV reg, [reg] / MOV 寄存器, [寄存器]
                if (mem_reg == 4 || mem_reg == 5) print(" Warning: [ESP]/[EBP] pointers need SIB byte!\n");
                bin_buffer[bin_size++] = 0x8B;
                bin_buffer[bin_size++] = 0x00 | (reg1 << 3) | mem_reg;
            }
            else if (parse_mem_operand(op1, &mem_reg) && reg2 != -1) {
                // MOV [reg], reg / MOV [寄存器], 寄存器
                if (mem_reg == 4 || mem_reg == 5) print(" Warning: [ESP]/[EBP] pointers need SIB byte!\n");
                bin_buffer[bin_size++] = 0x89;
                bin_buffer[bin_size++] = 0x00 | (reg2 << 3) | mem_reg;
            }
            else if (reg1 != -1) {
                // MOV reg, imm / MOV 寄存器, 立即数
                int imm = is_number(op2) ? atoi_hex(op2) : (get_symbol_addr(op2) + LOAD_ADDR);
                bin_buffer[bin_size++] = 0xB8 + reg1;
                bin_buffer[bin_size++] = (imm & 0xFF);
                bin_buffer[bin_size++] = ((imm >> 8) & 0xFF);
                bin_buffer[bin_size++] = ((imm >> 16) & 0xFF);
                bin_buffer[bin_size++] = ((imm >> 24) & 0xFF);
            }
        }
        else if (strcmp(mnemonic, "ADD") == 0 || strcmp(mnemonic, "SUB") == 0 || strcmp(mnemonic, "CMP") == 0) {
            int reg1 = get_reg_id(op1);
            int reg2 = get_reg_id(op2);
            if (reg1 != -1 && reg2 != -1) {
                if (strcmp(mnemonic, "ADD") == 0) bin_buffer[bin_size++] = 0x01;
                else if (strcmp(mnemonic, "SUB") == 0) bin_buffer[bin_size++] = 0x29;
                else bin_buffer[bin_size++] = 0x39; // CMP
                bin_buffer[bin_size++] = 0xC0 | (reg2 << 3) | reg1;
            } else if (reg1 != -1) {
                int imm = is_number(op2) ? atoi_hex(op2) : (get_symbol_addr(op2) + LOAD_ADDR);
                bin_buffer[bin_size++] = 0x81; 
                if (strcmp(mnemonic, "ADD") == 0) bin_buffer[bin_size++] = 0xC0 + reg1;
                else if (strcmp(mnemonic, "SUB") == 0) bin_buffer[bin_size++] = 0xE8 + reg1;
                else bin_buffer[bin_size++] = 0xF8 + reg1; // CMP
                bin_buffer[bin_size++] = (imm & 0xFF);
                bin_buffer[bin_size++] = ((imm >> 8) & 0xFF);
                bin_buffer[bin_size++] = ((imm >> 16) & 0xFF);
                bin_buffer[bin_size++] = ((imm >> 24) & 0xFF);
            }
        }
        else if (strcmp(mnemonic, "INT") == 0) {
            bin_buffer[bin_size++] = 0xCD;
            bin_buffer[bin_size++] = atoi_hex(op1);
        }
        else if (strcmp(mnemonic, "DB") == 0) {
            bin_size += process_db(code_part, &bin_buffer[bin_size]);
        }
        else if (strcmp(mnemonic, "INC") == 0 || strcmp(mnemonic, "DEC") == 0) {
            int reg_id = get_reg_id(op1);
            if (reg_id != -1) {
                bin_buffer[bin_size++] = (strcmp(mnemonic, "INC") == 0 ? 0x40 : 0x48) + reg_id;
            }
        }
        else if (strcmp(mnemonic, "JMP") == 0) {
            int target_addr = get_symbol_addr(op1); 
            int rel32 = target_addr - (bin_size + 5); 
            bin_buffer[bin_size++] = 0xE9; 
            bin_buffer[bin_size++] = (rel32 & 0xFF);
            bin_buffer[bin_size++] = ((rel32 >> 8) & 0xFF);
            bin_buffer[bin_size++] = ((rel32 >> 16) & 0xFF);
            bin_buffer[bin_size++] = ((rel32 >> 24) & 0xFF);
        }
        else if (strcmp(mnemonic, "JE") == 0 || strcmp(mnemonic, "JNE") == 0 || 
                 strcmp(mnemonic, "JZ") == 0 || strcmp(mnemonic, "JNZ") == 0) {
            int target_addr = get_symbol_addr(op1); 
            int rel32 = target_addr - (bin_size + 6); 
            
            bin_buffer[bin_size++] = 0x0F; 
            bin_buffer[bin_size++] = (strcmp(mnemonic, "JE") == 0 || strcmp(mnemonic, "JZ") == 0) ? 0x84 : 0x85;
            
            bin_buffer[bin_size++] = (rel32 & 0xFF);
            bin_buffer[bin_size++] = ((rel32 >> 8) & 0xFF);
            bin_buffer[bin_size++] = ((rel32 >> 16) & 0xFF);
            bin_buffer[bin_size++] = ((rel32 >> 24) & 0xFF);
        }
        else if (strcmp(mnemonic, "LOOP") == 0) {
            int target_addr = get_symbol_addr(op1); 
            int rel8 = target_addr - (bin_size + 2); 
            
            if (rel8 < -128 || rel8 > 127) {
                print("\n [Compiler Error] LOOP target '"); print(op1); print("' is out of range! (-128 to +127 bytes)\n");
                exit(1); 
            }
            bin_buffer[bin_size++] = 0xE2;
            bin_buffer[bin_size++] = (rel8 & 0xFF);
        }
        else if (strcmp(mnemonic, "PUSH") == 0 || strcmp(mnemonic, "POP") == 0) {
            int reg_id = get_reg_id(op1);
            if (reg_id != -1) {
                bin_buffer[bin_size++] = (strcmp(mnemonic, "PUSH") == 0 ? 0x50 : 0x58) + reg_id;
            } else {
                print(" Error: PUSH/POP needs a register!\n");
                // 占位以避免地址偏移！(如果代码写错了，最好是直接 exit(1)，或者塞个 1 字节垃圾)
                bin_buffer[bin_size++] = 0x90; // NOP
            }
        }
        else if (strcmp(mnemonic, "RET") == 0) {
            bin_buffer[bin_size++] = 0xC3;
        }
        else if (strcmp(mnemonic, "CALL") == 0) {
            int target_addr = get_symbol_addr(op1);
            int rel32 = target_addr - (bin_size + 5); 
            
            bin_buffer[bin_size++] = 0xE8; 
            bin_buffer[bin_size++] = (rel32 & 0xFF);
            bin_buffer[bin_size++] = ((rel32 >> 8) & 0xFF);
            bin_buffer[bin_size++] = ((rel32 >> 16) & 0xFF);
            bin_buffer[bin_size++] = ((rel32 >> 24) & 0xFF);
        }
    }

    // Write output file / 写入输出文件
    if (write_file(out_file, (char *)bin_buffer, bin_size) < 0) {
        touch(out_file); write_file(out_file, (char *)bin_buffer, bin_size);
    }

    print(" Success! Output: ");
    print(out_file);
    print("\n");
    free(source_code); free(bin_buffer);
    exit(0);
    return 0;
}