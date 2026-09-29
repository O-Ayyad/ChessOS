#include "memory.h"
#include "text_utils.h"
#include "screen.h"

// kernel/linker.ld
extern "C" u8 bss_begin[];
extern "C" u8 bss_end[];
extern "C" u8 kernel_end[];

const u64 FOUR_GIGABYTES = 0x100000000ULL;
const u64 DEFAULT_ALIGNMENT = 64;
const u64 PAGE_SIZE = 4096;
const u64 LOWEST_STACK_ADDRESS = KERNEL_STACK_TOP - 0x100000;

static u8* next_free = nullptr;
static u8* memory_end = nullptr;

void clear_global_variables() {

    memset(bss_begin, 0, bss_end - bss_begin);
}

static u64 round_up(u64 value, u64 multiple) {
    return (value + multiple - 1) / multiple * multiple;
}

void setup_memory(const BootInfo* boot_info) {
    if ((u64)kernel_end > LOWEST_STACK_ADDRESS) {
        fatal_error("The kernel is too big: it runs into its stack");
    }

    u64 heap_start = round_up(boot_info->assets_address + boot_info->assets_size, PAGE_SIZE);

    // Find the usable memory region that contains that address
    for (u32 i = 0; i < boot_info->memory_region_count; i++) {
        const MemoryRegion& region = boot_info->memory_regions[i];
        u64 region_end = region.base + region.length;
        if (region.type != MEMORY_TYPE_USABLE) continue;
        if (heap_start < region.base || heap_start >= region_end) continue;

        if (region_end > FOUR_GIGABYTES) {
            region_end = FOUR_GIGABYTES;
        }
        next_free = (u8*)heap_start;
        memory_end = (u8*)region_end;
        return;
    }
    fatal_error("Not enough memory: ChessOS needs at least 64 MB of RAM");
}

void* allocate_aligned_memory(u64 byte_count, u64 alignment) {
    u8* start = (u8*)round_up((u64)next_free, alignment);
    if (start + byte_count > memory_end) {
        Text message;
        message.add("Out of memory (wanted ").add_number(byte_count / 1024).add(" KB, ");
        message.add_number(free_memory_bytes() / 1024).add(" KB free)");
        fatal_error(message.text);
    }
    next_free = start + byte_count;
    memset(start, 0, byte_count);
    return start;
}

void* allocate_memory(u64 byte_count) {
    return allocate_aligned_memory(byte_count, DEFAULT_ALIGNMENT);
}

u64 free_memory_bytes() {
    return memory_end - next_free;
}

void* memory_bookmark() {
    return next_free;
}

void free_memory_back_to(void* bookmark) {
    if (bookmark != nullptr && (u8*)bookmark <= next_free) {
        next_free = (u8*)bookmark;
    }
}
