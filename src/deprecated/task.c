#include <task.h>  
#include <paging.h>
#include <common.h>
#include <monitor.h>
#include <kheap.h>

task_t *current_task;
task_t *ready_queue;

extern int npageDirs;

int next_pid = 1;

void tasking_init(){
    //disable interrupts
    asm volatile("cli");

    // monitor_printf("disable interrupts\n");
    // asm volatile("xchgw %bx, %bx");

    //initialize the ready queue
    ready_queue = (task_t*)kmalloc(sizeof(task_t));
    if(!ready_queue){
        PANIC("Could not allocate ready queue");
    }
    // ready_queue->prev = ready_queue;
    // ready_queue->next = ready_queue;

    // monitor_printf("malloc ready queue\n");
    // asm volatile("xchgw %bx, %bx");

    //initialize the current task
    current_task = (task_t*)kmalloc(sizeof(task_t));
    // monitor_printf("kmalloc call\n");
    // asm volatile ("xchgw %bx, %bx");
    if(!current_task){
        PANIC("Could not allocate current task");
    }

    // monitor_printf("malloc current task\n");
    // asm volatile("xchgw %bx, %bx");

    current_task->id = next_pid++;
    current_task->esp = current_task->ebp = 0;
    current_task->eip = 0;
    current_task->cr3 = current_directory->physicalAddr;
    current_task->page_directory = current_directory;
    // monitor_printf("current task setup\n");
    current_task->kernel_stack_start = kmalloc(KERNEL_STACK_SIZE);
    if(!current_task->kernel_stack_start){
        PANIC("Could not allocate kernel stack for current task");
    }
    // monitor_printf("malloc kernel stack\n");
    // asm volatile("xchgw %bx, %bx");
    current_task->user_stack_start = 0;
    current_task->user_stack_size = 0;
    current_task->user_heap = 0;
    current_task->prev = ready_queue;
    current_task->next = ready_queue;
    // monitor_printf("current task stack setup 2\n");
    // asm volatile("xchgw %bx, %bx");

    ready_queue->prev = current_task;
    ready_queue->next = current_task;
    // monitor_printf("ready queue setup\n");
    // asm volatile("xchgw %bx, %bx");

    //Reenable interrupts
    asm volatile("sti");
}

void tasking_create_task(uint32_t eip, int is_user){
    //disable interrupts
    asm volatile("cli");

    monitor_printf("clear interrupts\n");

    asm volatile("xchgw %bx, %bx");

    //get a new page table for the new task program
    alloc_new_page_table(current_directory, is_user, 1);
    //get a new page table for the new task heap/stack
    alloc_new_page_table(current_directory, is_user, 0);

    monitor_printf("alloc_new_page_table\n");

    asm volatile("xchgw %bx, %bx");

    //allocate a new task
    task_t *new_task = (task_t*)kmalloc(sizeof(task_t));
    if(!new_task){
        PANIC("Could not allocate new task_t");
    }
    new_task->id = next_pid++;
    new_task->esp = new_task->ebp = 0;
    new_task->eip = eip;
    if(current_directory){
        new_task->cr3 = current_directory->physicalAddr;
        new_task->page_directory = current_directory;
    } else {
        PANIC("No current directory set");
    }
    new_task->prev = ready_queue->prev;
    new_task->next = new_task;

    ready_queue->prev->next = new_task;
    ready_queue->prev = new_task;

    monitor_printf("new_task\n");

    asm volatile("xchgw %bx, %bx");

    //allocate a new kernel stack
    uint32_t kstack = kmalloc(KERNEL_STACK_SIZE);
    if(!kstack){
        // monitor_printf("kstack: %x\n", kstack);
        // asm volatile("xchgw %bx, %bx");
        PANIC("Could not allocate new stack for task_t");
    }
    new_task->kernel_stack_start = kstack;
    kstack += KERNEL_STACK_SIZE - 4;
    new_task->esp = kstack;

    monitor_printf("kernel_stack\n");

    asm volatile("xchgw %bx, %bx");

    //build user stack + heap
    if(is_user){
        //get stack location from page directory
        uint32_t stack_loc = current_directory->tables[npageDirs-1]->pages[0].frame << 12;

        new_task->user_stack_size = 0x10000; //64KB - 4 pages
        new_task->user_stack_start = stack_loc + 0x10000 - 4;
        if(!new_task->user_stack_start){
            PANIC("Could not allocate new user stack for task_t");
        }
        uint32_t page_dir_end = current_directory->tables[npageDirs-1]->pages[1023].frame << 12;
        new_task->user_heap = create_heap(stack_loc + 0x10000, page_dir_end, page_dir_end - (stack_loc + 0x10000), 0, 0);
    }
    else{
        new_task->user_stack_size = 0;
        new_task->user_stack_start = 0;
    }

    monitor_printf("user_stack + heap\n");

    asm volatile("xchgw %bx, %bx");

    //add the task to the end of the ready queue
    task_t *tmp = (task_t*)ready_queue;
    while(tmp->next){
        tmp = tmp->next;
    }
    tmp->next = new_task;

    ready_queue->prev = new_task;

    monitor_printf("ready_queue\n");

    asm volatile("xchgw %bx, %bx");

    //Reenable interrupts
    asm volatile("sti");
}

