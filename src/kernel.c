#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

size_t strlen(const char* str) 
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

// memory shit
#define HEAP_SIZE (1024 * 1024)
#define ALIGN(size) (((size) + 7) & ~((size_t)7))

uint8_t heap[HEAP_SIZE] __attribute__((aligned(8)));

typedef struct block_header {
    size_t size;
    struct block_header* next;
} block_header_t;

block_header_t* free_list = NULL;

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

void memcpy(void *to, void *from, size_t size) {
    uint8_t *cfrom = (uint8_t *)from;
    uint8_t *cto = (uint8_t *)to;

    for (size_t i = 0; i < size; i++) {
	cto[i] = cfrom[i];
    }
}
/*
 * I/O
 */
uint8_t inb(uint16_t port) {
    uint8_t result;
    asm volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

uint16_t inw(uint16_t port) {
    uint16_t val;
    asm volatile("inw %w1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

uint32_t inl(uint16_t port) {
    uint32_t val;
    asm volatile("inl %w1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %w1" : : "a"(val), "Nd"(port));
}

void outw(uint16_t port, uint16_t val) {
    asm volatile("outw %0, %w1" : : "a"(val), "Nd"(port));
}

void outl(uint16_t port, uint32_t val) {
    asm volatile("outl %0, %w1" : : "a"(val), "Nd"(port));
}

uint8_t ioread8(volatile void *addr) {
    return *(volatile uint8_t *)addr;
}

uint16_t ioread16(volatile void *addr) {
    uint16_t tmp;
    memcpy(&tmp, (void *)addr, sizeof(tmp));
    return tmp;
}

uint32_t ioread32(volatile void *addr) {
    uint32_t tmp;
    memcpy(&tmp, (void *)addr, sizeof(tmp));
    return tmp;
}

void iowrite8(volatile void *addr, uint8_t val) {
    *(volatile uint8_t *)addr = val;
}

void iowrite16(volatile void *addr, uint16_t val) {
    memcpy((void *)addr, &val, sizeof(val));
}

void iowrite32(volatile void *addr, uint32_t val) {
    memcpy((void *)addr, &val, sizeof(val));
}
// interrupts
// TODO: use APIC and impliement real functions
#define IDT_SIZE 256

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idtr_t;

idt_entry_t idt[IDT_SIZE];
idtr_t idtr;

void idt_set_gate(int n, uint64_t handler_addr)
{
    idt_entry_t *e = &idt[n];
    e->offset_low  = handler_addr & 0xFFFF;
    e->selector    = 0x08;
    e->ist         = 0;
    e->type_attr   = 0x8E;
    e->offset_mid  = (handler_addr >> 16) & 0xFFFF;
    e->offset_high = (handler_addr >> 32) & 0xFFFFFFFF;
    e->zero        = 0;
}

void pic_init(void)
{
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0x00);
    outb(0xA1, 0x00);
}

void isr_common(uint32_t vec, void *regs)
{
    (void)vec;
    (void)regs;
    if (vec >= 0x20 && vec <= 0x2F) {
        if (vec >= 0x28) outb(0xA0, 0x20);
        outb(0x20, 0x20);
    }
}

#define ISR_STUB_DEF(NUM)                            \
void isr_stub##NUM(void) __attribute__((naked));    \
void isr_stub##NUM(void) {                          \
    __asm__ volatile (                              \
        "pushq $0\n\t"                              \
        "pushq %rax\n\t"                            \
        "pushq %rbx\n\t"                            \
        "pushq %rcx\n\t"                            \
        "pushq %rdx\n\t"                            \
        "pushq %rsi\n\t"                            \
        "pushq %rdi\n\t"                            \
        "pushq %rbp\n\t"                            \
        "pushq %r8\n\t"                             \
        "pushq %r9\n\t"                             \
        "pushq %r10\n\t"                            \
        "pushq %r11\n\t"                            \
        "pushq %r12\n\t"                            \
        "pushq %r13\n\t"                            \
        "pushq %r14\n\t"                            \
        "pushq %r15\n\t"                            \
        "movq %rsp, %rsi\n\t"                       \
        "movl $" #NUM ", %edi\n\t"                  \
        "subq $8, %rsp\n\t"                         \
        "call isr_common\n\t"                       \
        "addq $8, %rsp\n\t"                         \
        "popq %r15\n\t"                             \
        "popq %r14\n\t"                             \
        "popq %r13\n\t"                             \
        "popq %r12\n\t"                             \
        "popq %r11\n\t"                             \
        "popq %r10\n\t"                             \
        "popq %r9\n\t"                              \
        "popq %r8\n\t"                              \
        "popq %rbp\n\t"                             \
        "popq %rdi\n\t"                             \
        "popq %rsi\n\t"                             \
        "popq %rdx\n\t"                             \
        "popq %rcx\n\t"                             \
        "popq %rbx\n\t"                             \
        "popq %rax\n\t"                             \
        "addq $8, %rsp\n\t"                         \
        "iretq\n\t"                                 \
    );                                             \
}

ISR_STUB_DEF(0)
ISR_STUB_DEF(1)
ISR_STUB_DEF(2)
ISR_STUB_DEF(3)
ISR_STUB_DEF(4)
ISR_STUB_DEF(5)
ISR_STUB_DEF(6)
ISR_STUB_DEF(7)
ISR_STUB_DEF(8)
ISR_STUB_DEF(9)
ISR_STUB_DEF(10)
ISR_STUB_DEF(11)
ISR_STUB_DEF(12)
ISR_STUB_DEF(13)
ISR_STUB_DEF(14)
ISR_STUB_DEF(15)
ISR_STUB_DEF(16)
ISR_STUB_DEF(17)
ISR_STUB_DEF(18)
ISR_STUB_DEF(19)
ISR_STUB_DEF(20)
ISR_STUB_DEF(21)
ISR_STUB_DEF(22)
ISR_STUB_DEF(23)
ISR_STUB_DEF(24)
ISR_STUB_DEF(25)
ISR_STUB_DEF(26)
ISR_STUB_DEF(27)
ISR_STUB_DEF(28)
ISR_STUB_DEF(29)
ISR_STUB_DEF(30)
ISR_STUB_DEF(31)
ISR_STUB_DEF(32)
ISR_STUB_DEF(33)
ISR_STUB_DEF(34)
ISR_STUB_DEF(35)
ISR_STUB_DEF(36)
ISR_STUB_DEF(37)
ISR_STUB_DEF(38)
ISR_STUB_DEF(39)
ISR_STUB_DEF(40)
ISR_STUB_DEF(41)
ISR_STUB_DEF(42)
ISR_STUB_DEF(43)
ISR_STUB_DEF(44)
ISR_STUB_DEF(45)
ISR_STUB_DEF(46)
ISR_STUB_DEF(47)

void isr_default(void) __attribute__((naked));
void isr_default(void) {
    __asm__ volatile (
        "pushq $0\n\t"
        "pushq $-1\n\t"
        "pushq %rax\n\t"
        "pushq %rbx\n\t"
        "pushq %rcx\n\t"
        "pushq %rdx\n\t"
        "pushq %rsi\n\t"
        "pushq %rdi\n\t"
        "pushq %rbp\n\t"
        "pushq %r8\n\t"
        "pushq %r9\n\t"
        "pushq %r10\n\t"
        "pushq %r11\n\t"
        "pushq %r12\n\t"
        "pushq %r13\n\t"
        "pushq %r14\n\t"
        "pushq %r15\n\t"
        "movq %rsp, %rsi\n\t"
        "movl $-1, %edi\n\t"
        "subq $8, %rsp\n\t"
        "call isr_common\n\t"
        "addq $8, %rsp\n\t"
        "popq %r15\n\t"
        "popq %r14\n\t"
        "popq %r13\n\t"
        "popq %r12\n\t"
        "popq %r11\n\t"
        "popq %r10\n\t"
        "popq %r9\n\t"
        "popq %r8\n\t"
        "popq %rbp\n\t"
        "popq %rdi\n\t"
        "popq %rsi\n\t"
        "popq %rdx\n\t"
        "popq %rcx\n\t"
        "popq %rbx\n\t"
        "popq %rax\n\t"
        "addq $8, %rsp\n\t"
        "iretq\n\t"
    );
}

void idt_init(void)
{
    for (int i = 0; i < IDT_SIZE; ++i) {
        idt_set_gate(i, (uint64_t)(uintptr_t) &isr_default);
    }

    idt_set_gate(0,  (uint64_t)(uintptr_t) &isr_stub0);
    idt_set_gate(1,  (uint64_t)(uintptr_t) &isr_stub1);
    idt_set_gate(2,  (uint64_t)(uintptr_t) &isr_stub2);
    idt_set_gate(3,  (uint64_t)(uintptr_t) &isr_stub3);
    idt_set_gate(4,  (uint64_t)(uintptr_t) &isr_stub4);
    idt_set_gate(5,  (uint64_t)(uintptr_t) &isr_stub5);
    idt_set_gate(6,  (uint64_t)(uintptr_t) &isr_stub6);
    idt_set_gate(7,  (uint64_t)(uintptr_t) &isr_stub7);
    idt_set_gate(8,  (uint64_t)(uintptr_t) &isr_stub8);
    idt_set_gate(9,  (uint64_t)(uintptr_t) &isr_stub9);
    idt_set_gate(10, (uint64_t)(uintptr_t) &isr_stub10);
    idt_set_gate(11, (uint64_t)(uintptr_t) &isr_stub11);
    idt_set_gate(12, (uint64_t)(uintptr_t) &isr_stub12);
    idt_set_gate(13, (uint64_t)(uintptr_t) &isr_stub13);
    idt_set_gate(14, (uint64_t)(uintptr_t) &isr_stub14);
    idt_set_gate(15, (uint64_t)(uintptr_t) &isr_stub15);
    idt_set_gate(16, (uint64_t)(uintptr_t) &isr_stub16);
    idt_set_gate(17, (uint64_t)(uintptr_t) &isr_stub17);
    idt_set_gate(18, (uint64_t)(uintptr_t) &isr_stub18);
    idt_set_gate(19, (uint64_t)(uintptr_t) &isr_stub19);
    idt_set_gate(20, (uint64_t)(uintptr_t) &isr_stub20);
    idt_set_gate(21, (uint64_t)(uintptr_t) &isr_stub21);
    idt_set_gate(22, (uint64_t)(uintptr_t) &isr_stub22);
    idt_set_gate(23, (uint64_t)(uintptr_t) &isr_stub23);
    idt_set_gate(24, (uint64_t)(uintptr_t) &isr_stub24);
    idt_set_gate(25, (uint64_t)(uintptr_t) &isr_stub25);
    idt_set_gate(26, (uint64_t)(uintptr_t) &isr_stub26);
    idt_set_gate(27, (uint64_t)(uintptr_t) &isr_stub27);
    idt_set_gate(28, (uint64_t)(uintptr_t) &isr_stub28);
    idt_set_gate(29, (uint64_t)(uintptr_t) &isr_stub29);
    idt_set_gate(30, (uint64_t)(uintptr_t) &isr_stub30);
    idt_set_gate(31, (uint64_t)(uintptr_t) &isr_stub31);
    idt_set_gate(32, (uint64_t)(uintptr_t) &isr_stub32);
    idt_set_gate(33, (uint64_t)(uintptr_t) &isr_stub33);
    idt_set_gate(34, (uint64_t)(uintptr_t) &isr_stub34);
    idt_set_gate(35, (uint64_t)(uintptr_t) &isr_stub35);
    idt_set_gate(36, (uint64_t)(uintptr_t) &isr_stub36);
    idt_set_gate(37, (uint64_t)(uintptr_t) &isr_stub37);
    idt_set_gate(38, (uint64_t)(uintptr_t) &isr_stub38);
    idt_set_gate(39, (uint64_t)(uintptr_t) &isr_stub39);
    idt_set_gate(40, (uint64_t)(uintptr_t) &isr_stub40);
    idt_set_gate(41, (uint64_t)(uintptr_t) &isr_stub41);
    idt_set_gate(42, (uint64_t)(uintptr_t) &isr_stub42);
    idt_set_gate(43, (uint64_t)(uintptr_t) &isr_stub43);
    idt_set_gate(44, (uint64_t)(uintptr_t) &isr_stub44);
    idt_set_gate(45, (uint64_t)(uintptr_t) &isr_stub45);
    idt_set_gate(46, (uint64_t)(uintptr_t) &isr_stub46);
    idt_set_gate(47, (uint64_t)(uintptr_t) &isr_stub47);

    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)(uintptr_t)&idt;
    __asm__ volatile ("lidt %0" : : "m"(idtr));

    pic_init();

    __asm__ volatile ("sti");
}

/*
 * PS/2 DRIVER
 */
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
    idt_init();
    ps2_keyboard_init();
}
