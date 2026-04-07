
#include "userlib.h"

int main(char *arg) {
    // 检查是否接收到了参数 (例如 "/BIN")
    if (arg != 0 && arg[0] != '\0') {
        // print("LS target: ");
        // print(arg);
        // print("\n");
        ls(arg); // 把目标路径传给内核
    } else {
        // 无参数，读取当前根目录
        ls("/");
    }

    exit(0);
    return 0;
}