void tasking_switch(){
    //If we haven't initialized tasking yet, return
    if(!current_task){
        return;
    }

    //Read esp, ebp now for saving later on.
    uint32_t esp, ebp, eip;
    asm volatile("mov %%esp, %0" : "=r"(esp));
    asm volatile("mov %%ebp, %0" : "=r"(ebp));

    //Read the instruction pointer. We do some cunning logic here:
    //One of two things could have happened when this function exits -
    //1. We called the function and it returned the EIP as requested.
    //2. We have just switched tasks, and because the saved EIP is essentially the
    //instruction after (what would have been) the call to this function, it will seem as if
    //this function has just returned.
    //In the second case we need to return immediately. To detect it we put a dummy value in EAX
    //during the first call, and check to see if it's still the same value when the function returns.
    asm volatile("mov %%eax, %0" : "=r"(eip));

    //Have we just switched tasks?
    if(eip == 0){
        return;
    }

    //We have just switched tasks.
    current_task->eip = eip;
    current_task->esp = esp;
    current_task->ebp = ebp;

    //get the next task to run
    current_task = current_task->next;

    //If the next task is the same as the current task, we didn't actually switch tasks
    if(current_task == ready_queue){
        return;
    }

    //get the next task's esp, ebp, and eip
    esp = current_task->esp;
    ebp = current_task->ebp;
    eip = current_task->eip;

    //switch page directory
    current_directory = current_task->page_directory;
    asm volatile("mov %0, %%cr3":: "r"(current_directory->physicalAddr));

    //switch stacks
    asm volatile("mov %0, %%esp":: "r"(esp));
    asm volatile("mov %0, %%ebp":: "r"(ebp));

    //jump to the next task
    asm volatile("jmp %0":: "r"(eip));
}

void tasking_kill_task(int id){
    //disable interrupts
    asm volatile("cli");

    task_t *tmp = (task_t*)ready_queue;
    while(tmp->next){
        if(tmp->next->id == id){
            task_t *to_kill = tmp->next;
            tmp->next = to_kill->next;
            if(to_kill == ready_queue->prev){
                ready_queue->prev = tmp;
            }
            kfree(to_kill->kernel_stack_start);
            if(to_kill->user_stack_start){
                kfree(to_kill->user_stack_start);
            }
            kfree(to_kill);
            break;
        }
        tmp = tmp->next;
    }

    //Reenable interrupts
    asm volatile("sti");
}

void tasking_switch_task(int id){
    //disable interrupts
    asm volatile("cli");

    task_t *tmp = (task_t*)ready_queue;
    while(tmp->next){
        if(tmp->next->id == id){
            current_task = tmp->next;
            break;
        }
        tmp = tmp->next;
    }

    //Reenable interrupts
    asm volatile("sti");
}

void tasking_fork(){
    //disable interrupts
    asm volatile("cli");

    //allocate a new task
    task_t *new_task = (task_t*)kmalloc(sizeof(task_t));
    if(!new_task){
        PANIC("Could not allocate new task_t");
    }
    new_task->id = next_pid++;
    new_task->esp = new_task->ebp = 0;
    new_task->eip = current_task->eip;
    if(current_directory){
        new_task->cr3 = current_directory->physicalAddr;
        new_task->page_directory = current_directory;
    } else {
        PANIC("No current directory set");
    }
    new_task->prev = ready_queue->prev;
    new_task->next = new_task;

    ready_queue->prev->next = new_task;
    ready_queue->prev = new_task;

    //allocate a new kernel stack
    uint32_t kstack = kmalloc(KERNEL_STACK_SIZE);
    if(!kstack){
        PANIC("Could not allocate new stack for task_t");
    }
    new_task->kernel_stack_start = kstack;
    kstack += KERNEL_STACK_SIZE - 4;
    new_task->esp = kstack;

    //build user stack
    new_task->user_stack_size = current_task->user_stack_size;
    new_task->user_stack_start = current_task->user_stack_start;
    if(current_task->user_stack_start){
        uint32_t page_dir_end = current_directory->tables[npageDirs-1]->pages[1023].frame << 12;
        new_task->user_heap = create_heap(current_task->user_heap->start_address, page_dir_end, page_dir_end - current_task->user_heap->start_address, 0, 0);
    }

    //add the task to the end of the ready queue
    task_t *tmp = (task_t*)ready_queue;
    while(tmp->next){
        tmp = tmp->next;
    }
    tmp->next = new_task;

    ready_queue->prev = new_task;

    //Reenable interrupts
    asm volatile("sti");
}

int tasking_get_current_task_id(){
    return current_task->id;
}