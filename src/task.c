#include <task.h>
#include <stdint.h>
#include <paging.h>
#include <common.h>
#include <kheap.h>

int next_pid = 1;

TaskArray *task_array;

extern uint32_t read_eip();
extern void perform_task_switch(uint32_t, uint32_t, uint32_t, uint32_t);

extern uint32_t initial_esp;

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

//set esp and ebp to the appropriate values
void set_kernel_stack(uint32_t esp, uint32_t ebp)
{
    uint32_t return_address;
    //return back to the kernel root stack
    asm volatile("mov %%eax, %0" : "=r"((uint32_t)&return_address));
}

void move_stack(void *new_stack_start, uint32_t size)
{
    uint32_t i;
    // Allocate some space for the new stack.
    for( i = (uint32_t)new_stack_start;
        i >= ((uint32_t)new_stack_start-size);
        i -= 0x1000)
    {
        // General-purpose stack is in user-mode.
        alloc_frame( get_page(i, 1, current_directory), 0 /* User mode */, 1 /* Is writable */ );
    } 
      // Flush the TLB by reading and writing the page directory address again.
    uint32_t pd_addr;
    asm volatile("mov %%cr3, %0" : "=r" (pd_addr));
    asm volatile("mov %0, %%cr3" : : "r" (pd_addr)); 
    // Old ESP and EBP, read from registers.
    uint32_t old_stack_pointer; asm volatile("mov %%esp, %0" : "=r" (old_stack_pointer));
    uint32_t old_base_pointer;  asm volatile("mov %%ebp, %0" : "=r" (old_base_pointer));
    uint32_t offset            = (uint32_t)new_stack_start - initial_esp;
    uint32_t new_stack_pointer = old_stack_pointer + offset;
    uint32_t new_base_pointer  = old_base_pointer + offset;
    // Copy the stack.
    memcpy((void*)new_stack_pointer, (void*)old_stack_pointer, initial_esp-old_stack_pointer);
    // Backtrace through the original stack, copying new values into the new stack.
    for(i = (uint32_t)new_stack_start; i > (uint32_t)new_stack_start-size; i -= 4)
    {
        uint32_t tmp = * (uint32_t*)i;
        // If the value of tmp is inside the range of the old stack, assume it is a base pointer
        // and remap it. This will unfortunately remap ANY value in this range, whether they are
        // base pointers or not.
        if (( old_stack_pointer < tmp) && (tmp < initial_esp))
        {
            tmp = tmp + offset;
            uint32_t *tmp2 = (uint32_t*)i;
            *tmp2 = tmp;
        }
    }
    // Change stacks.
    asm volatile("mov %0, %%esp" : : "r" (new_stack_pointer));
    asm volatile("mov %0, %%ebp" : : "r" (new_base_pointer));
}

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

void create_new_task(void (*entry)())
{
    task_t task;
    task.id = task_array->size;
    task.eip = (uint32_t)entry;
    task.esp = task.ebp = 0;
    task.page_table = create_new_page_table(current_directory, 1, 1);
    task.kernel_stack = virtual_address_to_physical(kmalloc(KERNEL_STACK_SIZE) + KERNEL_STACK_SIZE, kernel_directory);
    task.heap = create_heap(task.page_table->pages[0].frame, task.page_table->pages[1023].frame - task.page_table->pages[0].frame, 0xCFFFF000, 0, 0);

    //initialize esp and ebp
    task.esp = task.ebp = task.kernel_stack;

    add_task(task_array, task);
}

void switch_task(){
    //return if only one task is present or tasking is not initialized
    if(!task_array){
        // monitor_printf("task_array is null\n");
        return;
    }
    if(task_array->size <= 1){
        // monitor_printf("task_array size is less than 1\n");
        return;
    }

    // save the current esp and ebp
    uint32_t esp, ebp;

    asm volatile("mov %%esp, %0" : "=r"(esp));
    asm volatile("mov %%ebp, %0" : "=r"(ebp));

    monitor_printf("esp: %x, ebp: %x\n", esp, ebp);
    asm volatile("xchgw %bx, %bx");

    // Get the current task and the next task.
    task_t *current_task = get_task(task_array, next_pid);
    if(++next_pid >= task_array->size){
        next_pid = 0;
    }
    task_t *next_task = get_task(task_array, next_pid);

    monitor_printf("current_task: %x, next_task: %x\n", current_task, next_task);
    asm volatile("xchgw %bx, %bx");

    // Set the kernel stack of the next task.
    set_kernel_stack(next_task->esp, next_task->ebp);

    // monitor_printf("next_task->kernel_stack: %x\n", next_task->kernel_stack);
    // asm volatile("xchgw %bx, %bx");

    // Read the instruction pointer. We do some cunning logic here:
    // One of two things could have happened when this function exits -
    // (a) We called the function and it returned the EIP as requested.
    // (b) We have just switched tasks, and because the saved EIP is essentially
    // the instruction after read_eip(), it will seem as if read_eip has just
    // returned.
    // In the second case we need to return immediately. To detect it we put a dummy
    // value in EAX further down at the end of this function. As C returns values in EAX,
    // it will look like the return value is this dummy value! (0x12345).
    monitor_printf("before read_eip\n");
    uint32_t eip = read_eip();
    monitor_printf("after read_eip\n");

    monitor_printf("eip: %x\n", eip);
    asm volatile("xchgw %bx, %bx");

    // Have we just switched tasks?
    if (eip == 0x12345){
        monitor_printf("switched tasks\n");
        asm volatile("xchgw %bx, %bx");
        return; 
    }

    // save values and switch tasks, we didnt last time.
    current_task->eip = eip;
    current_task->esp = esp;
    current_task->ebp = ebp;

    // switch tasks
    perform_task_switch(next_task->eip, current_directory->physicalAddr, ebp, esp);
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
    create_new_task(idle_task);
}