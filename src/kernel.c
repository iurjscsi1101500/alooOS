#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif
size_t strlen(const char* str) 
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

#define HEAP_SIZE (1024 * 1024)
#define ALIGN(size) (((size) + 7) & ~((size_t)7))

static uint8_t heap[HEAP_SIZE] __attribute__((aligned(8)));

typedef struct block_header {
    size_t size;
    struct block_header* next;
} block_header_t;

static block_header_t* free_list = NULL;

void heap_initialize(void) {
    free_list = (block_header_t*)heap;
    free_list->size = HEAP_SIZE;
    free_list->next = NULL;
}

void* malloc(size_t size) {
    if (size == 0) return NULL;
    size = ALIGN(size);
    size_t required = size + sizeof(block_header_t);

    block_header_t* prev = NULL;
    block_header_t* curr = free_list;

    while (curr) {
        size_t curr_size = curr->size & ~(size_t)1;

        if (curr_size >= required) {
            size_t remainder = curr_size - required;

            if (remainder > sizeof(block_header_t)) {
                block_header_t* next_block = (block_header_t*)((uint8_t*)curr + required);
                next_block->size = remainder;
                next_block->next = curr->next;

                if (prev) {
                    prev->next = next_block;
                } else {
                    free_list = next_block;
                }

                curr->size = required | 1;
            } else {
                if (prev) {
                    prev->next = curr->next;
                } else {
                    free_list = curr->next;
                }
                curr->size = curr_size | 1;
            }

            return (void*)(curr + 1);
        }

        prev = curr;
        curr = curr->next;
    }

    return NULL;
}

void free(void* ptr) {
    if (!ptr) return;

    block_header_t* block = (block_header_t*)ptr - 1;
    block->size &= ~(size_t)1;

    block->next = free_list;
    free_list = block;
}

uint8_t inb(uint16_t port) {
    uint8_t result;
    asm volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

void wait_for_input_buffer_clear() {
    while (inb(0x64) & 0x02);
}

void wait_for_output_buffer() {
    while (!(inb(0x64) & 0x01));
}

void ps2_write_command(uint8_t command) {
    wait_for_input_buffer_clear();
    outb(0x60, command);
}

uint8_t ps2_read() {
    wait_for_output_buffer();
    return inb(0x60);
}

void ps2_keyboard_init() {
    ps2_write_command(0xF4);
}

void itoa(int num, char* str, int base) {
    int i = 0;
    bool is_negative = false;
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    if (num < 0 && base == 10) {
        is_negative = true;
        num = -num;
    }
    while (num != 0) {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }
    if (is_negative) {
        str[i++] = '-';
    }
    str[i] = '\0';
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

void delay_ms(uint32_t ms) {
    uint32_t divisor = 1193180 / 1000;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
    for (uint32_t i = 0; i < ms; i++) {
        while ((inb(0x61) & 0x01) == 0);
    }
}

void kernel_main() {
    heap_initialize();
    ps2_keyboard_init();
}
