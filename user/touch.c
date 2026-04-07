
#include "userlib.h"

int main(char *arg) {
    if (arg == 0 || arg[0] == '\0') {
        print(" Usage: touch <path>\n");
        exit(1);
        return 1;
    }

    int res = touch(arg);
    
    if (res == 0) {
        print(" File created successfully.\n");
    } else {
        print(" Failed to create file. Error code: ");
        if(res == -1) print("Invalid path!\n");
        if(res == -2) print("Parent not found!\n");
        if(res == -3) print("Parent not a directory!\n");
        if(res == -5) print("I don't know what's wrong\n");
    }
    
    exit(0);
    return 0;
}