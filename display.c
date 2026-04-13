#include "display.h"
#include <string.h>

static void _spi_write(display_obj_t *self, const uint8_t *data, size_t len) {
    mp_obj_t data_obj = mp_obj_new_bytes(data, len);
    mp_call_function_1(self->spi_write_method, data_obj);
}

void display_cmd(display_obj_t *self, uint8_t cmd) {
    mp_hal_pin_write(self->pin_dc, 0);
    if (self->pin_cs != (mp_hal_pin_obj_t)-1) mp_hal_pin_write(self->pin_cs, 0);
    _spi_write(self, &cmd, 1);
    if (self->pin_cs != (mp_hal_pin_obj_t)-1) mp_hal_pin_write(self->pin_cs, 1);
}

void display_cmd_data(display_obj_t *self, uint8_t cmd, const uint8_t *data, size_t len) {
    mp_hal_pin_write(self->pin_dc, 0);
    if (self->pin_cs != (mp_hal_pin_obj_t)-1) mp_hal_pin_write(self->pin_cs, 0);
    _spi_write(self, &cmd, 1);
    if (len > 0) {
        mp_hal_pin_write(self->pin_dc, 1);
        _spi_write(self, data, len);
    }
    if (self->pin_cs != (mp_hal_pin_obj_t)-1) mp_hal_pin_write(self->pin_cs, 1);
}

static void _set_window(display_obj_t *self, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t x2 = x + w - 1;
    uint16_t y2 = y + h - 1;

    uint8_t caset_data[] = {x >> 8, x & 0xFF, x2 >> 8, x2 & 0xFF};
    display_cmd_data(self, 0x2A, caset_data, 4);

    uint8_t raset_data[] = {y >> 8, y & 0xFF, y2 >> 8, y2 & 0xFF};
    display_cmd_data(self, 0x2B, raset_data, 4);

    display_cmd(self, 0x2C);
}

void display_show(display_obj_t *self, const uint8_t *buffer, size_t len) {
    size_t expected_len = (size_t)self->width * self->height * 2;
    if (len != expected_len) return;

    _set_window(self, 0, 0, self->width, self->height);

    mp_hal_pin_write(self->pin_dc, 1);
    if (self->pin_cs != (mp_hal_pin_obj_t)-1) mp_hal_pin_write(self->pin_cs, 0);
    _spi_write(self, buffer, len);
    if (self->pin_cs != (mp_hal_pin_obj_t)-1) mp_hal_pin_write(self->pin_cs, 1);
}

void display_update_rect(display_obj_t *self, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *buffer, size_t len) {
    if (x >= self->width || y >= self->height) return;
    if (x + w > self->width) w = self->width - x;
    if (y + h > self->height) h = self->height - y;
    if (w == 0 || h == 0) return;

    _set_window(self, x, y, w, h);

    mp_hal_pin_write(self->pin_dc, 1);
    if (self->pin_cs != (mp_hal_pin_obj_t)-1) mp_hal_pin_write(self->pin_cs, 0);
    _spi_write(self, buffer, len);
    if (self->pin_cs != (mp_hal_pin_obj_t)-1) mp_hal_pin_write(self->pin_cs, 1);
}

void display_set_backlight(display_obj_t *self, uint8_t percent) {
    if (self->pin_bl == (mp_hal_pin_obj_t)-1) return;
    if (percent > 100) percent = 100;

    if (self->backlight_active_high)
        mp_hal_pin_write(self->pin_bl, percent > 0 ? 1 : 0);
    else
        mp_hal_pin_write(self->pin_bl, percent > 0 ? 0 : 1);
}

