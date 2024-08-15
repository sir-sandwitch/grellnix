#include <task.h>
#include <stdint.h>
#include <paging.h>
#include <common.h>
#include <kheap.h>

int next_pid = 1;

TaskArray *task_array;

task_t* current_task;

extern uint32_t read_eip();
extern void perform_task_switch(uint32_t, uint32_t, uint32_t, uint32_t);

extern uint32_t initial_esp;

extern heap_t *kheap;

extern page_directory_t *kernel_directory;

extern uint32_t placement_address;

extern void switch_tasks(task_t* next_task);

void resize_task_array(TaskArray *arr, size_t new_capacity)
{
    task_t *new_tasks = (task_t *)kmalloc(new_capacity * sizeof(task_t));
    for (size_t i = 0; i < arr->size; i++)
    {
        new_tasks[i] = arr->tasks[i];
    }
    kfree(arr->tasks);
    arr->tasks = new_tasks;
    arr->capacity = new_capacity;
}

TaskArray *create_task_array(size_t capacity)
{
    TaskArray *arr = (TaskArray *)kmalloc(sizeof(TaskArray));
    arr->tasks = (task_t *)kmalloc(capacity * sizeof(task_t));
    arr->capacity = capacity;
    arr->size = 0;
    return arr;
}

void destroy_task_array(TaskArray *arr)
{
    kfree(arr->tasks);
    kfree(arr);
}

void add_task(TaskArray *arr, task_t task)
{
    if (arr->size >= arr->capacity)
    {
        resize_task_array(arr, arr->capacity + 3);
    }
    arr->tasks[arr->size++] = task;
}

void remove_task(TaskArray *arr, size_t index)
{
    for (size_t i = index; i < arr->size - 1; i++)
    {
        arr->tasks[i] = arr->tasks[i + 1];
    }
    arr->size--;
}

task_t *get_task(TaskArray *arr, size_t index)
{
    return &arr->tasks[index];
}

// //set kernel stack of a task
// void set_kernel_stack(uint32_t esp, uint32_t ebp) {
    
// }

// void move_stack(void *new_stack_start, uint32_t size)
// {
//     uint32_t i;
//     // Allocate some space for the new stack.
//     for( i = (uint32_t)new_stack_start;
//         i >= ((uint32_t)new_stack_start-size);
//         i -= 0x1000)
//     {
//         // General-purpose stack is in user-mode.
//         alloc_frame( get_page(i, 1, current_directory), 0 /* User mode */, 1 /* Is writable */ );
//     } 
//       // Flush the TLB by reading and writing the page directory address again.
//     uint32_t pd_addr;
//     asm volatile("mov %%cr3, %0" : "=r" (pd_addr));
//     asm volatile("mov %0, %%cr3" : : "r" (pd_addr)); 
//     // Old ESP and EBP, read from registers.
//     uint32_t old_stack_pointer; asm volatile("mov %%esp, %0" : "=r" (old_stack_pointer));
//     uint32_t old_base_pointer;  asm volatile("mov %%ebp, %0" : "=r" (old_base_pointer));
//     uint32_t offset            = (uint32_t)new_stack_start - initial_esp;
//     uint32_t new_stack_pointer = old_stack_pointer + offset;
//     uint32_t new_base_pointer  = old_base_pointer + offset;
//     // Copy the stack.
//     memcpy((void*)new_stack_pointer, (void*)old_stack_pointer, initial_esp-old_stack_pointer);
//     // Backtrace through the original stack, copying new values into the new stack.
//     for(i = (uint32_t)new_stack_start; i > (uint32_t)new_stack_start-size; i -= 4)
//     {
//         uint32_t tmp = * (uint32_t*)i;
//         // If the value of tmp is inside the range of the old stack, assume it is a base pointer
//         // and remap it. This will unfortunately remap ANY value in this range, whether they are
//         // base pointers or not.
//         if (( old_stack_pointer < tmp) && (tmp < initial_esp))
//         {
//             tmp = tmp + offset;
//             uint32_t *tmp2 = (uint32_t*)i;
//             *tmp2 = tmp;
//         }
//     }
//     // Change stacks.
//     asm volatile("mov %0, %%esp" : : "r" (new_stack_pointer));
//     asm volatile("mov %0, %%ebp" : : "r" (new_base_pointer));
// }

