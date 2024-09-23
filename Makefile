CC := x86_64-linux-gnu-gcc
AS := x86_64-linux-gnu-as

K_C_SRC := $(shell find kernel/ -type f -name '*.c')
K_ASM_SRC := $(shell find kernel/ -type f -name '*.asm')
K_OBJ = ${K_ASM_SRC:.asm=.o} ${K_C_SRC:.c=.o}
K_OBJ_B := $(K_OBJ:%=build/%)

K_H := $(shell find kernel/ -type f -name '*.h')

# development
	
run_kernel_i386: build/os.bin
	qemu-system-i386 -m 32M \
	-kernel build/os.bin \
	-serial stdio \
	-object filter-dump,id=f1,netdev=eth,file=qemu-pktlog.pcap \
	-netdev user,id=eth -device e1000,netdev=eth \
	-hda os.img
#	-nographic \
#	-chardev stdio,id=serial1 -device pci-serial,chardev=serial1 \

run_cdrom_i386: build/os.iso
	qemu-system-i386 -m 32M \
	-cdrom build/os.iso \
	-serial stdio \
	-hda os.img \
	-object filter-dump,id=f1,netdev=eth,file=qemu-pktlog.pcap \
	-netdev user,id=eth -device e1000,netdev=eth 

run_cdrom_vmware: build/os.iso
	vboxmanage startvm os

run_kernel_x86_64: build/os.bin
	qemu-system-x86_64 -s -m 32M \
	-kernel build/os.bin \
	-netdev user,id=eth -device e1000,netdev=eth 

run_cdrom_x86_64: build/os.iso
	qemu-system-x86_64 -s -m 32M \
	-cdrom build/os.iso \
	-netdev user,id=eth -device e1000,netdev=eth 


clean:
	rm -rf build/

clean_font:
	rm -f build/kernel/user/gui/font/font.o

# compiling

build/os.iso: build/os.bin
	mkdir -p build/iso/boot/grub
	cp build/os.bin build/iso/boot/os.bin
	cp grub.cfg build/iso/boot/grub/grub.cfg
	grub-mkrescue -o build/os.iso build/iso

build/os.bin: build/boot.o ${K_OBJ_B}
	$(CC) -T linker.ld -o build/os.bin build/boot.o $(K_OBJ_B) -m32 -lgcc -ffreestanding -O3 -nostdlib -Werror -Wall 

build/boot.o:
	@mkdir -p $(dir $@)
	$(AS) --32 boot/boot.s -o build/boot.o

build/%.o: %.asm
	@mkdir -p $(dir $@)
	nasm $< -f elf -o $@

build/%.o: %.c ${HEADERS}
	@mkdir -p $(dir $@)
	$(CC) $< -o $@ -g -c -m32 -ffreestanding -O3 -nostdlib -Wno-unused-variable -Werror -Wall

test: build_test/test.o ${K_OBJ_BT}
	$(CC) test.o $(K_OBJ_BT)