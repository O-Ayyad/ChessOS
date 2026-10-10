bits 16
org 0x7E00
%include "layout.inc"

; tools/make_image.py tells us where the kernel and the assets are on the disk.
%ifndef KERNEL_SECTOR_COUNT
    %error "Assemble this with tools/make_image.py"
%endif

;video mode
VIDEO_WANTED_WIDTH          equ 1024
VIDEO_WANTED_HEIGHT         equ 768
VIDEO_WANTED_BITS           equ 32
VIDEO_FALLBACK_MIN_WIDTH    equ 800
VIDEO_FALLBACK_MAX_WIDTH    equ 1280


VBE_SUCCESS                 equ 0x004F
VBE_USE_LINEAR_FRAMEBUFFER  equ 0x4000
VBE_MODE_LIST_END           equ 0xFFFF
MEMORY_MODEL_DIRECT_COLOR   equ 6

;get VBE info function 0x4F00
VBE_INFO_MODE_LIST_OFFSET   equ 14
VBE_INFO_MODE_LIST_SEGMENT  equ 16

;get mode info function 0x4F01
MODE_ATTRIBUTES             equ 0
MODE_BYTES_PER_LINE         equ 16
MODE_WIDTH                  equ 18
MODE_HEIGHT                 equ 20
MODE_BITS_PER_PIXEL         equ 25
MODE_MEMORY_MODEL           equ 27
MODE_RED_POSITION           equ 32
MODE_GREEN_POSITION         equ 34
MODE_BLUE_POSITION          equ 36
MODE_FRAMEBUFFER            equ 40
MODE_SUPPORTED              equ 00000001b   ;bit 0 of the attributes
MODE_HAS_LINEAR_FRAMEBUFFER equ 10000000b   ;bit 7 of the attributes

;bios memory map
E820_SIGNATURE              equ 0x534D4150  ; "SMAP"
E820_ENTRY_SIZE             equ MEMORY_REGION_SIZE

stage2_start:
    mov si, loader_message
    call print_text

    call enable_a20_line
    call clear_boot_info
    call read_memory_map

    mov si, loading_message
    call print_text
    call load_kernel_and_assets
    mov si, new_line
    call print_text

    call set_video_mode
    jmp switch_to_long_mode

enable_a20_line:
    mov ax, 0x2401                  ;enable A20
    int 0x15
    call is_a20_on
    jnc .done

    in al, 0x92
    or al, 00000010b                ; bit 1 = A20 on
    and al, 11111110b
    out 0x92, al
    call is_a20_on
    jnc .done

    mov si, a20_error_message
    jmp fatal_error
.done:
    ret

is_a20_on:
    push ds
    push es
    xor ax, ax
    mov ds, ax
    mov ax, 0xFFFF
    mov es, ax

    mov byte [ds:0x0500], 0x00
    mov byte [es:0x0510], 0xFF
    cmp byte [ds:0x0500], 0xFF
    pop es
    pop ds
    je .off
    clc
    ret
.off:
    stc
    ret


;memory map
clear_boot_info:
    mov di, BOOT_INFO_ADDRESS
    mov cx, BootInfo_size
    xor al, al
    rep stosb
    mov dword [BOOT_INFO_ADDRESS + BootInfo.magic], BOOT_INFO_MAGIC
    ret

read_memory_map:
    mov di, BOOT_INFO_ADDRESS + BootInfo.memory_regions
    xor ebx, ebx
    xor bp, bp 
.next_region:
    mov eax, 0xE820
    mov edx, E820_SIGNATURE
    mov ecx, E820_ENTRY_SIZE
    mov dword [di + 20], 1
    int 0x15
    jc .done
    cmp eax, E820_SIGNATURE
    jne .done
    inc bp
    add di, E820_ENTRY_SIZE
    cmp bp, MAX_MEMORY_REGIONS
    je .done
    test ebx, ebx  ; ebx = 0 means last region
    jnz .next_region
.done:
    mov [BOOT_INFO_ADDRESS + BootInfo.memory_region_count], bp
    ret


;load kernel and assets
load_kernel_and_assets:
    mov eax, KERNEL_FIRST_SECTOR
    mov ecx, KERNEL_SECTOR_COUNT
    mov edi, KERNEL_ADDRESS
    call load_sectors_to_high_memory

    mov eax, ASSETS_FIRST_SECTOR
    mov ecx, ASSETS_SECTOR_COUNT
    mov edi, ASSETS_ADDRESS
    call load_sectors_to_high_memory

    ;tell kernel where things are
    mov dword [BOOT_INFO_ADDRESS + BootInfo.kernel_address], KERNEL_ADDRESS
    mov dword [BOOT_INFO_ADDRESS + BootInfo.kernel_size], KERNEL_SIZE
    mov dword [BOOT_INFO_ADDRESS + BootInfo.assets_address], ASSETS_ADDRESS
    mov dword [BOOT_INFO_ADDRESS + BootInfo.assets_size], ASSETS_SIZE
    ret

