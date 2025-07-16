.section .multiboot
    .long 0x1BADB002              # magic
    .long 0x00000000              # flags
    .long -(0x1BADB002 + 0x00000000)  # checksum

.section .text
.global _start
.type _start, @function
.extern kernel_main
_start:
    cli                 # Disable interrupts
    call kernel_main		# Call your main function

.hang:
    hlt
    jmp .hang           # Halt forever after running kernel_main
