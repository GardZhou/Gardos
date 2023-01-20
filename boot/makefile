AS = nasm
ASFLAGS = -I include/ 


./mbr.bin: mbr.asm
	$(AS) $(ASFLAGS) $^ -o $@
./loader.bin: loader.asm
	$(AS) $(ASFLAGS) $^ -o $@


.PHONY: hd build all

hd:
	dd if=./mbr.bin \
		of=/home/gming/bochs/hd60M.img \
		bs=512 count=1 conv=notrunc

	dd if=./loader.bin \
		of=/home/gming/bochs/hd60M.img \
		bs=512 count=1 seek=2 conv=notrunc


build:./mbr.bin  ./loader.bin

all: build hd 