load_sectors_to_high_memory:
.next_chunk:
    cmp ecx, 0
    je .done

    mov ebx, SECTORS_PER_READ
    cmp ecx, ebx
    jae .count_ok
    mov ebx, ecx
.count_ok:
    push es
    push di
    mov dx, DISK_BUFFER_SEGMENT
    mov es, dx
    xor di, di
    push cx
    mov cx, bx
    call read_disk_sectors
    pop cx
    pop di
    pop es
    jc .disk_error

    ; copy the chunk up to edi
    pushad
    call enter_unreal_mode
    popad
    pushad
    cld
    mov esi, DISK_BUFFER_ADDRESS
    mov ecx, ebx
    shl ecx, 7                      ; sectors * 512 / 4
    a32 rep movsd                   ; copy ecx 4byte words from ds esi to es edi
    sti
    popad

    ;move on to the next chunk
    add eax, ebx
    sub ecx, ebx
    mov edx, ebx
    shl edx, 9                      ;sectors * 512 = bytes
    add edi, edx

    pushad
    mov si, dot
    call print_text                 ;dot for every 32kb
    popad
    jmp .next_chunk
.done:
    ret
.disk_error:
    mov si, disk_error_message
    call print_text
    mov al, [disk_error_code]
    call print_hex_byte
    jmp fatal_error.stop

enter_unreal_mode:
    cli
    push ds
    push es
    lgdt [gdt_pointer]
    mov eax, cr0
    or al, 1                        ;protected mode on
    mov cr0, eax
    mov bx, DATA_SELECTOR
    mov ds, bx
    mov es, bx
    and al, 0xFE                    ;protected mode off
    mov cr0, eax
    pop es
    pop ds
    ret


set_video_mode:
    mov di, VBE_INFO_ADDRESS
    mov dword [di], 'VBE2'
    mov ax, 0x4F00                  ;get info about the video card
    int 0x10
    cmp ax, VBE_SUCCESS
    jne .no_vbe

    mov si, [VBE_INFO_ADDRESS + VBE_INFO_MODE_LIST_OFFSET]
    mov ax, [VBE_INFO_ADDRESS + VBE_INFO_MODE_LIST_SEGMENT]
    mov fs, ax
    mov word [chosen_mode], VBE_MODE_LIST_END

.next_mode:
    mov cx, [fs:si]
    add si, 2
    cmp cx, VBE_MODE_LIST_END
    je .end_of_list

    push si
    push fs
    push cx
    mov ax, 0x4F01
    mov di, VBE_MODE_INFO_ADDRESS
    int 0x10
    pop cx
    pop fs
    pop si
    cmp ax, VBE_SUCCESS
    jne .next_mode

    test word [VBE_MODE_INFO_ADDRESS + MODE_ATTRIBUTES], MODE_SUPPORTED
    jz .next_mode
    test word [VBE_MODE_INFO_ADDRESS + MODE_ATTRIBUTES], MODE_HAS_LINEAR_FRAMEBUFFER
    jz .next_mode
    cmp byte [VBE_MODE_INFO_ADDRESS + MODE_BITS_PER_PIXEL], VIDEO_WANTED_BITS
    jne .next_mode
    cmp byte [VBE_MODE_INFO_ADDRESS + MODE_MEMORY_MODEL], MEMORY_MODEL_DIRECT_COLOR
    jne .next_mode

    ;if exactly the size we want then stop looking
    cmp word [VBE_MODE_INFO_ADDRESS + MODE_WIDTH], VIDEO_WANTED_WIDTH
    jne .maybe_fallback
    cmp word [VBE_MODE_INFO_ADDRESS + MODE_HEIGHT], VIDEO_WANTED_HEIGHT
    jne .maybe_fallback
    mov [chosen_mode], cx
    jmp .end_of_list

.maybe_fallback:
    cmp word [fallback_mode], VBE_MODE_LIST_END
    jne .next_mode                  ;we already have a fallback
    cmp word [VBE_MODE_INFO_ADDRESS + MODE_WIDTH], VIDEO_FALLBACK_MIN_WIDTH
    jb .next_mode
    cmp word [VBE_MODE_INFO_ADDRESS + MODE_WIDTH], VIDEO_FALLBACK_MAX_WIDTH
    ja .next_mode
    mov [fallback_mode], cx
    jmp .next_mode

