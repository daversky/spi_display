#ifndef ST7789_INIT_H
#define ST7789_INIT_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t cmd;
    size_t data_len;
    const uint8_t *data;
    uint16_t delay_ms;
} st7789_init_cmd_t;

static const uint8_t st7789_d0[] = {0x00};
static const uint8_t st7789_d1[] = {0x55};
static const uint8_t st7789_d2[] = {0x05, 0x3C, 0x3C, 0x05, 0x3C, 0x3C};
static const uint8_t st7789_d3[] = {0x05, 0x14};
static const uint8_t st7789_d4[] = {0x0A, 0x0C};
static const uint8_t st7789_d5[] = {0x0E, 0x0F, 0x08, 0x0A, 0x0D, 0x00};

static const st7789_init_cmd_t st7789_init_sequence[] = {
    {0x01, 0, NULL, 150},
    {0x11, 0, NULL, 50},
    {0x36, 1, st7789_d0, 0},
    {0x3A, 1, st7789_d1, 0},
    {0xB2, 6, st7789_d2, 0},
    {0xB7, 1, st7789_d0, 0},
    {0xBB, 1, st7789_d0, 0},
    {0xC0, 1, st7789_d4, 0},
    {0xC2, 2, st7789_d3, 0},
    {0xC3, 2, st7789_d4, 0},
    {0xC4, 2, st7789_d4, 0},
    {0xC6, 1, st7789_d0, 0},
    {0xD0, 2, st7789_d5, 0},
    {0x21, 0, NULL, 0},
    {0x29, 0, NULL, 100},
};

#define ST7789_INIT_SEQUENCE_LEN (sizeof(st7789_init_sequence) / sizeof(st7789_init_cmd_t))

#endif