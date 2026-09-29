SECTOR_SIZE             equ 512
CD_SECTOR_SIZE          equ 2048        ; CDs use bigger sectors: 1 CD sector = 4 of ours
STAGE2_FIRST_SECTOR     equ 4           ; stage 2 starts at byte 2048 (a CD sector boundary)
STAGE2_SECTOR_COUNT     equ 32          ; 32 sectors = 16 KB, more than stage 2 needs

BOOT_SECTOR_ADDRESS     equ 0x7C00      ; the BIOS always loads sector 0 here
STAGE2_ADDRESS          equ 0x7E00      ; stage 1 loads stage 2 right after itself
DISK_INFO_ADDRESS       equ 0x0600      ; stage 1 notes how to read the boot drive here
BOOT_INFO_ADDRESS       equ 0x5000      ; facts we collect for the kernel (screen, memory)
VBE_INFO_ADDRESS        equ 0x6000      ; scratch space for the video BIOS answers
VBE_MODE_INFO_ADDRESS   equ 0x6400
DISK_BUFFER_SEGMENT     equ 0x1000      ; disk reads land at 0x10000 first ...
DISK_BUFFER_ADDRESS     equ 0x10000
SECTORS_PER_READ        equ 64          ; ... 64 sectors (32 KB) at a time
PAGE_TABLES_ADDRESS     equ 0x70000     ; 6 pages: PML4, PDPT, and 4 page directories

KERNEL_ADDRESS          equ 0x100000    ; 1 MB the kernel is copied here
KERNEL_STACK_TOP        equ 0xF00000    ; 15 MB
ASSETS_ADDRESS          equ 0x1000000   ; 16 MB

BOOT_INFO_MAGIC         equ 0x33534F43
MAX_MEMORY_REGIONS      equ 64

struc DiskInfo
    .drive              resb 1
    .sectors_per_read_unit resb 1
    .padding            resb 2
    .image_first_sector resd 1
endstruc

;must match kernel/boot_info.h
struc BootInfo
    .magic              resd 1
    .memory_region_count resd 1
    .framebuffer_address resq 1
    .screen_width       resd 1
    .screen_height      resd 1
    .bytes_per_line     resd 1
    .bits_per_pixel     resd 1
    .red_shift          resb 1
    .green_shift        resb 1
    .blue_shift         resb 1
    .padding            resb 1
    .kernel_address     resd 1
    .kernel_size        resd 1
    .assets_address     resd 1
    .assets_size        resd 1
    .memory_regions     resb 24 * MAX_MEMORY_REGIONS
endstruc
