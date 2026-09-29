read_disk_sectors:
    pushad
    add eax, [DISK_INFO_ADDRESS + DiskInfo.image_first_sector]
    movzx ebx, byte [DISK_INFO_ADDRESS + DiskInfo.sectors_per_read_unit]
    xor edx, edx
    div ebx 
    mov [read_packet.first_sector], eax

    movzx eax, cx
    add eax, ebx
    dec eax
    xor edx, edx
    div ebx
    mov [read_packet.sector_count], ax

    mov [read_packet.buffer_offset], di
    mov [read_packet.buffer_segment], es
    mov si, read_packet
    mov ah, 0x42                    ; BIOS: extended read
    mov dl, [DISK_INFO_ADDRESS + DiskInfo.drive]
    int 0x13
    mov [disk_error_code], ah
    popad
    ret

disk_error_code db 0

align 4
read_packet:
    db 16  ; size of this packet
    db 0
.sector_count   dw 0
.buffer_offset  dw 0
.buffer_segment dw 0
.first_sector   dd 0
                dd 0 ;should be 0
