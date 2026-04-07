
#include "userlib.h"

int main(char *arg) {
    if (arg == 0 || arg[0] == '\0') {
        print(" Usage: mkdir <path>\n");
        exit(1);
        return 1;
    }

    int res = mkdir(arg);

    if (res == 0) {
        print(" Directory created successfully.\n");
    } else {
        print(" Failed to create directory. Error code: ");
        // 简单打印一下错误码 (你可能需要一个 itoa 函数，这里先简写)
        if(res == -1) print("Invalid path\n");
        if(res == -2) print("Parent not found\n");
        if(res == -4) print("Disk full\n");
    }

    exit(0);
    return 0;
}