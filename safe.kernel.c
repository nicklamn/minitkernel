#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_SIZE (VGA_WIDTH * VGA_HEIGHT)
#define VGA_ADDR (uint16_t*)0xB8000
#define WHITE_ON_BLACK 0x0F
#define VGA_COMMAND_PORT 0x3D4
#define VGA_DATA_PORT    0x3D5
#define COM1_PORT 0x3F8
#define PS2_STATUS 0x64
#define PS2_DATA   0x60
#define MAX_INPUT 4096
uint8_t vga_color = WHITE_ON_BLACK;

static uint16_t cursor_row = 0;
static uint16_t cursor_col = 0;
char input_buffer[MAX_INPUT];
int input_len = 0;
int shell_running = 1;

#ifdef kinfo_SERIAL
#define print_kinfo_raw(msg) serial_print(msg)
#else
#define print_kinfo_raw(msg) print(msg)
#endif

/* --- I/O functions --- */
void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void power_off_qemu() {
    outw(0x604, 0x2000); // QEMU ACPI shutdown
    outw(0xB004, 0x2000); // Bochs shutdown fallback
    outw(0x4004, 0x3400); // older shutdown port
    __asm__ volatile ("hlt");
    while (1); // hang if shutdown fails
}

void sleep(int ticks) {
    for (volatile int i = 0; i < ticks * 100000; i++);
}

/* --- strcmp minimal --- */
int strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) {
        a++; b++;
    }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

/* --- strncmp minimal --- */
int strncmp(const char* a, const char* b, int n) {
    while (n-- && *a && (*a == *b)) {
        a++; b++;
    }
    if (n < 0) return 0;
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

/* --- VGA cursor --- */
void move_cursor(uint16_t row, uint16_t col) {
    uint16_t pos = row * VGA_WIDTH + col;
    outb(VGA_COMMAND_PORT, 0x0F);
    outb(VGA_DATA_PORT, (uint8_t)(pos & 0xFF));
    outb(VGA_COMMAND_PORT, 0x0E);
    outb(VGA_DATA_PORT, (uint8_t)((pos >> 8) & 0xFF));
}

/* --- VGA output --- */
void set_color(const char* name) {
    uint8_t fg = 0x0F;  // default white

    if (strcmp(name, "black") == 0)        fg = 0x00;
    else if (strcmp(name, "blue") == 0)    fg = 0x01;
    else if (strcmp(name, "green") == 0)   fg = 0x02;
    else if (strcmp(name, "cyan") == 0)    fg = 0x03;
    else if (strcmp(name, "red") == 0)     fg = 0x04;
    else if (strcmp(name, "magenta") == 0) fg = 0x05;
    else if (strcmp(name, "brown") == 0)   fg = 0x06;
    else if (strcmp(name, "lightgray") == 0) fg = 0x07;
    else if (strcmp(name, "darkgray") == 0)  fg = 0x08;
    else if (strcmp(name, "lightred") == 0)  fg = 0x0C;
    else if (strcmp(name, "yellow") == 0)    fg = 0x0E;
    else if (strcmp(name, "white") == 0)     fg = 0x0F;

    vga_color = (vga_color & 0xF0) | fg;  // preserve background
}

void set_bgcolor(const char* name) {
    uint8_t bg = 0x00;  // default black

    if (strcmp(name, "black") == 0)        bg = 0x00;
    else if (strcmp(name, "blue") == 0)    bg = 0x01;
    else if (strcmp(name, "green") == 0)   bg = 0x02;
    else if (strcmp(name, "cyan") == 0)    bg = 0x03;
    else if (strcmp(name, "red") == 0)     bg = 0x04;
    else if (strcmp(name, "magenta") == 0) bg = 0x05;
    else if (strcmp(name, "brown") == 0)   bg = 0x06;
    else if (strcmp(name, "lightgray") == 0) bg = 0x07;
    else if (strcmp(name, "darkgray") == 0)  bg = 0x08;

    vga_color = (vga_color & 0x0F) | (bg << 4);  // preserve foreground
}

void clear_screen() {
    volatile uint16_t* vga = VGA_ADDR;
    uint16_t blank = ' ' | (WHITE_ON_BLACK << 8);
    for (int i = 0; i < VGA_SIZE; i++) {
        vga[i] = blank;
    }
    cursor_row = 0;
    cursor_col = 0;
    move_cursor(cursor_row, cursor_col);
}

void scroll() {
    volatile uint16_t* vga = VGA_ADDR;
    for (int row = 1; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            vga[(row - 1) * VGA_WIDTH + col] = vga[row * VGA_WIDTH + col];
        }
    }
    uint16_t blank = ' ' | (WHITE_ON_BLACK << 8);
    for (int col = 0; col < VGA_WIDTH; col++) {
        vga[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = blank;
    }
    if (cursor_row > 0) cursor_row--;
}

void vga_putc(char c) {
    volatile uint16_t* vga = VGA_ADDR;

    if (c == '\n') {
        cursor_row++;
        cursor_col = 0;
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            vga[cursor_row * VGA_WIDTH + cursor_col] = ' ' | (vga_color << 8);
        }
    } else {
        vga[cursor_row * VGA_WIDTH + cursor_col] = c | (vga_color << 8);
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    if (cursor_row >= VGA_HEIGHT) {
        scroll();
    }

    move_cursor(cursor_row, cursor_col);
}

void print(const char* str) {
    while (*str) {
        vga_putc(*str++);
    }
}

/* --- Serial output --- */
void serial_init() {
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x80);
    outb(COM1_PORT + 0, 0x03); // Baud divisor low byte (38400)
    outb(COM1_PORT + 1, 0x00); // Baud divisor high byte
    outb(COM1_PORT + 3, 0x03); // 8N1
    outb(COM1_PORT + 2, 0xC7); // FIFO enabled, clear
    outb(COM1_PORT + 4, 0x0B); // IRQs enabled, RTS/DSR
}

