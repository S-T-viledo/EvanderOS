; Example program demonstrating pointer and memory manipulation in EvanderOS
; 示例程序：演示 EvanderOS 中的指针和内存操作

; 1. 获取字符串的绝对地址，存入 EBX 
; Get the absolute address of the string and store it in EBX 
    MOV EBX, MSG

; 2. 准备我们要篡改的数据
; Prepare the data we want to modify
    ; 字符串 "Evander" 前四个字母 "Evan" 的 ASCII 码
    ; ASCII codes for the first four letters "Evan" of "Evander"
    ; 'H' (0x48), 'a' (0x61), 'c' (0x63), 'k' (0x6B)
    ; In little-endian x86, it's packed as 32-bit integer 0x6B636148
    MOV EAX, 0x6B636148

; 3. 内存写入！把 EAX 里的四个字节，强行写入 EBX 指向的内存！
; Memory write! Force the four bytes in EAX into the memory pointed by EBX!
    MOV [EBX], EAX

; 4. 打印字符串
; Print the string
    MOV EAX, 0      ; print syscall
    ; EBX still points to MSG, no need to reassign
    INT 0x30

; 5. 退出
; Exit
    MOV EAX, 1
    MOV EBX, 0
    INT 0x30

MSG:
    DB "EvanderOS Pointer Test!", 0