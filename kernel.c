#include <stdint.h>
#include <stddef.h>
#include <font8x8_basic.h>

#define VGA_MODE13_WIDTH  320
#define VGA_MODE13_HEIGHT 200
#define VGA_MODE13_ADDR   ((uint8_t*)0xA0000)
#define VGA_COMMAND_PORT 0x3D4
#define VGA_DATA_PORT    0x3D5
#define COM1_PORT 0x3F8
#define PS2_STATUS 0x64
#define PS2_DATA   0x60
#define PIT_CHANNEL0_DATA_PORT 0x40
#define PIT_COMMAND_PORT 0x43
#define PIT_FREQUENCY 1193182UL
#define ABS(x) ((x) < 0 ? -(x) : (x))

char input_buffer[4096];
int input_len = 0;

void handle_command(const char* cmd);

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (unsigned char)(*s1) - (unsigned char)(*s2);
}

int strncmp(const char* s1, const char* s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i] || s1[i] == '\0' || s2[i] == '\0')
            return (unsigned char)s1[i] - (unsigned char)s2[i];
    }
    return 0;
}
char* strcpy(char* dest, const char* src) {
    char* orig = dest;
    while ((*dest++ = *src++));
    return orig;
}

char* strncpy(char* dest, const char* src, int n) {
    char* orig = dest;
    int i = 0;
    while (i < n && src[i]) {
        dest[i] = src[i];
        i++;
    }
    while (i < n) {
        dest[i++] = '\0';
    }
    return orig;
}

