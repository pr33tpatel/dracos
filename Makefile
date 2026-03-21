CC		= g++
AS 		= as
LD 		= ld

CFLAGS		 = -m32 -fno-use-cxa-atexit -nostdlib -fno-builtin  -fno-rtti -fno-exceptions  -Wno-write-strings -Iinclude
							# -fno-leading-underscore
							# -fno-rtti
ASFLAGS 	 = --32
LDFLAGS		 = -melf_i386

OBJECTS = obj/loader.o \
					obj/gdt.o \
					obj/memorymanagement.o \
					obj/drivers/driver.o \
					obj/hardwarecommunication/port.o \
					obj/hardwarecommunication/interruptstubs.o \
					obj/hardwarecommunication/interrupts.o \
					obj/drivers/timer.o \
					obj/ciu/ciu.o \
					obj/ciu/officer.o \
					obj/syscalls.o \
					obj/multitasking.o \
					obj/drivers/amd_am79c973.o \
					obj/hardwarecommunication/pci.o \
					obj/drivers/keyboard.o \
					obj/drivers/mouse.o \
					obj/drivers/terminal.o \
					obj/drivers/vga.o \
					obj/drivers/ata.o \
					obj/gui/widget.o \
					obj/gui/window.o \
					obj/gui/desktop.o \
					obj/net/etherframe.o \
					obj/net/arp.o \
					obj/net/ipv4.o \
					obj/net/icmp.o \
					obj/net/udp.o \
					obj/utils/print.o \
					obj/utils/string.o \
					obj/utils/math.o \
					obj/cli/shell.o \
					obj/cli/commandregistry.o \
					obj/cli/commands/networkCmds.o \
					obj/cli/commands/systemCmds.o \
					obj/kernel.o \


all: mykernel.bin

obj/%.o: src/%.cc
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

obj/%.o: src/%.s
	mkdir -p $(@D)
	$(AS) $(ASFLAGS) -c $< -o $@

mykernel.bin: src/linker.ld $(OBJECTS)
	$(LD) $(LDFLAGS) -T src/linker.ld -o $@ $(OBJECTS)

install: mykernel.bin
	sudo cp $< /boot/mykernel.bin

mykernel.iso: mykernel.bin
	mkdir iso
	mkdir iso/boot
	mkdir iso/boot/grub
	cp mykernel.bin iso/boot/mykernel.bin
	echo 'set timeout=0'                      > iso/boot/grub/grub.cfg
	echo 'set default=0'                     >> iso/boot/grub/grub.cfg
	echo ''                                  >> iso/boot/grub/grub.cfg
	echo 'menuentry "My Operating System" {' >> iso/boot/grub/grub.cfg
	echo '  multiboot /boot/mykernel.bin'    >> iso/boot/grub/grub.cfg
	echo '  boot'                            >> iso/boot/grub/grub.cfg
	echo '}'                                 >> iso/boot/grub/grub.cfg
	grub-mkrescue --output=mykernel.iso iso
	rm -rf iso

Image.img:
	qemu-img create -f raw Image.img 128M

run: mykernel.iso Image.img
	qemu-system-i386 \
		-boot d \
		-cdrom mykernel.iso \
		-m 512 \
		-smp 1 \
		-net nic,model=pcnet -net user \
		-drive id=disk,file=Image.img,format=raw,if=ide,index=0 \
		-vga qxl 
		# -device piix4-ide,id=piix4 -device ide-hd,drive=disk,bus=piix4.0


kernel-debug: mykernel.bin
	qemu-system-i386 -kernel mykernel.bin -no-reboot -no-shutdown -serial stdio -d cpu,int

iso: mykernel.bin grub.cfg
	mkdir -p iso/boot/grub
	cp mykernel.bin iso/boot/
	cp grub.cfg iso/boot/grub/
	grub-mkrescue -o myos.iso iso

run-iso: iso
	qemu-system-i386 -cdrom myos.iso


.PHONY: clean
clean:
	rm -rf obj mykernel.bin mykernel.iso Image.img
clean-objects:
	rm -rf obj 