.end_of_list:
    mov cx, [chosen_mode]
    cmp cx, VBE_MODE_LIST_END
    jne .have_mode
    mov cx, [fallback_mode]
    cmp cx, VBE_MODE_LIST_END
    je .no_mode

.have_mode:
    ; get the info of the mode we picked again
    push cx
    ;get info about mode cx
    mov ax, 0x4F01
    mov di, VBE_MODE_INFO_ADDRESS
    int 0x10
    pop bx
    cmp ax, VBE_SUCCESS
    jne .no_mode

    ;switch to it
    or bx, VBE_USE_LINEAR_FRAMEBUFFER
    mov ax, 0x4F02
    int 0x10
    cmp ax, VBE_SUCCESS
    jne .no_mode

    ;tell kernel about the screen
    mov eax, [VBE_MODE_INFO_ADDRESS + MODE_FRAMEBUFFER]     ; physical address of the screen memory
    mov [BOOT_INFO_ADDRESS + BootInfo.framebuffer_address], eax
    movzx eax, word [VBE_MODE_INFO_ADDRESS + MODE_WIDTH]
    mov [BOOT_INFO_ADDRESS + BootInfo.screen_width], eax
    movzx eax, word [VBE_MODE_INFO_ADDRESS + MODE_HEIGHT]
    mov [BOOT_INFO_ADDRESS + BootInfo.screen_height], eax
    movzx eax, word [VBE_MODE_INFO_ADDRESS + MODE_BYTES_PER_LINE]
    mov [BOOT_INFO_ADDRESS + BootInfo.bytes_per_line], eax
    movzx eax, byte [VBE_MODE_INFO_ADDRESS + MODE_BITS_PER_PIXEL]
    mov [BOOT_INFO_ADDRESS + BootInfo.bits_per_pixel], eax

    ;where red green blue sit in a pixel
    mov al, [VBE_MODE_INFO_ADDRESS + MODE_RED_POSITION]
    mov [BOOT_INFO_ADDRESS + BootInfo.red_shift], al
    mov al, [VBE_MODE_INFO_ADDRESS + MODE_GREEN_POSITION]
    mov [BOOT_INFO_ADDRESS + BootInfo.green_shift], al
    mov al, [VBE_MODE_INFO_ADDRESS + MODE_BLUE_POSITION]
    mov [BOOT_INFO_ADDRESS + BootInfo.blue_shift], al
    ret

.no_vbe:
    mov si, no_vbe_message
    jmp fatal_error
.no_mode:
    mov si, no_mode_message
    jmp fatal_error

switch_to_long_mode:
    cli
    lgdt [gdt_pointer]
    mov eax, cr0
    or eax, 1 
    mov cr0, eax
    jmp CODE32_SELECTOR:protected_mode_start

CR0_MONITOR_FPU             equ 1 << 1
CR0_EMULATE_FPU             equ 1 << 2
CR0_PAGING                  equ 1 << 31
CR4_PAE                     equ 1 << 5
CR4_OS_SUPPORTS_SSE         equ 1 << 9
CR4_OS_SUPPORTS_SSE_ERRORS  equ 1 << 10
MSR_EFER                    equ 0xC0000080
EFER_LONG_MODE              equ 1 << 8

bits 32
protected_mode_start:
    mov ax, DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, BOOT_SECTOR_ADDRESS

    call build_page_tables

    mov eax, cr4
    or eax, CR4_PAE | CR4_OS_SUPPORTS_SSE | CR4_OS_SUPPORTS_SSE_ERRORS
    mov cr4, eax

    mov eax, PML4_ADDRESS           ;CR3 points to the top page table
    mov cr3, eax

    mov ecx, MSR_EFER
    rdmsr
    or eax, EFER_LONG_MODE
    wrmsr

    ;turn on paging
    mov eax, cr0
    or eax, CR0_PAGING | CR0_MONITOR_FPU
    and eax, ~CR0_EMULATE_FPU
    mov cr0, eax

    jmp CODE64_SELECTOR:long_mode_start

;page tables
PAGE_SIZE_4K        equ 4096
PML4_ADDRESS        equ PAGE_TABLES_ADDRESS
PDPT_ADDRESS        equ PAGE_TABLES_ADDRESS + 1 * PAGE_SIZE_4K
PAGE_DIRS_ADDRESS   equ PAGE_TABLES_ADDRESS + 2 * PAGE_SIZE_4K
PAGE_DIR_COUNT      equ 4
ENTRIES_PER_TABLE   equ 512
TABLE_ENTRY_SIZE    equ 8
TWO_MEGABYTES       equ 0x200000
PAGE_PRESENT        equ 1
PAGE_WRITABLE       equ 2
PAGE_HUGE           equ 0x80        ;this entry is a 2 MB page

