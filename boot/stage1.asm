bits 16
org 0x7C00
%include "layout.inc"

    jmp short start
    nop
    times 8 - ($ - $$) db 0
cd_primary_volume_sector    dd 0

;where chessos.img starts on the CD
cd_image_first_sector       dd 0
cd_image_length             dd 0
cd_image_checksum           dd 0
    times 64 - ($ - $$) db 0

DRIVE_PARAMETERS_SIZE   equ 0x1A    ;the size of the buffer for BIOS function 0x48
BYTES_PER_SECTOR_FIELD  equ 0x18    ;where the sector size is in that buffer

start:

    ;all segments to 0 and a stack that grows down from below this code
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, BOOT_SECTOR_ADDRESS
    sti
    mov [DISK_INFO_ADDRESS + DiskInfo.drive], dl    ; the BIOS passes the boot drive in DL

    mov si, hello_message
    call print_text

    mov ah, 0x41                    ; BIOS are extended disk reads supported
    mov bx, 0x55AA
    mov dl, [DISK_INFO_ADDRESS + DiskInfo.drive]
    int 0x13
    jc no_extended_reads
    cmp bx, 0xAA55
    jne no_extended_reads

    mov eax, [cd_image_first_sector]
    shl eax, 2
    mov [DISK_INFO_ADDRESS + DiskInfo.image_first_sector], eax

    mov byte [DISK_INFO_ADDRESS + DiskInfo.sectors_per_read_unit], 1
    mov si, drive_parameters
    mov word [si], DRIVE_PARAMETERS_SIZE
    mov ah, 0x48                    ; BIOS get drive parameters
    mov dl, [DISK_INFO_ADDRESS + DiskInfo.drive]
    int 0x13
    jc read_stage2                  ;assume a normal disk
    cmp word [drive_parameters + BYTES_PER_SECTOR_FIELD], CD_SECTOR_SIZE
    jne read_stage2

    ; it is a CD
    mov byte [DISK_INFO_ADDRESS + DiskInfo.sectors_per_read_unit], CD_SECTOR_SIZE / SECTOR_SIZE

read_stage2:
    mov eax, STAGE2_FIRST_SECTOR
    mov cx, STAGE2_SECTOR_COUNT
    mov di, STAGE2_ADDRESS
    call read_disk_sectors
    jc disk_error
    jmp 0:STAGE2_ADDRESS

no_extended_reads:
    mov si, no_extended_message
    call print_text
    jmp stop_forever

disk_error:
    mov si, disk_error_message
    call print_text

stop_forever:
    cli
    hlt
    jmp stop_forever

; Prints the text at SI
print_text:
    cld
    xor bh, bh
.next:
    lodsb
    cmp al, 0
    je .done
    mov ah, 0x0E                     ;BIOS print the character in AL
    int 0x10
    jmp .next
.done:
    ret

%include "disk_read.inc"

;data
hello_message       db "ChessOS boot sector", 13, 10, 0
no_extended_message db "Error: this BIOS cannot do extended disk reads", 0
disk_error_message  db "Error: could not read stage 2", 0
; scratch memory for the answer of BIOS function 0x48
drive_parameters    equ 0x0700

; The partition table and the boot signature end the sector.
; tools/make_image.py fills in the partition table.
    times 446 - ($ - $$) db 0
partition_table:
    times 64 db 0
    dw 0xAA55
