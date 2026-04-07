
#include "userlib.h"

int main(char *arg) {
    if (arg == 0 || arg[0] == '\0') {
        print(" Usage: rm <path>\n");
        exit(1);
        return 1;
    }

    int res = rm(arg);

    if (res == 0) {
        print(" File delete successfully.\n");
    } else {
        print(" Failed to delete the file. Error code: ");
        // 简单打印一下错误码 (你可能需要一个 itoa 函数，这里先简写)
        if(res == -1) print("Can't delete root\n");
        if(res == -2) print("Parent not found\n");
        if(res == -4) print("File not found\n");
    }

    exit(0);
    return 0;
}