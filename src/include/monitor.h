
#ifndef MONITOR_H
#define MONITOR_H

#include <stdint.h>
#include <stdarg.h>

// Write a single character out to the screen.
extern void monitor_put(char c);

// Clear the screen to all black.
extern void monitor_clear();

// Output a null-terminated ASCII string to the monitor.
extern void monitor_write_func(char *c);

extern void move_cursor();

extern void scroll();

extern void monitor_put(char c);

extern void monitor_write_hex(uint32_t n);

extern void monitor_write_dec(uint32_t n);

extern void monitor_printf(char *format, ...);

extern uint8_t foreground_color;
extern uint8_t background_color;

extern uint8_t cursor_x;
extern uint8_t cursor_y;

extern uint16_t *video_memory;

#define monitor_write(c) do { \
    int i = 0; \
    while(c[i]) { \
        monitor_put(c[i++]); \
    } \
} while (0)

#endif // MONITOR_H