extern current_task
extern tss
global switch_to_task

global switch_to_first_task

section .text

;C declaration:
;   void switch_to_task(thread_control_block *next_thread);
;
;WARNING: Caller is expected to disable IRQs before calling, and enable IRQs again after function returns

switch_to_task:

    ;Save previous task's state

    ;Notes:
    ;  For cdecl; EAX, ECX, and EDX are already saved by the caller and don't need to be saved again
    ;  EIP is already saved on the stack by the caller's "CALL" instruction
    ;  The task isn't able to change CR3 so it doesn't need to be saved
    ;  Segment registers are constants (while running kernel code) so they don't need to be saved

    push ebx
    push esi
    push edi
    push ebp

    mov edi,[current_task]    ;edi = address of the previous task's "thread control block"
    mov [edi+4],esp         ;Save ESP for previous task's kernel stack in the thread's TCB

    ;Load next task's state

    mov esi,[esp+(4+1)*4]         ;esi = address of the next task's "thread control block" (parameter passed on stack)
    mov [current_task],esi    ;Current task's TCB is the next task TCB

    mov esp,[esi+4]         ;Load ESP for next task's kernel stack from the thread's TCB
    mov eax,[esi+8]         ;eax = address of page directory for next task
    mov ebx,[esi+0]        ;ebx = address for the top of the next task's kernel stack
    mov [tss + 4],ebx            ;Adjust the ESP0 field in the TSS (used by CPU for for CPL=3 -> CPL=0 privilege level changes)
    mov ecx,cr3                   ;ecx = previous task's virtual address space

    cmp eax,ecx                   ;Does the virtual address space need to being changed?
    je .doneVAS                   ; no, virtual address space is the same, so don't reload it and cause TLB flushes
    mov cr3,eax                   ; yes, load the next task's virtual address space
.doneVAS:

    pop ebp
    pop edi
    pop esi
    pop ebx

    sti
    ret                           ;Load next task's EIP from its kernel stack

switch_to_first_task:
    push ebx
    push esi
    push edi
    push ebp

    mov edi,[current_task]    ;edi = address of the previous task's "thread control block"

    ; Don't save the previous task's stack pointer, because there is no previous task
    ; mov [edi+4],esp         ;Save ESP for previous task's kernel stack in the thread's TCB

    ;Load next task's state

    mov esi,[esp+(4+1)*4]         ;esi = address of the next task's "thread control block" (parameter passed on stack)
    mov [current_task],esi    ;Current task's TCB is the next task TCB

    mov esp,[esi+4]         ;Load ESP for next task's kernel stack from the thread's TCB
    mov eax,[esi+8]         ;eax = address of page directory for next task
    mov ebx,[esi+0]        ;ebx = address for the top of the next task's kernel stack
    mov [tss + 4],ebx            ;Adjust the ESP0 field in the TSS (used by CPU for for CPL=3 -> CPL=0 privilege level changes)
    mov ecx,cr3                   ;ecx = previous task's virtual address space

    cmp eax,ecx                   ;Does the virtual address space need to being changed?
    je .doneVAS1                   ; no, virtual address space is the same, so don't reload it and cause TLB flushes
    mov cr3,eax                   ; yes, load the next task's virtual address space
.doneVAS1:

    pop ebp
    mov ebp, ebx
    pop edi
    pop esi
    pop ebx

    sti
    ret                           ;Load next task's EIP from its kernel stack

