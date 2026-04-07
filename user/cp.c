#include "userlib.h"

int main(char *arg) {
    // 如果用户没传参数，直接报错退出
    if (arg == 0 || arg[0] == '\0') { 
        print("Usage: cp <src> <dest>\n"); 
        exit(1); 
        return 1;
    }
    char* src = arg;
    char* dest = 0;
    char *file_buffer = malloc(65536);
    if (!file_buffer) {
        print("Error: Malloc failed!\n");
        exit(1);
    }
    for(int i = 0; arg[i] != '\0'; i++) {
        if(arg[i] == ' ') {
            arg[i] = '\0'; dest = &arg[i+1]; break;
        }
    }
    if (!dest) { print("Usage: cp <src> <dest>\n"); exit(1); }

    int size = get_file_size(src);
    if (size < 0) { print("Source file error.\n"); exit(1); }
    if (size > 65536) { print("File too large for CP.\n"); exit(1); }

    // 读数据 -> 建新文件 -> 写数据
    read_file(src, file_buffer);
    touch(dest);
    write_file(dest, file_buffer, size);
    
    print("Copy completed.\n");
    free(file_buffer);
    exit(0);
    return 0;
}