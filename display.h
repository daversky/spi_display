#ifndef MICROPYTHON_DISPLAY_H
#define MICROPYTHON_DISPLAY_H

#include "py/runtime.h"
#include "py/objarray.h"
#include "py/mphal.h"

typedef struct _display_obj_t {
    mp_obj_base_t base;
    mp_obj_t spi_obj;
    mp_obj_t spi_write_method;
    mp_hal_pin_obj_t pin_dc;
    mp_hal_pin_obj_t pin_cs;
    mp_hal_pin_obj_t pin_rst;
    mp_hal_pin_obj_t pin_bl;
    uint16_t width;
    uint16_t height;
    bool backlight_active_high;
} display_obj_t;

extern const mp_obj_type_t display_type;

void display_cmd(display_obj_t *self, uint8_t cmd);
void display_cmd_data(display_obj_t *self, uint8_t cmd, const uint8_t *data, size_t len);
void display_show(display_obj_t *self, const uint8_t *buffer, size_t len);
void display_update_rect(display_obj_t *self, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *buffer, size_t len);
void display_set_backlight(display_obj_t *self, uint8_t percent);

#endif