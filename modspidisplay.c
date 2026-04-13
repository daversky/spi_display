#include "py/runtime.h"
#include "py/obj.h"
#include "py/mphal.h"
#include "py/objarray.h"
#include "display.h"
#include "st7735_init.h"
#include "st7789_init.h"
#include <string.h>

// Вспомогательные функции для framebuf
static mp_obj_t get_framebuf_class(void) {
    mp_obj_t mod = mp_import_name(MP_QSTR_framebuf, mp_const_none, 0);
    return mp_load_attr(mod, MP_QSTR_FrameBuffer);
}

static mp_obj_t get_rgb565_format(void) {
    mp_obj_t mod = mp_import_name(MP_QSTR_framebuf, mp_const_none, 0);
    return mp_load_attr(mod, MP_QSTR_RGB565);
}

// ========== ST7735 ==========
typedef struct {
    mp_obj_base_t base;
    display_obj_t *display;
    mp_obj_t framebuffer_obj;
    mp_obj_t framebuf_obj;
    uint16_t width;
    uint16_t height;
} st7735_obj_t;

static mp_obj_t st7735_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum { ARG_spi, ARG_width, ARG_height, ARG_buffer, ARG_dc, ARG_cs, ARG_rst, ARG_bl, ARG_backlight_active_high };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_spi, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_width, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_height, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_buffer, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_dc, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_cs, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_rst, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_bl, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_backlight_active_high, MP_ARG_BOOL, {.u_bool = true} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    st7735_obj_t *self = mp_obj_malloc(st7735_obj_t, type);
    self->width = args[ARG_width].u_int;
    self->height = args[ARG_height].u_int;

    size_t buffer_size = self->width * self->height * 2;
    if (args[ARG_buffer].u_obj != mp_const_none) {
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(args[ARG_buffer].u_obj, &bufinfo, MP_BUFFER_RW);
        if (bufinfo.len < buffer_size) mp_raise_ValueError(MP_ERROR_TEXT("buffer too small"));
        self->framebuffer_obj = args[ARG_buffer].u_obj;
    } else {
        self->framebuffer_obj = mp_obj_new_bytearray(buffer_size, NULL);
    }

    // Создаём Display через вызов типа (аналог `display_type.make_new`)
    mp_obj_t display_args[8] = {
        args[ARG_spi].u_obj,
        mp_obj_new_int(self->width),
        mp_obj_new_int(self->height),
        args[ARG_dc].u_obj,
        args[ARG_cs].u_obj,
        args[ARG_rst].u_obj,
        args[ARG_bl].u_obj,
        mp_obj_new_bool(args[ARG_backlight_active_high].u_bool),
    };

    // Правильный способ создать экземпляр Display
    mp_obj_t display_obj = mp_call_function_2(MP_OBJ_FROM_PTR(&display_type),
                                               MP_OBJ_NEW_SMALL_INT(8),
                                               (mp_obj_t)display_args);
    self->display = MP_OBJ_TO_PTR(display_obj);

    // Инициализация ST7735
    for (size_t i = 0; i < ST7735_INIT_SEQUENCE_LEN; i++) {
        const st7735_init_cmd_t *cmd = &st7735_init_sequence[i];
        display_cmd_data(self->display, cmd->cmd, cmd->data, cmd->data_len);
        if (cmd->delay_ms > 0) mp_hal_delay_ms(cmd->delay_ms);
    }

    // Создаём FrameBuffer
    mp_obj_t fb_args[4] = {
        self->framebuffer_obj,
        mp_obj_new_int(self->width),
        mp_obj_new_int(self->height),
        get_rgb565_format()
    };
    self->framebuf_obj = mp_call_function_n_kw(get_framebuf_class(), 4, 0, fb_args);

    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t st7735_show(mp_obj_t self_in) {
    st7735_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(self->framebuffer_obj, &bufinfo, MP_BUFFER_READ);
    display_show(self->display, bufinfo.buf, bufinfo.len);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(st7735_show_obj, st7735_show);

static mp_obj_t st7735_update_rect(size_t n_args, const mp_obj_t *args) {
    st7735_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    uint16_t x = mp_obj_get_int(args[1]), y = mp_obj_get_int(args[2]);
    uint16_t w = mp_obj_get_int(args[3]), h = mp_obj_get_int(args[4]);

    size_t rect_size = w * h * 2;
    uint8_t *temp = m_new(uint8_t, rect_size);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(self->framebuffer_obj, &bufinfo, MP_BUFFER_READ);

    uint8_t *src = (uint8_t *)bufinfo.buf + (y * self->width + x) * 2;
    uint8_t *dst = temp;
    for (uint16_t row = 0; row < h; row++) {
        memcpy(dst, src, w * 2);
        src += self->width * 2;
        dst += w * 2;
    }

    display_update_rect(self->display, x, y, w, h, temp, rect_size);
    m_del(uint8_t, temp, rect_size);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(st7735_update_rect_obj, 5, 5, st7735_update_rect);

static mp_obj_t st7735_set_backlight(mp_obj_t self_in, mp_obj_t percent_in) {
    st7735_obj_t *self = MP_OBJ_TO_PTR(self_in);
    display_set_backlight(self->display, mp_obj_get_int(percent_in));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(st7735_set_backlight_obj, st7735_set_backlight);

static void st7735_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    st7735_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (dest[0] == MP_OBJ_NULL) {
        if (attr == MP_QSTR_display) dest[0] = self->framebuf_obj;
        else if (attr == MP_QSTR_buffer) dest[0] = self->framebuffer_obj;
    }
}

static const mp_rom_map_elem_t st7735_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_show), MP_ROM_PTR(&st7735_show_obj) },
    { MP_ROM_QSTR(MP_QSTR_update_rect), MP_ROM_PTR(&st7735_update_rect_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_backlight), MP_ROM_PTR(&st7735_set_backlight_obj) },
};
static MP_DEFINE_CONST_DICT(st7735_locals_dict, st7735_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    st7735_type,
    MP_QSTR_ST7735,
    MP_TYPE_FLAG_NONE,
    make_new, st7735_make_new,
    attr, st7735_attr,
    locals_dict, &st7735_locals_dict
);