// void fork()
// {
//     task_t *parent_task = get_task(task_array, 0);
//     task_t child_task = *parent_task;
//     child_task.page_table = clone_page_table(parent_task->page_table);
//     add_task(task_array, child_task);
// }

// Bad code

// void exec(void (*entry)())
// {
//     task_t *parent_task = get_task(task_array, next_pid - 1 % task_array->size);
//     parent_task->eip = (uint32_t)entry;

//     // share kernel stack
//     parent_task->esp = parent_task->esp;
//     parent_task->ebp = parent_task->ebp;

//     // Set up EIP (instruction pointer) to the entry function.
//     uint32_t eip = (uint32_t)entry;

//     // Jump to the entry function.
//     asm volatile("jmp *%0" : : "r"(eip));
// }

void create_new_task(void (*entry)(), uint8_t user) {
    if (!task_array) {
        PANIC("task_array is null");
    }

    page_directory_t *new_directory;
    if (user) {
        // Create new page directory for user tasks
        new_directory = create_new_page_directory(1);
    } else {
        // Use the kernel's page directory for kernel tasks
        new_directory = kernel_directory;
    }

    task_t task;
    task.id = task_array->size;
    task.eip = (uint32_t)entry;
    task.esp = task.ebp = 0;
    task.page_directory = new_directory;

    if (user) {
        // Create user heap
        task.heap = create_heap(0x100000, 0x1000000, 0xCFFFF000, 0, 0);
    } else {
        // Use kernel heap for kernel tasks
        task.heap = kheap;
    }

    // Allocate kernel stack
    uint32_t stack_base = alloc(KERNEL_STACK_SIZE, 1, task.heap);
    task.kernel_stack = virtual_address_to_physical(stack_base + KERNEL_STACK_SIZE, new_directory);

    // Check if allocation was successful
    if (task.kernel_stack == 0) {
        PANIC("Failed to allocate kernel stack");
    }

    task.cr3 = new_directory->physicalAddr;

    // Initialize esp and ebp to the top of the kernel stack
    task.esp = task.ebp = stack_base + KERNEL_STACK_SIZE;

    uint32_t* stack_ptr = (uint32_t*)task.esp;
    // Push initial values for EBP, EDI, ESI, EBX onto the stack
    *(--stack_ptr) = 0; // EBP
    *(--stack_ptr) = 0; // EDI
    *(--stack_ptr) = 0; // ESI
    *(--stack_ptr) = 0; // EBX

    // Update task's ESP to the current stack pointer
    task.esp = (uint32_t)stack_ptr;

    // Add the new task to the task array
    add_task(task_array, task);
}


void switch_task(){
    // //return if only one task is present or tasking is not initialized
    if(!task_array){
        // monitor_printf("task_array is null\n");
        return;
    }
    if(task_array->size <= 1){
        // monitor_printf("task_array size is less than 1\n");
        return;
    }

    // Get the current task and the next task.
    current_task = get_task(task_array, next_pid);
    if(++next_pid >= task_array->size){
        next_pid = 0;
    }
    task_t *next_task = get_task(task_array, next_pid);

    // monitor_printf("task cr3: %x\n", current_task->cr3);
    // monitor_printf("next task cr3: %x\n", next_task->cr3);
    // asm volatile("xchgw %bx, %bx");

    switch_tasks(next_task);
    // monitor_printf("switched tasks\n");
}

void idle_task()
{
    while (1)
    {
        // Do nothing.
    }
}

void initialise_tasking()
{
    task_array = create_task_array(1);
    create_new_task(idle_task, 0);
}