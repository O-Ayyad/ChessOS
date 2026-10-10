#pragma once
#include "types.h"

const u64 KERNEL_ADDRESS   = 0x100000;    //1 MB the kernel is loaded here
const u64 KERNEL_STACK_TOP = 0xF00000;    //15 MB the call here
const u64 ASSETS_ADDRESS   = 0x1000000;   //16 MB the assets are loaded here
const u32 BOOT_INFO_MAGIC  = 0x33534F43;  //COS3
const int MAX_MEMORY_REGIONS = 64;

const u32 MEMORY_TYPE_USABLE = 1;
struct __attribute__((packed)) MemoryRegion {
    u64 base;
    u64 length;
    u32 type;       //MEMORY_TYPE_USABLE
    u32 extra;
};

struct __attribute__((packed)) BootInfo {
    u32 magic;                  // BOOT_INFO_MAGIC
    u32 memory_region_count;

    u64 framebuffer_address;    //screen memory

    u32 screen_width;
    u32 screen_height;
    
    u32 bytes_per_line;
    u32 bits_per_pixel;

    u8 red_shift;        //where red green and blue sit in a pixel
    u8 green_shift;
    u8 blue_shift;

    u8 padding;

    u32 kernel_address;
    u32 kernel_size;

    u32 assets_address;
    u32 assets_size;

    MemoryRegion memory_regions[MAX_MEMORY_REGIONS];
};
