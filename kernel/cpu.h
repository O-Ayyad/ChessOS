#pragma once
#include "types.h"


static inline void write_port_8(u16 port, u8 value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void write_port_16(u16 port, u16 value) {
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline void write_port_32(u16 port, u32 value) {
    asm volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 read_port_8(u16 port) {
    u8 value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline u16 read_port_16(u16 port) {
    u16 value;
    asm volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline u32 read_port_32(u16 port) {
    u32 value;
    asm volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

//cpu cycles it was switched on
static inline u64 read_cycle_counter() {
    u32 low, high;
    asm volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((u64)high << 32) | low;
}

//Tells the CPU we are in a waiting loop
static inline void cpu_relax() { asm volatile("pause"); }


[[noreturn]] static inline void kernel_panic() {
    while (true) {
        asm volatile("cli; hlt");
    }
}

// Sets up the table that tells the CPU where to jump on a panic
// (like dividing by zero). We then show a red error screen.
void setup_error_handlers();