unsigned long long __udivdi3(unsigned long long dividend, unsigned long long divisor) {
    if (divisor == 0) return 0xFFFFFFFFFFFFFFFFULL;

    unsigned long long quotient = 0;
    unsigned long long remainder = 0;

    for (int i = 63; i >= 0; i--) {
        remainder <<= 1;
        remainder |= (dividend >> i) & 1ULL;
        if (remainder >= divisor) {
            remainder -= divisor;
            quotient |= 1ULL << i;
        }
    }

    return quotient;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint64_t rdtsc() {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

void pit_write(uint16_t value) {
    outb(PIT_COMMAND_PORT, 0x34);
    outb(PIT_CHANNEL0_DATA_PORT, value & 0xFF);
    outb(PIT_CHANNEL0_DATA_PORT, value >> 8);
}

void pit_latch() {
    outb(PIT_COMMAND_PORT, 0x00);
}

uint16_t pit_read_counter() {
    pit_latch();
    uint8_t lsb = inb(PIT_CHANNEL0_DATA_PORT);
    uint8_t msb = inb(PIT_CHANNEL0_DATA_PORT);
    return ((uint16_t)msb << 8) | lsb;
}

void pit_wait_count(uint16_t target_count) {
    while (pit_read_counter() > target_count);
}

uint64_t measure_cpu_frequency() {
    pit_write(0xFFFF);

    uint64_t start = rdtsc();

    pit_wait_count(0xF000);

    uint64_t end = rdtsc();

    uint16_t count_elapsed = 0xFFFF - pit_read_counter();

    uint64_t time_us = ((uint64_t)count_elapsed * 1000000ULL) / PIT_FREQUENCY;

    if (time_us == 0) return 0;

    uint64_t cpu_freq = ((end - start) * 1000000ULL) / time_us;

    return cpu_freq;
}

void set_vga_mode_13() {
    asm volatile ("cli");
    outb(0x3C2, 0x63);

    outb(0x3C4, 0x00); outb(0x3C5, 0x03);
    outb(0x3C4, 0x01); outb(0x3C5, 0x01);
    outb(0x3C4, 0x02); outb(0x3C5, 0x0F);
    outb(0x3C4, 0x03); outb(0x3C5, 0x00);
    outb(0x3C4, 0x04); outb(0x3C5, 0x0E);

    outb(0x3D4, 0x11); outb(0x3D5, 0x0E);
    static const uint8_t crtc_vals[] = {
        0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
        0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3, 0xFF
    };
    for (uint8_t i = 0; i < sizeof(crtc_vals); i++) {
        outb(0x3D4, i);
        outb(0x3D5, crtc_vals[i]);
    }

    outb(0x3CE, 0x00); outb(0x3CF, 0x00);
    outb(0x3CE, 0x01); outb(0x3CF, 0x00);
    outb(0x3CE, 0x02); outb(0x3CF, 0x00);
    outb(0x3CE, 0x03); outb(0x3CF, 0x00);
    outb(0x3CE, 0x04); outb(0x3CF, 0x00);
    outb(0x3CE, 0x05); outb(0x3CF, 0x40);
    outb(0x3CE, 0x06); outb(0x3CF, 0x05);

    (void)inb(0x3DA);
    for (uint8_t i = 0; i < 16; i++) {
        outb(0x3C0, i);
        outb(0x3C0, i);
    }
    outb(0x3C0, 0x10); outb(0x3C0, 0x41);
    outb(0x3C0, 0x11); outb(0x3C0, 0x00);
    outb(0x3C0, 0x12); outb(0x3C0, 0x0F);
    outb(0x3C0, 0x13); outb(0x3C0, 0x00);
    outb(0x3C0, 0x14); outb(0x3C0, 0x00);

    outb(0x3C0, 0x20);
}

void serial_init() {
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x80);
    outb(COM1_PORT + 0, 0x03);
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03);
    outb(COM1_PORT + 2, 0xC7);
    outb(COM1_PORT + 4, 0x0B);
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

char keyboard_poll() {
    uint8_t status = inb(PS2_STATUS);
    if ((status & 0x01) && !(status & 0x20)) {
        uint8_t sc = inb(PS2_DATA);

        if (sc == 0x2A || sc == 0x36) {
            shift_pressed = 1;
            return 0;
        } else if (sc == 0xAA || sc == 0xB6) {
            shift_pressed = 0;
            return 0;
        }

        if (sc & 0x80) return 0;

        return scancode_to_char(sc, shift_pressed);
    }
    return 0;
}

void mouse_wait() {
    while (inb(PS2_STATUS) & 0x02);
}

void mouse_write(uint8_t val) {
    mouse_wait();
    outb(PS2_STATUS, 0xD4);
    mouse_wait();
    outb(PS2_DATA, val);
}

uint8_t mouse_read() {
    while (!(inb(PS2_STATUS) & 0x01));
    return inb(PS2_DATA);
}

void mouse_init() {
    mouse_write(0xF4);
    mouse_read();
}

extern const uint8_t font8x8_basic[128][8];

void put_pixel(int x, int y, uint8_t color) {
    if (x < 0 || x >= VGA_MODE13_WIDTH || y < 0 || y >= VGA_MODE13_HEIGHT) return;
    uint8_t* vga = (uint8_t*)VGA_MODE13_ADDR;
    vga[y * VGA_MODE13_WIDTH + x] = color;
}

void fill_screen(uint8_t color) {
    uint8_t* vga = (uint8_t*)VGA_MODE13_ADDR;
    for (int i = 0; i < VGA_MODE13_WIDTH * VGA_MODE13_HEIGHT; i++) {
        vga[i] = color;
    }
}

void draw_char(uint8_t c, int x, int y, uint8_t color) {
    if (c >= 128) return;
    const uint8_t* glyph = font8x8_basic[c];
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if ((glyph[row] >> col) & 1) {
                put_pixel(x + col, y + row, color);
            }
        }
    }
}

void draw_string(const char* s, int x, int y, uint8_t color) {
    while (*s) {
        draw_char(*s++, x, y, color);
        x += 8;
    }
}

void clear_screen() {
    fill_screen(0x00);
}

void draw_box(int topleftx, int toplefty, int bottomrightx, int bottomrighty, uint8_t color) {
    if (topleftx < 0) topleftx = 0;
    if (toplefty < 0) toplefty = 0;
    if (bottomrightx >= VGA_MODE13_WIDTH) bottomrightx = VGA_MODE13_WIDTH - 1;
    if (bottomrighty >= VGA_MODE13_HEIGHT) bottomrighty = VGA_MODE13_HEIGHT - 1;

    for (int y = toplefty; y <= bottomrighty; y++) {
        for (int x = topleftx; x <= bottomrightx; x++) {
            put_pixel(x, y, color);
        }
    }
}

void sort_vertices(int* x0, int* y0, int* x1, int* y1, int* x2, int* y2) {
    if (*y0 > *y1) { int t=*y0; *y0=*y1; *y1=t; t=*x0; *x0=*x1; *x1=t; }
    if (*y1 > *y2) { int t=*y1; *y1=*y2; *y2=t; t=*x1; *x1=*x2; *x2=t; }
    if (*y0 > *y1) { int t=*y0; *y0=*y1; *y1=t; t=*x0; *x0=*x1; *x1=t; }
}