// ========== ST7789 ==========
typedef struct {
    mp_obj_base_t base;
    display_obj_t *display;
    mp_obj_t framebuffer_obj;
    mp_obj_t framebuf_obj;
    uint16_t width;
    uint16_t height;
} st7789_obj_t;

static mp_obj_t st7789_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum { ARG_spi, ARG_width, ARG_height, ARG_buffer, ARG_dc, ARG_cs, ARG_rst, ARG_bl, ARG_backlight_active_high };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_spi, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_width, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_height, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_buffer, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_dc, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_cs, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_rst, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_bl, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_backlight_active_high, MP_ARG_BOOL, {.u_bool = true} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    st7789_obj_t *self = mp_obj_malloc(st7789_obj_t, type);
    self->width = args[ARG_width].u_int;
    self->height = args[ARG_height].u_int;

    size_t buffer_size = self->width * self->height * 2;
    if (args[ARG_buffer].u_obj != mp_const_none) {
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(args[ARG_buffer].u_obj, &bufinfo, MP_BUFFER_RW);
        if (bufinfo.len < buffer_size) mp_raise_ValueError(MP_ERROR_TEXT("buffer too small"));
        self->framebuffer_obj = args[ARG_buffer].u_obj;
    } else {
        self->framebuffer_obj = mp_obj_new_bytearray(buffer_size, NULL);
    }

    mp_obj_t display_args[8] = {
        args[ARG_spi].u_obj,
        mp_obj_new_int(self->width),
        mp_obj_new_int(self->height),
        args[ARG_dc].u_obj,
        args[ARG_cs].u_obj,
        args[ARG_rst].u_obj,
        args[ARG_bl].u_obj,
        mp_obj_new_bool(args[ARG_backlight_active_high].u_bool),
    };

    mp_obj_t display_obj = mp_call_function_2(MP_OBJ_FROM_PTR(&display_type),
                                               MP_OBJ_NEW_SMALL_INT(8),
                                               (mp_obj_t)display_args);
    self->display = MP_OBJ_TO_PTR(display_obj);

    for (size_t i = 0; i < ST7789_INIT_SEQUENCE_LEN; i++) {
        const st7789_init_cmd_t *cmd = &st7789_init_sequence[i];
        display_cmd_data(self->display, cmd->cmd, cmd->data, cmd->data_len);
        if (cmd->delay_ms > 0) mp_hal_delay_ms(cmd->delay_ms);
    }

    mp_obj_t fb_args[4] = {
        self->framebuffer_obj,
        mp_obj_new_int(self->width),
        mp_obj_new_int(self->height),
        get_rgb565_format()
    };
    self->framebuf_obj = mp_call_function_n_kw(get_framebuf_class(), 4, 0, fb_args);

    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t st7789_show(mp_obj_t self_in) {
    st7789_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(self->framebuffer_obj, &bufinfo, MP_BUFFER_READ);
    display_show(self->display, bufinfo.buf, bufinfo.len);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(st7789_show_obj, st7789_show);

static mp_obj_t st7789_update_rect(size_t n_args, const mp_obj_t *args) {
    st7789_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    uint16_t x = mp_obj_get_int(args[1]), y = mp_obj_get_int(args[2]);
    uint16_t w = mp_obj_get_int(args[3]), h = mp_obj_get_int(args[4]);

    size_t rect_size = w * h * 2;
    uint8_t *temp = m_new(uint8_t, rect_size);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(self->framebuffer_obj, &bufinfo, MP_BUFFER_READ);

    uint8_t *src = (uint8_t *)bufinfo.buf + (y * self->width + x) * 2;
    uint8_t *dst = temp;
    for (uint16_t row = 0; row < h; row++) {
        memcpy(dst, src, w * 2);
        src += self->width * 2;
        dst += w * 2;
    }

    display_update_rect(self->display, x, y, w, h, temp, rect_size);
    m_del(uint8_t, temp, rect_size);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(st7789_update_rect_obj, 5, 5, st7789_update_rect);

static mp_obj_t st7789_set_backlight(mp_obj_t self_in, mp_obj_t percent_in) {
    st7789_obj_t *self = MP_OBJ_TO_PTR(self_in);
    display_set_backlight(self->display, mp_obj_get_int(percent_in));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(st7789_set_backlight_obj, st7789_set_backlight);

static void st7789_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    st7789_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (dest[0] == MP_OBJ_NULL) {
        if (attr == MP_QSTR_display) dest[0] = self->framebuf_obj;
        else if (attr == MP_QSTR_buffer) dest[0] = self->framebuffer_obj;
    }
}

static const mp_rom_map_elem_t st7789_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_show), MP_ROM_PTR(&st7789_show_obj) },
    { MP_ROM_QSTR(MP_QSTR_update_rect), MP_ROM_PTR(&st7789_update_rect_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_backlight), MP_ROM_PTR(&st7789_set_backlight_obj) },
};
static MP_DEFINE_CONST_DICT(st7789_locals_dict, st7789_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    st7789_type,
    MP_QSTR_ST7789,
    MP_TYPE_FLAG_NONE,
    make_new, st7789_make_new,
    attr, st7789_attr,
    locals_dict, &st7789_locals_dict
);

// ========== Регистрация модуля ==========
static const mp_rom_map_elem_t spi_displays_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_spi_displays) },
    { MP_ROM_QSTR(MP_QSTR_Display), MP_ROM_PTR(&display_type) },
    { MP_ROM_QSTR(MP_QSTR_ST7735), MP_ROM_PTR(&st7735_type) },
    { MP_ROM_QSTR(MP_QSTR_ST7789), MP_ROM_PTR(&st7789_type) },
};
static MP_DEFINE_CONST_DICT(spi_displays_globals, spi_displays_globals_table);

const mp_obj_module_t spi_displays_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&spi_displays_globals,
};

MP_REGISTER_MODULE(MP_QSTR_spi_displays, spi_displays_module);