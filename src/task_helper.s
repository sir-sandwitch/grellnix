section .text
    ; [GLOBAL read_eip]
    ; read_eip:
    ;     mov eax, [esp] ; get the return address from the top of the stack
    ;     ret ; return, leaving the control flow unchanged
    ;     ; pop eax
    ;     ; ret

    ; Here we:
    ; * Stop interrupts so we don't get interrupted.
    ; * Temporarily put the new EIP location in ECX.
    ; * Temporarily put the new page directory's physical address in EAX.
    ; * Set the base and stack pointers
    ; * Set the page directory
    ; * Put a dummy value (0x12345) in EAX so that above we can recognize that we've just
    ;   switched task.
    ; * Restart interrupts. The STI instruction has a delay - it doesn't take effect until after
    ;   the next instruction.
    ; * Jump to the location in ECX (remember we put the new EIP in there).

    [GLOBAL perform_task_switch]
    perform_task_switch:
        cli;
        mov ecx, [esp+4]   ; EIP
        mov eax, [esp+8]   ; physical address of current directory
        mov ebp, [esp+12]  ; EBP
        mov esp, [esp+16]  ; ESP
        mov cr3, eax       ; set the page directory
        ; mov eax, 0x12345   ; magic number to detect a task switch
        ; push eax           ; push dummy value
        sti;
        jmp ecx