void draw_line(int x0, int y0, int x1, int y1, uint8_t color) {
    int dx = (x1 > x0 ? x1 - x0 : x0 - x1);
    int dy = (y1 > y0 ? y1 - y0 : y0 - y1);
    int sx = (x0 < x1 ? 1 : -1);
    int sy = (y0 < y1 ? 1 : -1);
    int err = dx - dy;

    while (1) {
        put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

void fill_flat_bottom_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t color) {
    int invslope1 = (x1 - x0) * 1000 / (y1 - y0);
    int invslope2 = (x2 - x0) * 1000 / (y2 - y0);

    int curx1 = x0 * 1000;
    int curx2 = x0 * 1000;

    for (int y = y0; y <= y1; y++) {
        int x_start = curx1 / 1000;
        int x_end   = curx2 / 1000;
        if (x_start > x_end) {
            int t = x_start; x_start = x_end; x_end = t;
        }
        for (int x = x_start; x <= x_end; x++) {
            put_pixel(x, y, color);
        }
        curx1 += invslope1;
        curx2 += invslope2;
    }
}

void fill_flat_top_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t color) {
    int invslope1 = (x2 - x0) * 1000 / (y2 - y0);
    int invslope2 = (x2 - x1) * 1000 / (y2 - y1);

    int curx1 = x2 * 1000;
    int curx2 = x2 * 1000;

    for (int y = y2; y >= y0; y--) {
        int x_start = curx1 / 1000;
        int x_end   = curx2 / 1000;
        if (x_start > x_end) {
            int t = x_start; x_start = x_end; x_end = t;
        }
        for (int x = x_start; x <= x_end; x++) {
            put_pixel(x, y, color);
        }
        curx1 -= invslope1;
        curx2 -= invslope2;
    }
}

void draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t color) {
    sort_vertices(&x0, &y0, &x1, &y1, &x2, &y2);

    if (y1 == y2) {
        fill_flat_bottom_triangle(x0, y0, x1, y1, x2, y2, color);
    } else if (y0 == y1) {
        fill_flat_top_triangle(x0, y0, x1, y1, x2, y2, color);
    } else {
        int x3 = x0 + ((y1 - y0) * (x2 - x0)) / (y2 - y0);
        int y3 = y1;
        fill_flat_bottom_triangle(x0, y0, x1, y1, x3, y3, color);
        fill_flat_top_triangle(x1, y1, x3, y3, x2, y2, color);
    }
}

int mouse_x = VGA_MODE13_WIDTH / 2;
int mouse_y = VGA_MODE13_HEIGHT / 2;

void draw_mouse_cursor(int x, int y, uint8_t color) {
    put_pixel(x, y + 2, color);
    put_pixel(x, y + 3, color);
    put_pixel(x, y + 4, color);
    put_pixel(x, y + 5, color);
    put_pixel(x, y + 6, color);
    put_pixel(x, y + 7, color);
    put_pixel(x, y + 8, color);
    put_pixel(x, y + 9, color);
    put_pixel(x, y + 10, color);
    put_pixel(x + 1, y + 3, color);
    put_pixel(x + 1, y + 4, color);
    put_pixel(x + 1, y + 5, color);
    put_pixel(x + 1, y + 6, color);
    put_pixel(x + 1, y + 7, color);
    put_pixel(x + 1, y + 8, color);
    put_pixel(x + 1, y + 9, color);
    put_pixel(x + 2, y + 4, color);
    put_pixel(x + 2, y + 5, color);
    put_pixel(x + 2, y + 6, color);
    put_pixel(x + 2, y + 7, color);
    put_pixel(x + 2, y + 8, color);
    put_pixel(x + 2, y + 9, color);
    put_pixel(x + 3, y + 5, color);
    put_pixel(x + 3, y + 6, color);
    put_pixel(x + 3, y + 7, color);
    put_pixel(x + 3, y + 8, color);
    put_pixel(x + 3, y + 9, color);
    put_pixel(x + 3, y + 10, color);
    put_pixel(x + 4, y + 6, color);
    put_pixel(x + 4, y + 7, color);
    put_pixel(x + 4, y + 8, color);
    put_pixel(x + 4, y + 9, color);
    put_pixel(x + 4, y + 10, color);
    put_pixel(x + 4, y + 11, color);
    put_pixel(x + 5, y + 7, color);
    put_pixel(x + 5, y + 8, color);
    put_pixel(x + 5, y + 10, color);
    put_pixel(x + 5, y + 11, color);
    put_pixel(x + 5, y + 12, color);
    put_pixel(x + 6, y + 8, color);
    put_pixel(x + 6, y + 11, color);
    put_pixel(x + 6, y + 12, color);
}

