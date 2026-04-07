[bits 32]
extern main
global _start

section .text
_start:
    push ebx        ; EBX 就是我们刚刚在内核里塞的参数地址，压栈传给 main!
    call main       ; 跳转到 C 语言的 main(char* arg)
    
    mov eax, 1      
    mov ebx, 0      
    int 0x30