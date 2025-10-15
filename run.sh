mkdir build/
i686-elf-as src/boot.s -o build/boot.o
i686-elf-gcc -c src/kernel.c -o build/kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
i686-elf-ld -T src/linker.ld -o build/kernel.elf build/boot.o build/kernel.o -nostdlib
qemu-system-x86_64 -kernel build/kernel.elf
rm -rf build/
