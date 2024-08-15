#include <stdint.h>
#include <isr.h>
#include <monitor.h>
#include <common.h>
#include <paging.h>

isr_t interrupt_handlers[256];

void irq_unmask(uint8_t IRQLine)
{
  uint16_t port;
  uint8_t value;

  if (IRQLine < 8)
    port = PIC1_DATA;
  else {
    port = PIC2_DATA;
    IRQLine -= 8;
  }
  value = inb(port) & ~(1 << IRQLine);
  outb(port, value);       
}

void register_interrupt_handler(uint8_t n, isr_t handler)
{
  irq_unmask(n - 32);
  interrupt_handlers[n] = handler;
} 

void gpf_handler(registers_t *regs)
{
  monitor_write("General Protection Fault\n");
  monitor_write("Error code: ");
  monitor_write_hex(regs->err_code);
  monitor_put('\n');
  monitor_write("EIP: ");
  monitor_write_hex(regs->eip);
  monitor_put('\n');
  monitor_write("CS: ");
  monitor_write_hex(regs->cs);
  monitor_put('\n');
  monitor_write("EFLAGS: ");
  monitor_write_hex(regs->eflags);
  monitor_put('\n');
  monitor_write("ESP: ");
  monitor_write_hex(regs->esp);
  monitor_put('\n');
  monitor_write("SS: ");
  monitor_write_hex(regs->ss);
  monitor_put('\n');
  for(;;);
}

// This gets called from our ASM interrupt handler stub.
void isr_handler(registers_t regs)
{
  if(regs.int_no == 14){
    page_fault(&regs);
  }
  else if(regs.int_no == 13){
    gpf_handler(&regs);
  }
  else{
    monitor_write("recieved interrupt (ISR): ");
    monitor_write_dec(regs.int_no);
    monitor_put('\n');
    for(;;);
  }
} 

// This gets called from our ASM interrupt handler stub.
void irq_handler(registers_t regs)
{
  // asm volatile("xchgw %bx, %bx");
  // Send an EOI (end of interrupt) signal to the PICs.
  // If this interrupt involved the slave.
  if (regs.int_no >= 40)
  {
    // Send reset signal to slave.
    outb(0xA0, 0x20);
  }
  // Send reset signal to master. (As well as slave, if necessary).
  outb(0x20, 0x20);

  if (interrupt_handlers[regs.int_no] != 0)
  {
    isr_t handler = interrupt_handlers[regs.int_no];
    handler(&regs);
    asm volatile("sti");
  }
} 