#pragma once
#include "types.h"
#include "boot_info.h"

void clear_global_variables();
void setup_memory(const BootInfo* boot_info);

void* allocate_memory(u64 byte_count);
void* allocate_aligned_memory(u64 byte_count, u64 alignment);
u64   free_memory_bytes();

void* memory_bookmark();
void  free_memory_back_to(void* bookmark);

template <typename T> T* allocate_array(u64 count) {
    return (T*)allocate_memory(count * sizeof(T));
}
