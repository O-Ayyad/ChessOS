// cpu.cpp - the error screen for CPU exceptions.
#include "cpu.h"
#include "text_utils.h"
#include "screen.h"

// One entry of the Interrupt Descriptor Table (IDT). The CPU reads these.
struct __attribute__((packed)) IdtEntry {
    u16 address_low;
    u16 code_segment;
    u8  stack_table;      // 0 = use the current stack
    u8  type;             // 0x8E = "present, kernel level, interrupt gate"
    u16 address_middle;
    u32 address_high;
    u32 reserved;
};

struct __attribute__((packed)) IdtPointer {
    u16 size_minus_one;
    u64 address;
};

const int ERROR_COUNT = 20;          // exceptions 0..19 (see exceptions.asm)
const u8 INTERRUPT_GATE_TYPE = 0x8E;
const u64 PAGE_FAULT = 14;

static IdtEntry idt[ERROR_COUNT];
extern "C" u64 error_entry_points[ERROR_COUNT];   // from exceptions.asm

static const char* error_names[ERROR_COUNT] = {
    "divide by zero", "debug", "non-maskable interrupt", "breakpoint", "overflow",
    "bound range exceeded", "invalid instruction", "no FPU", "double fault", "(unused)",
    "invalid TSS", "segment not present", "stack fault", "general protection fault",
    "page fault", "(reserved)", "floating point error", "alignment check",
    "machine check", "SSE floating point error"
};

void setup_error_handlers() {
    u16 code_segment;
    asm volatile("mov %%cs, %0" : "=r"(code_segment));

    for (int i = 0; i < ERROR_COUNT; i++) {
        u64 address = error_entry_points[i];
        idt[i].address_low = address & 0xFFFF;
        idt[i].code_segment = code_segment;
        idt[i].stack_table = 0;
        idt[i].type = INTERRUPT_GATE_TYPE;
        idt[i].address_middle = (address >> 16) & 0xFFFF;
        idt[i].address_high = address >> 32;
        idt[i].reserved = 0;
    }

    IdtPointer pointer;
    pointer.size_minus_one = sizeof(idt) - 1;
    pointer.address = (u64)idt;
    asm volatile("lidt %0" : : "m"(pointer));
}

// What exceptions.asm pushed on the stack, followed by what the CPU pushed.
struct ErrorStackFrame {
    u64 error_number;
    u64 error_code;
    u64 instruction_address;
    u64 code_segment;
    u64 flags;
    u64 stack_pointer;
    u64 stack_segment;
};

extern "C" void cpu_error_handler(ErrorStackFrame* frame) {
    Text message;
    message.add("CPU error: ");
    if (frame->error_number < ERROR_COUNT) {
        message.add(error_names[frame->error_number]);
    }
    message.add(" (#").add_number(frame->error_number).add(")");
    message.add("  at address ").add_hex(frame->instruction_address);
    message.add("  code ").add_hex(frame->error_code);
    if (frame->error_number == PAGE_FAULT) {
        u64 bad_address;
        asm volatile("mov %%cr2, %0" : "=r"(bad_address));   // CR2 = the address that failed
        message.add("  memory ").add_hex(bad_address);
    }
    fatal_error(message.text);
}
