SOURCES=boot.o main.o monitor.o common.o descriptor_tables.o isr.o interrupt.o gdt.o timer.o page.o pagehelper.o ordered_map.o fs.o initrd.o kheap.o task.o task_helper.o

CFLAGS=-ffreestanding -fno-stack-protector -Iinclude
LDFLAGS=-Tlink.ld
ASFLAGS=-f elf32

default: run

all: $(SOURCES) link

clean:
	-rm *.o kernel

link:
	i686-elf-ld $(LDFLAGS) -o kernel $(SOURCES)

.s.o:
	nasm $(ASFLAGS) $< -o $@

.c.o:
	i686-elf-gcc $(CFLAGS) -c $< -o $@

iso: all
	mkdir -p iso/boot/grub
	cp kernel iso/boot/kernel
	cp grub.cfg iso/boot/grub/grub.cfg
	cd ../initrd && make
	cd ../initrd && ./generateImage test.txt test.txt test2.txt test2.txt
	cp ../initrd/initrd.img iso/boot/initrd.img
	grub-mkrescue -o bin/kernel.iso iso

run: iso
	bochs -f bochsrc.txt