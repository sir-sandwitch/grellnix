; functions relating to paging.

section .text
    global load_page_directory

load_page_directory:
    mov eax, [esp + 4] ; get the address of the page directory
    mov cr3, eax ; load it into cr3
    ; xchg bx, bx ; do nothing
    mov eax, cr0 ; get the value of cr0
    or eax, 0x80000000 ; set the paging bit
    mov cr0, eax ; load it back into cr0
    ret ; return