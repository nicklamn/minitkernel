Hello! Welcome to my first large project!<br>
<br>
minitkernel (Mini T Kernel, we can guess what the T is) is a minimal "kernel" if you will call it that.<br>
minitkernel is designed for BIOS based, i386+ IBM PC compatible computers. Support for UEFI is only availble through CSM, and GRUB.<br>
In order to boot, it requires GRUB, or another method to boot Multiboot compatible ELFs.<br>
It has a simple GUI in VGA mode 0x13h, and supports boxes, lines, text, and triangle drawing.<br>
A small portion of the code, such as the includes, assembly, and complicated stuff, was made using help from ChatGPT.<br>
I sincerely apologize for using ChatGPT. ChatGPT has helped me learn a lot, and I understand a lot about C now.<br>
I will say, quite a bit of this is my own code.<br>
<br>
Makefile:<br>
    all: Calls boot.o, then kernel.o, then links those into kernel.elf.<br>
    boot.o: Compiles boot.s->boot.o.<br>
    kernel.o: Compiles kernel.c->kernel.o<br>
    clean: Deletes *.o, kernel.elf.
Includes:<br>
    stddef.h: C stddef.h but minimal
    stdint.h: C stdint.h but minimal
    string.g: C string.h but minimal
    font8x8_basic.h: 8x8 VGA Font, basic characters.
