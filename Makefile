all:
	mkdir -p temp
	mkdir -p build
	$(VC4_GCC_PATH)/xgcc -O2 -B$(VC4_GCC_PATH) -iquote src src/boot.s src/main.c src/sdram.c src/sdcard.c -nostdlib -nostartfiles -o temp/main.elf
	$(VC4_BINUTILS_PATH)/objcopy -O binary temp/main.elf build/bootcode.bin