void draw_mouse_cursor_outline(int x, int y, uint8_t color) {
    put_pixel(x, y + 1, color);
    put_pixel(x - 1, y + 2, color);
    put_pixel(x - 1, y + 3, color);
    put_pixel(x - 1, y + 4, color);
    put_pixel(x - 1, y + 5, color);
    put_pixel(x - 1, y + 6, color);
    put_pixel(x - 1, y + 7, color);
    put_pixel(x - 1, y + 8, color);
    put_pixel(x - 1, y + 9, color);
    put_pixel(x - 1, y + 10, color);
    put_pixel(x - 1, y + 11, color);
    put_pixel(x, y + 11, color);
    put_pixel(x + 1, y + 10, color);
    put_pixel(x + 2, y + 10, color);
    put_pixel(x + 3, y + 11, color);
    put_pixel(x + 4, y + 12, color);
    put_pixel(x + 5, y + 13, color);
    put_pixel(x + 6, y + 13, color);
    put_pixel(x + 7, y + 12, color);
    put_pixel(x + 7, y + 11, color);
    put_pixel(x + 6, y + 10, color);
    put_pixel(x + 5, y + 9, color);
    put_pixel(x + 6, y + 9, color);
    put_pixel(x + 7, y + 8, color);
    put_pixel(x + 6, y + 7, color);
    put_pixel(x + 5, y + 6, color);
    put_pixel(x + 4, y + 5, color);
    put_pixel(x + 3, y + 4, color);
    put_pixel(x + 2, y + 3, color);
    put_pixel(x + 1, y + 2, color);
    put_pixel(x - 1, y + 1, color);
}

void erase_mouse_cursor(int x, int y) {
    draw_mouse_cursor(x, y, 0);
}

void mouse_poll() {
    if ((inb(PS2_STATUS) & 0x21) != 0x21) return;

    static uint8_t cycle = 0;
    static uint8_t packet[3];

    packet[cycle++] = inb(PS2_DATA);

    if (cycle < 3) return;

    cycle = 0;

    int dx = (int8_t)packet[1];
    int dy = (int8_t)packet[2];

    erase_mouse_cursor(mouse_x, mouse_y);

    mouse_x += dx;
    mouse_y -= dy;

    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x >= VGA_MODE13_WIDTH) mouse_x = VGA_MODE13_WIDTH - 1;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y >= VGA_MODE13_HEIGHT) mouse_y = VGA_MODE13_HEIGHT - 1;

    draw_mouse_cursor(mouse_x, mouse_y, 0x3F);
}

int boxi = -20;

void kernel_main() {
    serial_init();
    serial_print("Measuring CPU frequency...\n");
    uint64_t cpu_freq = measure_cpu_frequency();

    uint64_t ticks_per_ms = cpu_freq / 1000;
    uint64_t last_render_time = rdtsc();

    serial_print("Serial works! Trying VGA 0x13...\n");
    set_vga_mode_13();
    serial_print("VGA works! Trying mouse...\n");
    mouse_init();
    serial_print("Mouse works! Continuing...\n");

    clear_screen();

    int boxi = -20;
    int direction = 1;

    while (1) {
        char c = keyboard_poll();
        if (c) {
            
        }

        mouse_poll();

        uint64_t now = rdtsc();
        if ((now - last_render_time) >= ticks_per_ms * 33) {
            last_render_time = now;

            draw_box(0, 0, 320, 200, 0x38);
            draw_box(0, 0, 320, 12, 0x3F);
            draw_string("minitkernel 0.0.2   ABC   Hello, World!", 4, 3, 0x00);
            draw_box(0, 188, 320, 200, 0x3F);
            draw_string("0.0.2                              test", 4, 190, 0x00);
            
            if (boxi >= 20) {
                direction = -1;
            } else if (boxi <= -20) {
                direction = 1;
            }
            boxi += direction;
            draw_box(40 + boxi, 60, 100 + boxi, 120, 0x04);
            draw_triangle(260, 75 + boxi, 230, 125 + boxi, 290, 125 + boxi, 0x06);
            draw_box(145 - boxi/2, 85 - boxi/2, 185 + boxi/2, 125 + boxi/2, 0x08);
            draw_mouse_cursor(mouse_x, mouse_y, 0x3F);
            draw_mouse_cursor_outline(mouse_x, mouse_y, 0x00);
        }
    }
}
