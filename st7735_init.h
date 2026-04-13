#ifndef ST7735_INIT_H
#define ST7735_INIT_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t cmd;
    size_t data_len;
    const uint8_t *data;
    uint16_t delay_ms;
} st7735_init_cmd_t;

static const uint8_t st7735_d0[] = {0x01, 0x2C, 0x2D};
static const uint8_t st7735_d1[] = {0x01, 0x2C, 0x2D};
static const uint8_t st7735_d2[] = {0xA2, 0xA0, 0xA8, 0x2B, 0x2A};
static const uint8_t st7735_d3[] = {0x01};
static const uint8_t st7735_d4[] = {0x05, 0x3C, 0x3C, 0x05, 0x3C, 0x3C};
static const uint8_t st7735_d5[] = {0x05, 0x14};
static const uint8_t st7735_d6[] = {0x0E, 0x0F, 0x08, 0x0A, 0x0D, 0x00};
static const uint8_t st7735_d7[] = {0x0A, 0x0C};
static const uint8_t st7735_d8[] = {0x2C};
static const uint8_t st7735_d9[] = {0x3C, 0x2D, 0x36, 0x3F, 0x3C};
static const uint8_t st7735_d10[] = {0x32, 0x3F, 0x3F};
static const uint8_t st7735_d11[] = {0x55};

static const st7735_init_cmd_t st7735_init_sequence[] = {
    {0x01, 0, NULL, 150},
    {0x11, 0, NULL, 50},
    {0xB1, 3, st7735_d0, 0},
    {0xB2, 3, st7735_d1, 0},
    {0xB3, 6, st7735_d2, 0},
    {0xB4, 1, st7735_d3, 0},
    {0xC0, 6, st7735_d4, 0},
    {0xC1, 2, st7735_d5, 0},
    {0xC2, 2, st7735_d7, 0},
    {0xC3, 2, st7735_d7, 0},
    {0xC4, 2, st7735_d7, 0},
    {0xC5, 2, st7735_d6, 0},
    {0x36, 1, st7735_d8, 0},
    {0x3A, 1, st7735_d11, 0},
    {0x2A, 4, st7735_d9, 0},
    {0x2B, 4, st7735_d10, 0},
    {0x13, 0, NULL, 0},
    {0x29, 0, NULL, 100},
};

#define ST7735_INIT_SEQUENCE_LEN (sizeof(st7735_init_sequence) / sizeof(st7735_init_cmd_t))

#endif