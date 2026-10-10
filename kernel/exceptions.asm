bits 64
extern cpu_error_handler
global error_entry_points

%macro ERROR_WITHOUT_CODE 1
error_entry_%1:

    ; fake error code
    push 0
    push %1                 ; exception number
    jmp common_error_code
%endmacro

%macro ERROR_WITH_CODE 1
error_entry_%1:
    push %1                 ; exception number
    jmp common_error_code
%endmacro

section .text
ERROR_WITHOUT_CODE 0        ;divide by zero
ERROR_WITHOUT_CODE 1        ;debug
ERROR_WITHOUT_CODE 2        ;non maskable interrupt
ERROR_WITHOUT_CODE 3        ;breakpoint
ERROR_WITHOUT_CODE 4        ;overflow
ERROR_WITHOUT_CODE 5        ;bound range exceeded
ERROR_WITHOUT_CODE 6        ;invalid instruction
ERROR_WITHOUT_CODE 7        ;no floating point unit
ERROR_WITH_CODE    8        ;double fault
ERROR_WITHOUT_CODE 9        ;(old, unused)
ERROR_WITH_CODE    10       ;invalid task state segment
ERROR_WITH_CODE    11       ;segment not present
ERROR_WITH_CODE    12       ;stack fault
ERROR_WITH_CODE    13       ;general protection fault
ERROR_WITH_CODE    14       ;page fault
ERROR_WITHOUT_CODE 15       
ERROR_WITHOUT_CODE 16       ;floating point error
ERROR_WITH_CODE    17       ;alignment check
ERROR_WITHOUT_CODE 18       ;machine check
ERROR_WITHOUT_CODE 19       ;SSE floating point error

common_error_code:
    mov rdi, rsp            ;a pointer to what is on the stack
    and rsp, -16
    call cpu_error_handler
.stop:
    cli
    hlt
    jmp .stop

; A table with the addresses of the 20 entry points for setup_error_handlers().
section .rodata
align 8
error_entry_points:
%assign i 0
%rep 20
    dq error_entry_ %+ i
%assign i i + 1
%endrep

section .note.GNU-stack noalloc noexec nowrite progbits