int serial_is_transmit_ready() {
    return inb(COM1_PORT + 5) & 0x20;
}

void serial_write_char(char c) {
    while (!serial_is_transmit_ready());
    outb(COM1_PORT, c);
}

void serial_print(const char* str) {
    while (*str) {
        if (*str == '\n') serial_write_char('\r');
        serial_write_char(*str++);
    }
}

/* --- Keyboard input --- */
static int shift_pressed = 0;

char scancode_to_char(uint8_t sc, int shift) {
    static const char map[128] = {
        0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
        '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
        'a','s','d','f','g','h','j','k','l',';','\'','`', 0,'\\',
        'z','x','c','v','b','n','m',',','.','/', 0,'*', 0,' ',
    };

    static const char shift_map[128] = {
        0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
        '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0,
        'A','S','D','F','G','H','J','K','L',':','"','~', 0,'|',
        'Z','X','C','V','B','N','M','<','>','?', 0,'*', 0,' ',
    };

    if (sc < sizeof(map))
        return shift ? shift_map[sc] : map[sc];
    return 0;
}

uint8_t keyboard_read_scancode() {
    while (1) {
        if (inb(PS2_STATUS) & 0x01) {
            return inb(PS2_DATA);
        }
    }
}

/* --- Move cursor to the bottom-most empty line or scroll if none --- */
void move_to_last_line() {
    volatile uint16_t* vga = VGA_ADDR;
    for (int row = VGA_HEIGHT - 1; row >= 0; row--) {
        int empty = 1;
        for (int col = 0; col < VGA_WIDTH; col++) {
            uint16_t val = vga[row * VGA_WIDTH + col];
            if ((val & 0xFF) != ' ') {
                empty = 0;
                break;
            }
        }
        if (empty) {
            cursor_row = row;
            cursor_col = 0;
            move_cursor(cursor_row, cursor_col);
            return;
        }
    }
    scroll();
    cursor_row = VGA_HEIGHT - 1;
    cursor_col = 0;
    move_cursor(cursor_row, cursor_col);
}

void print_kinfo(const char* msg) {
	set_color("white");
	set_bgcolor("red");
	print_kinfo_raw(msg);
	set_color("white");
	set_bgcolor("black");
}

/* --- Command Handling --- */
void print_help() {
    print("minitkernel is a \"kernel\" made to learn C and Assembly.\n");
    print("You can currently run:\n");
    print("    help: Shows help\n");
    print("    version: Displays version info\n");
    print("    echo: Echo's whatever is typed after it\n");
    print("    poweroff: Power off the computer\n");
    print("    exit: Exits the shell loop\n");
    print("Thanks for using minitkernel!\n");
}

void print_version() {
    print("minitkernel builtin shell ver 0.0.1 running on minitkernel ver 0.0.1\n");
}

void kernel_panic(const char* printstderror, const char* printdeb, const char* printstdpanic) {
    clear_screen();
    set_color("white");
    set_bgcolor("blue");
    print(printstderror);
    serial_print(printstderror);
    print(printdeb);
    serial_print(printdeb);
    set_color("white");
    set_bgcolor("blue");
    print(printstdpanic);
    serial_print(printstdpanic);
    print("FATAL ERROR!\n");
    serial_print("FATAL ERROR!\n");
    // sleep(1000); Longer/shorter on different speed systems. Disabled until Timer implemented.
    print("Trying to reexec minitkernel shell...\n");
    serial_print("Trying to reexec minitkernel shell...\n");
    print("Successfully restarted minitkernel builtin shell ver 0.0.1\n");
    serial_print("Successfully restarted minitkernel builtin shell ver 0.0.1\n");
    set_color("white");
    set_bgcolor("black");
    shell_running = 1;
}

void run_animation() {
    clear_screen();
    print("                                   THIS IS                                      ");
    print("                                    A COOL                                      ");
    print("                                   ANIMATION                                    ");
    print("                                    FOR MY                                      ");
    print("                                MINIMAL KERNEL                                  ");
    print("                                 JUST KIDDING                                   ");
    print("                            I'M TOO LAZY TO DO THAT                             ");
    print("                       I WOULD HAVE TO IMPLEMENT TIMERS                         ");

}

