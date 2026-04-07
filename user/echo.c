#include "userlib.h"

int main(char *arg) {
    if (!arg || arg[0] == '\0') { exit(0); return 0; }

    char* file_path = arg;
    char* text = 0;
    
    // 解析语法：ECHO.BIN /PATH TEXT
    for(int i = 0; arg[i] != '\0'; i++) {
        if(arg[i] == ' ') {
            arg[i] = '\0';
            text = &arg[i+1];
            break;
        }
    }

    if (!text) {
        // 没提供文本，直接打印路径字符串
        print(file_path); print("\n");
    } else {
        // 先确保文件存在，然后写入
        touch(file_path); 
        write_file(file_path, text, strlen(text));
        print("Written.\n");
    }
    
    exit(0);
    return 0;
}