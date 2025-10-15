rm -rf build && mkdir -p build
x86_64-elf-gcc -c -m64 -ffreestanding -o build/boot.o src/boot.s
x86_64-elf-gcc -c -m64 -ffreestanding -O2 -o build/kernel.o src/kernel.c
x86_64-elf-ld -nostdlib -T src/linker.ld build/boot.o build/kernel.o -o build/kernel.elf
qemu-system-x86_64 -kernel build/kernel.elf -machine type=pc-i440fx-3.1