void handle_command(const char* cmd) {
    print_kinfo("Executing command: ");
    print_kinfo(cmd);
    print_kinfo("\n");

    if (cmd[0] == '\0') return;

    if (cmd[0] == 'h' && cmd[1] == 'e' && cmd[2] == 'l' && cmd[3] == 'p' &&
        (cmd[4] == ' ' || cmd[4] == '\0')) {
        print_help();
    }
    else if (cmd[0] == 'e' && cmd[1] == 'c' && cmd[2] == 'h' && cmd[3] == 'o' &&
        (cmd[4] == ' ' || cmd[4] == '\0')) {
        const char* msg = cmd + 4;
        while (*msg == ' ') msg++;
        print(msg);
        print("\n");
        print_kinfo("Printed: ");
        print_kinfo(msg);
        print_kinfo("\n");
    }
    else if (cmd[0] == 'v' && cmd[1] == 'e' && cmd[2] == 'r' && cmd[3] == 's' && cmd[4] == 'i' && cmd[5] == 'o' && cmd[6] == 'n' &&
        (cmd[7] == ' ' || cmd[7] == '\0')) {
        print_version();
    }
    else if (cmd[0] == 'e' && cmd[1] == 'x' && cmd[2] == 'i' && cmd[3] == 't' &&
        (cmd[4] == ' ' || cmd[4] == '\0')) {
        shell_running = 0;
        kernel_panic("FATAL: KERNEL PANIC!\n", "ERROR: Tried to KILL shell, panicing\n", "FATAL: PANIC:\n00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000\n00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000\n00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000\n00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000\n00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000\n00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000\n00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000\n00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000\n00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000\n00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000\n00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000\n00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000\n00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000\n00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000\n00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000\n00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000\n");
    }
    else if (strncmp(cmd, "setcolor", 8) == 0 &&
            (cmd[8] == ' ' || cmd[8] == '\0')) {
        const char* color = cmd + 8;
        while (*color == ' ') color++;
        set_color(color);
        print("Text color set!\n");
        }
    else if (strncmp(cmd, "setbgcolor", 10) == 0 &&
            (cmd[10] == ' ' || cmd[10] == '\0')) {
        const char* color = cmd + 10;
        while (*color == ' ') color++;
        set_bgcolor(color);
        print("Background color set!\n");
    }
    else if (strncmp(cmd, "poweroff", 8) == 0 && (cmd[8] == '\0' || cmd[8] == ' ')) {
        print("Powering off.\n");
        power_off_qemu();
        print("Poweroff failed, are you on real hardware?\n");
    }
    else if (strncmp(cmd, "animation", 9) == 0 && (cmd[9] == '\0' || cmd[9] == ' ')) {
        run_animation();
    }
    else {
        print("Unknown command: ");
        print(cmd);
        print("\n");
        print_kinfo("Unknown command: ");
        print_kinfo(cmd);
        print_kinfo("\n");
    }
}

/* --- Input shell --- */
void shell_prompt() {
    move_to_last_line();
    set_color("white");
    set_bgcolor("green");
    print("minitkernel:/>");
    set_color("white");
    set_bgcolor("black");
    print(" ");
    input_len = 0;

    while (shell_running) {
        uint8_t sc = keyboard_read_scancode();

        // Handle Shift press/release
        if (sc == 0x2A || sc == 0x36) {  // Left or Right Shift press
            shift_pressed = 1;
            continue;
        }
        if (sc == 0xAA || sc == 0xB6) {  // Left or Right Shift release
            shift_pressed = 0;
            continue;
        }

        // Ignore key releases
        if (sc & 0x80) continue;

        char c = scancode_to_char(sc, shift_pressed);


        if (c == '\n') {
            vga_putc('\n');
            input_buffer[input_len] = '\0';
            handle_command(input_buffer);
            if (!shell_running) break;
            move_to_last_line();
            set_color("white");
            set_bgcolor("green");
            print("minitkernel:/>");
            set_color("white");
            set_bgcolor("black");
            print(" ");
            input_len = 0;
        }
        else if (c == '\b') {
            if (input_len > 0) {
                input_len--;
                vga_putc('\b');
            }
        }
        else if (c && input_len < MAX_INPUT - 1) {
            input_buffer[input_len++] = c;
            vga_putc(c);
        }
    }
}

/* --- Kernel entry point --- */
void kernel_main() {
    serial_init();

    print_kinfo("Booting Kernel: minitkernel 0.0.1\n");
    print_kinfo("Clearing VGA Screen\n");

    clear_screen();

    print_kinfo("Cleared VGA Screen\n");
    print_kinfo("Starting Shell: Builtin 0.0.1\n");

    print("Welcome to the minitkernel builtin shell version 0.0.1!\n");
    print("Type 'help' for some help!\n");

    shell_running = 1;
    while (shell_running) {
        shell_prompt();
    }
}
