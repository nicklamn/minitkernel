Hello! Welcome to my first large project!<br>
<br>
minitkernel (Mini T Kernel, we can guess what the T is) is a minimal "kernel" if you will call it that.<br>
minitkernel is designed for BIOS based, i386+ IBM PC compatible computers. Support for UEFI is only available through CSM/BIOS, and GRUB (https://www.gnu.org/software/grub/).<br>
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
    clean: Deletes *.o, kernel.elf.<br>
<br>
Includes:<br>
    stddef.h: C stddef.h but minimal.<br>
    stdint.h: C stdint.h but minimal.<br>
    string.g: C string.h but minimal.<br>
    font8x8_basic.h: 8x8 VGA Font, basic characters.<br>


This would be impossible without:<br>
    https://www.gnu.org/software/grub/ - GNU GRUB, the best bootloader.<br>
    https://gcc.gnu.org/ - GNU GCC, the C compiler.<br>
    https://www.gnu.org/software/binutils/ - GNU Binutils, forgot what these do but we need them.<br>
    https://archlinux.org/ - The simplest, lightest distro out there, with it's large AUR.<br>
    https://www.apple.com/os/macos/ - A better alternative to Linux, smoother, and nicer to look at.<br>
