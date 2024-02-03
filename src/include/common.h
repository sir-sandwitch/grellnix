#ifndef COMMON_H
#define COMMON_H
#include <stdint.h>
#include <monitor.h>
#include <stdbool.h>

// static inline void PANIC(const char* msg) {
//     monitor_printf("%s at %s:%d\n", msg, __FILE__, __LINE__);
//     while(1) {}
// }

#define PANIC(msg) \
    monitor_printf("%s at %s:%d\n", msg, __FILE__, __LINE__); \
    while(1) {}

#define ASSERT(b) \
    if (!(b)){ \
        PANIC("ASSERTION FAILED: " #b) \
    }

extern void outb(uint16_t port, uint8_t value);
extern uint8_t inb(uint16_t port);
extern uint16_t inw(uint16_t port);

extern void memcpy(void *dest, const uint8_t *src, uint32_t len);
extern void memset(void *dest, uint8_t val, uint32_t len);
extern int strlen(const char *str);
extern int strcmp(const char *str1, const char *str2);
extern void strcpy(char *dest, const char *src);

#endif