build_page_tables:
    mov edi, PAGE_TABLES_ADDRESS
    mov ecx, (2 + PAGE_DIR_COUNT) * PAGE_SIZE_4K / 4
    xor eax, eax
    rep stosd

    mov dword [PML4_ADDRESS], PDPT_ADDRESS | PAGE_PRESENT | PAGE_WRITABLE
    mov edi, PDPT_ADDRESS
    mov eax, PAGE_DIRS_ADDRESS | PAGE_PRESENT | PAGE_WRITABLE
    mov ecx, PAGE_DIR_COUNT
.next_directory:
    mov [edi], eax
    add eax, PAGE_SIZE_4K
    add edi, TABLE_ENTRY_SIZE
    loop .next_directory

    ;one loop fills all four
    mov edi, PAGE_DIRS_ADDRESS
    mov eax, PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE
    mov ecx, PAGE_DIR_COUNT * ENTRIES_PER_TABLE
.next_page:
    mov [edi], eax
    add eax, TWO_MEGABYTES
    add edi, TABLE_ENTRY_SIZE
    loop .next_page
    ret

bits 64
long_mode_start:
    mov ax, DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    fninit      ;reset the floating point unit
    mov rsp, KERNEL_STACK_TOP
    mov rdi, BOOT_INFO_ADDRESS      ;the first arg of kernel_start
    mov rax, KERNEL_ADDRESS
    call rax                        ;kernel start
.stop:
    cli
    hlt
    jmp .stop


bits 16

print_text:
    cld
    xor bh, bh
.next:
    lodsb
    cmp al, 0
    je .done
    mov ah, 0x0E
    int 0x10
    jmp .next
.done:
    ret

print_hex_byte:
    push ax
    shr al, 4         ;high 4 bits first
    call .print_digit
    pop ax
    and al, 0x0F      ;low 4 bits
.print_digit:
    add al, '0'
    cmp al, '9'
    jbe .show
    add al, 'A' - '0' - 10 ;10 -> 15 become A -> F
.show:
    mov ah, 0x0E
    int 0x10
    ret

fatal_error:
    call print_text
.stop:
    cli
    hlt
    jmp .stop


; written by Claude below
; The GDT ("global descriptor table") describes memory segments. In 64-bit
; mode segments barely matter, but the CPU still needs a table with a code
; and a data entry.
ACCESS_CODE     equ 10011010b       ; present, kernel level, code, readable
ACCESS_DATA     equ 10010010b       ; present, kernel level, data, writable
FLAGS_32_BIT    equ 1100b           ; 4 KB units (so the limit covers 4 GB), 32-bit
FLAGS_64_BIT    equ 1010b           ; 4 KB units, 64-bit code

%macro GDT_ENTRY 2                  ; GDT_ENTRY access, flags  (base 0, limit 4 GB)
    dw 0xFFFF                       ; limit, low 16 bits
    dw 0                            ; base, low 16 bits
    db 0                            ; base, next 8 bits
    db %1                           ; access byte
    db (%2 << 4) | 0x0F             ; flags + limit, high 4 bits
    db 0                            ; base, high 8 bits
%endmacro

align 8
gdt_start:
    ; the first entry must be empty
    dq 0
    GDT_ENTRY ACCESS_CODE, FLAGS_32_BIT
    GDT_ENTRY ACCESS_DATA, FLAGS_32_BIT
    GDT_ENTRY ACCESS_CODE, FLAGS_64_BIT
gdt_end:

; a selector is the entry's position in the table, in bytes
CODE32_SELECTOR equ 1 * 8
DATA_SELECTOR   equ 2 * 8
CODE64_SELECTOR equ 3 * 8

gdt_pointer:
    dw gdt_end - gdt_start - 1
    dd gdt_start


; ---- data ----
chosen_mode         dw VBE_MODE_LIST_END
fallback_mode       dw VBE_MODE_LIST_END
loader_message      db "ChessOS loader", 13, 10, 0
loading_message     db "Loading", 0
new_line            db 13, 10, 0
dot                 db ".", 0
a20_error_message   db "Error: could not turn on the A20 line", 0
disk_error_message  db 13, 10, "Error: could not read the disk, BIOS error code ", 0
no_vbe_message      db "Error: this video card has no VESA BIOS", 0
no_mode_message     db "Error: no 32-bit video mode between 800 and 1280 pixels wide", 0

%include "disk_read.inc"