// Конструктор
static mp_obj_t display_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum { ARG_spi, ARG_width, ARG_height, ARG_dc, ARG_cs, ARG_rst, ARG_bl, ARG_backlight_active_high };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_spi, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_width, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_height, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_dc, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_cs, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_rst, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_bl, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_backlight_active_high, MP_ARG_BOOL, {.u_bool = true} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    display_obj_t *self = mp_obj_malloc(display_obj_t, &display_type);

    self->spi_obj = args[ARG_spi].u_obj;
    self->spi_write_method = mp_load_attr(self->spi_obj, MP_QSTR_write);

    self->width = args[ARG_width].u_int;
    self->height = args[ARG_height].u_int;

    self->pin_dc = mp_hal_get_pin_obj(args[ARG_dc].u_obj);
    mp_hal_pin_output(self->pin_dc);

    self->pin_cs = (args[ARG_cs].u_obj != mp_const_none) ?
                   mp_hal_get_pin_obj(args[ARG_cs].u_obj) : (mp_hal_pin_obj_t)-1;
    if (self->pin_cs != (mp_hal_pin_obj_t)-1) {
        mp_hal_pin_output(self->pin_cs);
        mp_hal_pin_write(self->pin_cs, 1);
    }

    self->pin_rst = (args[ARG_rst].u_obj != mp_const_none) ?
                    mp_hal_get_pin_obj(args[ARG_rst].u_obj) : (mp_hal_pin_obj_t)-1;
    if (self->pin_rst != (mp_hal_pin_obj_t)-1) {
        mp_hal_pin_output(self->pin_rst);
        mp_hal_pin_write(self->pin_rst, 0);
        mp_hal_delay_ms(5);
        mp_hal_pin_write(self->pin_rst, 1);
        mp_hal_delay_ms(150);
    }

    self->pin_bl = (args[ARG_bl].u_obj != mp_const_none) ?
                   mp_hal_get_pin_obj(args[ARG_bl].u_obj) : (mp_hal_pin_obj_t)-1;
    if (self->pin_bl != (mp_hal_pin_obj_t)-1) {
        mp_hal_pin_output(self->pin_bl);
        mp_hal_pin_write(self->pin_bl, 0);
    }

    self->backlight_active_high = args[ARG_backlight_active_high].u_bool;

    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t display_cmd_obj(mp_obj_t self_in, mp_obj_t cmd_in) {
    display_cmd(MP_OBJ_TO_PTR(self_in), mp_obj_get_int(cmd_in));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(display_cmd_obj_obj, display_cmd_obj);

static mp_obj_t display_cmd_data_obj(mp_obj_t self_in, mp_obj_t cmd_in, mp_obj_t data_in) {
    display_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(data_in, &bufinfo, MP_BUFFER_READ);
    display_cmd_data(self, mp_obj_get_int(cmd_in), bufinfo.buf, bufinfo.len);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(display_cmd_data_obj_obj, display_cmd_data_obj);

static mp_obj_t display_show_obj(mp_obj_t self_in, mp_obj_t buffer_in) {
    display_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buffer_in, &bufinfo, MP_BUFFER_READ);
    display_show(self, bufinfo.buf, bufinfo.len);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(display_show_obj_obj, display_show_obj);

static mp_obj_t display_update_rect_obj(size_t n_args, const mp_obj_t *args) {
    display_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[5], &bufinfo, MP_BUFFER_READ);
    display_update_rect(self,
        mp_obj_get_int(args[1]), mp_obj_get_int(args[2]),
        mp_obj_get_int(args[3]), mp_obj_get_int(args[4]),
        bufinfo.buf, bufinfo.len);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(display_update_rect_obj_obj, 6, 6, display_update_rect_obj);

static mp_obj_t display_set_backlight_obj(mp_obj_t self_in, mp_obj_t percent_in) {
    display_set_backlight(MP_OBJ_TO_PTR(self_in), mp_obj_get_int(percent_in));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(display_set_backlight_obj_obj, display_set_backlight_obj);

static const mp_rom_map_elem_t display_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_cmd), MP_ROM_PTR(&display_cmd_obj_obj) },
    { MP_ROM_QSTR(MP_QSTR_cmd_data), MP_ROM_PTR(&display_cmd_data_obj_obj) },
    { MP_ROM_QSTR(MP_QSTR_show), MP_ROM_PTR(&display_show_obj_obj) },
    { MP_ROM_QSTR(MP_QSTR_update_rect), MP_ROM_PTR(&display_update_rect_obj_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_backlight), MP_ROM_PTR(&display_set_backlight_obj_obj) },
};
static MP_DEFINE_CONST_DICT(display_locals_dict, display_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    display_type,
    MP_QSTR_Display,
    MP_TYPE_FLAG_NONE,
    make_new, display_make_new,
    locals_dict, &display_locals_dict
);