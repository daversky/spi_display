// spi_displays/draw.c
#include "draw.h"
#include "fonts.h"
#include "py/runtime.h"
#include <stdlib.h>

#define PARSE_COORD(obj, x, y) { \
    size_t _len; mp_obj_t *_items; \
    mp_obj_get_array(obj, &_len, &_items); \
    x = mp_obj_get_int(_items[0]); \
    y = mp_obj_get_int(_items[1]); \
}

static int compare_int(const void *a, const void *b) {
    return (*(int *)a - *(int *)b);
}

static uint16_t convert_color(mp_obj_t color_obj) {
    if (mp_obj_is_type(color_obj, &mp_type_tuple)) {
        // Если это кортеж (r,g,b)
        mp_obj_t *items;
        size_t len;
        mp_obj_tuple_get(color_obj, &len, &items);
        if (len != 3) {
            mp_raise_ValueError(MP_ERROR_TEXT("color tuple must have 3 elements (r,g,b)"));
        }
        int r = mp_obj_get_int(items[0]);
        int g = mp_obj_get_int(items[1]);
        int b = mp_obj_get_int(items[2]);
        if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
            mp_raise_ValueError(MP_ERROR_TEXT("RGB values must be 0-255"));
        }
        uint16_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        return rgb565;
    }
    if (mp_obj_is_int(color_obj)) {
        // Если это число (уже RGB565)
        uint16_t raw = mp_obj_get_int(color_obj);
        return raw;
    }
    // Неподдерживаемый тип
    mp_raise_TypeError(MP_ERROR_TEXT("color must be int or tuple (r,g,b)"));
}

// text
static uint32_t decode_utf8(const char **ptr) {
    const uint8_t *s = (const uint8_t *)*ptr;
    uint32_t res = *s++;
    if (res >= 0x80) {
        if (res < 0xe0) {
            res = ((res & 0x1f) << 6) | (*s++ & 0x3f);
        } else if (res < 0xf0) {
            res = ((res & 0x0f) << 12) | ((s[0] & 0x3f) << 6) | (s[1] & 0x3f);
            s += 2;
        }
    }
    *ptr = (const char *)s;
    return res;
}

static void draw_char(const mp_display_obj_t *display, const int16_t x, int16_t y, uint16_t c, uint16_t color, const GFXfont *font) {
    if (c < font->first || c > font->last) return;
    uint16_t glyph_index = c - font->first;
    GFXglyph *glyph = &(font->glyph[glyph_index]);
    uint8_t *bitmap = font->bitmap;
    uint16_t bo = glyph->bitmapOffset;
    uint8_t w = glyph->width;
    uint8_t h = glyph->height;
    int8_t xo = glyph->xOffset;
    int8_t yo = glyph->yOffset;
    uint8_t bits = 0, bit = 0;
    uint16_t *buffer = display->buffer;
    for (uint8_t yy = 0; yy < h; yy++) {
        for (uint8_t xx = 0; xx < w; xx++) {
            if (!(bit++ & 7)) {
                bits = bitmap[bo++];
            }
            if (bits & 0x80) {
                int16_t cur_x = x + xo + xx;
                int16_t cur_y = y + yo + yy;
                if (cur_x >= 0 && cur_x < display->width && cur_y >= 0 && cur_y < display->height) {
                    buffer[cur_y * display->width + cur_x] = color;
                }
            }
            bits <<= 1;
        }
    }
}
void draw_text(const mp_display_obj_t *display, const char *str, int16_t x, int16_t y, uint16_t color, int font_size) {
    const GFXfont *f_latin = &Font_L_6;
    const GFXfont *f_cyrillic = &Font_C_6;
    switch (font_size) {
        case 8:  f_latin = &Font_L_8;  f_cyrillic = &Font_C_8;  break;
        case 12: f_latin = &Font_L_12; f_cyrillic = &Font_C_12; break;
        case 16: f_latin = &Font_L_16; f_cyrillic = &Font_C_16; break;
        case 20: f_latin = &Font_L_20; f_cyrillic = &Font_C_20; break;
        case 24: f_latin = &Font_L_24; f_cyrillic = &Font_C_24; break;
        default: f_latin = &Font_L_6;  f_cyrillic = &Font_C_6;  break;
    }
    int16_t top_offset = 0;
    if (f_latin->glyph) {
        top_offset = (int16_t)(-(f_latin->glyph[65 - f_latin->first].yOffset));
    }
    int16_t baseline_y = y + top_offset;
    while (*str) {
        const uint32_t code = decode_utf8(&str);
        const GFXfont *current_font = NULL;
        if (code >= 32 && code <= 127) {
            current_font = f_latin;
        } else if (code >= 1025 && code <= 1105) {
            current_font = f_cyrillic;
        }
        if (current_font) {
            draw_char(display, x, baseline_y, (uint16_t)code, color, current_font);
            x += current_font->glyph[code - current_font->first].xAdvance;
        } else {
            x += (font_size / 2 + 2);
        }
        if (x >= display->width) break;
    }
}

// plot ...
// Пиксель
static void plot_pixel(const mp_display_obj_t *display, int x, int y, uint16_t color) {
    if (x >= 0 && x < display->width && y >= 0 && y < display->height) {
        uint16_t *buf = display->buffer;
        buf[y * display->width + x] = color;
    }
}
// horizont line
static void plot_hline(const mp_display_obj_t *self, int x1, int x2, const int y, const uint16_t color) {
    if (y < 0 || y >= self->height) return;
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (x1 < 0) x1 = 0;
    if (x2 >= (int)self->width) x2 = self->width - 1;

    if (x1 <= x2) {
        uint16_t *p = &((uint16_t *)self->buffer)[y * self->width + x1];
        int n = x2 - x1 + 1;
        while (n--) *p++ = color;
    }
}
// ....
// Any Line
static void plot_line(const mp_display_obj_t *self, int x1, int y1, int x2, int y2, uint16_t color) {
    if (y1 == y2) { plot_hline(self, x1, x2, y1, color); return; }
    int dx = abs(x2 - x1), dy = -abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        plot_pixel(self, x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}
// wrappers
// draw.text(text="", (x,y), (r,g,b), 10)
mp_obj_t draw_text_wrapper(size_t n_args, const mp_obj_t *args) {
    draw_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    const char *text = mp_obj_str_get_str(args[1]);
    int16_t x = mp_obj_get_int(args[2]);
    int16_t y = mp_obj_get_int(args[3]);
    // Обработка цвета (4-й аргумент)
    uint16_t color = 0xFFFF;
    if (n_args > 4) {
        if (mp_obj_is_type(args[4], &mp_type_tuple)) {
            // Цвет как кортеж (r,g,b)
            mp_obj_t *items;
            size_t len;
            mp_obj_tuple_get(args[4], &len, &items);
            if (len != 3) {
                mp_raise_ValueError(MP_ERROR_TEXT("color tuple must have 3 elements (r,g,b)"));
            }
            int r = mp_obj_get_int(items[0]);
            int g = mp_obj_get_int(items[1]);
            int b = mp_obj_get_int(items[2]);
            color = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        } else {
            color = (uint16_t)mp_obj_get_int(args[4]);
        }
    }
    int font_size = (n_args > 5) ? mp_obj_get_int(args[5]) : 0;
    draw_text(self->display, text, x, y, color, font_size);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(draw_text_obj, 4, 6, draw_text_wrapper);

// draw.line(color=(r,g,b), start=(x,y), end=(x,y))
mp_obj_t draw_line_wrapper(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // vars
    enum { ARG_color, ARG_start, ARG_end };
    static const mp_arg_t allowed_args[] = {
            { MP_QSTR_color, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
            { MP_QSTR_start, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
            { MP_QSTR_end,   MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    draw_obj_t *draw_ptr = MP_OBJ_TO_PTR(pos_args[0]);
    mp_display_obj_t *self = draw_ptr->display;
    int x1, y1, x2, y2;
    const uint16_t color = convert_color(args[ARG_color].u_obj);
    PARSE_COORD(args[ARG_start].u_obj, x1, y1);
    PARSE_COORD(args[ARG_end].u_obj, x2, y2);
    // draw
    plot_line(self, x1, y1, x2, y2, color);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(draw_line_obj, 1, draw_line_wrapper);

// draw.rect(color=(r,g,b), start=(x,y), width=0, height=0, fill=False)
mp_obj_t draw_rect_wrapper(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_color, ARG_start, ARG_width, ARG_height, ARG_fill };
    static const mp_arg_t allowed_args[] = {
            { MP_QSTR_color,  MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
            { MP_QSTR_start,  MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
            { MP_QSTR_width,  MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
            { MP_QSTR_height, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
            { MP_QSTR_fill,   MP_ARG_BOOL, {.u_bool = false} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    draw_obj_t *draw_ptr = MP_OBJ_TO_PTR(pos_args[0]);
    mp_display_obj_t *self = draw_ptr->display;
    int x, y;
    PARSE_COORD(args[ARG_start].u_obj, x, y);
    const uint16_t color = convert_color(args[ARG_color].u_obj);
    int w = args[ARG_width].u_int;
    int h = args[ARG_height].u_int;
    if (args[ARG_fill].u_bool) {
        for (int i = 0; i < h; i++) plot_hline(self, x, x + w - 1, y + i, color);
    } else {
        int x2 = x + w - 1, y2 = y + h - 1;
        plot_line(self, x, y, x2, y, color);
        plot_line(self, x2, y, x2, y2, color);
        plot_line(self, x2, y2, x, y2, color);
        plot_line(self, x, y2, x, y, color);
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(draw_rect_obj, 1, draw_rect_wrapper);

// draw.ellipse(color=0, center=(x,y), width=0, height=0, fill=False)
mp_obj_t draw_ellipse_wrapper(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_color, ARG_center, ARG_width, ARG_height, ARG_fill };
    static const mp_arg_t allowed_args[] = {
            { MP_QSTR_color,  MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
            { MP_QSTR_center, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
            { MP_QSTR_width,  MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} }, // rx
            { MP_QSTR_height, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} }, // ry
            { MP_QSTR_fill,   MP_ARG_BOOL, {.u_bool = false} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    draw_obj_t *draw_ptr = MP_OBJ_TO_PTR(pos_args[0]);
    mp_display_obj_t *self = draw_ptr->display;

    int cx, cy;
    PARSE_COORD(args[ARG_center].u_obj, cx, cy);

    uint16_t color = convert_color(args[ARG_color].u_obj);
    int rx = args[ARG_width].u_int;
    int ry = args[ARG_height].u_int;

    long x = 0, y = ry;
    long rx2 = (long)rx * rx, ry2 = (long)ry * ry;
    long err = ry2 - rx2 * ry;

    while (y >= 0) {
        if (args[ARG_fill].u_bool) {
            plot_hline(self, cx - x, cx + x, cy + y, color);
            if (y > 0) plot_hline(self, cx - x, cx + x, cy - y, color);
        } else {
            plot_pixel(self, cx + x, cy + y, color); plot_pixel(self, cx - x, cy + y, color);
            plot_pixel(self, cx + x, cy - y, color); plot_pixel(self, cx - x, cy - y, color);
        }
        if (err < 0) err += 2 * ry2 * (++x) + ry2;
        else err += 2 * ry2 * (++x) - 2 * rx2 * (--y) + ry2;
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(draw_ellipse_obj, 1, draw_ellipse_wrapper);

// draw.circle(color=0, center=(x,y), radius=0, fill=False)
mp_obj_t draw_circle_wrapper(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_color, ARG_center, ARG_radius, ARG_fill };
    static const mp_arg_t allowed_args[] = {
            { MP_QSTR_color,  MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
            { MP_QSTR_center, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
            { MP_QSTR_radius, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
            { MP_QSTR_fill,   MP_ARG_BOOL, {.u_bool = false} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    draw_obj_t *draw_ptr = MP_OBJ_TO_PTR(pos_args[0]);
    mp_display_obj_t *self = draw_ptr->display;

    int cx, cy, r = args[ARG_radius].u_int;
    PARSE_COORD(args[ARG_center].u_obj, cx, cy);
    uint16_t color = convert_color(args[ARG_color].u_obj);

    int x = 0, y = r, d = 3 - 2 * r;
    while (y >= x) {
        if (args[ARG_fill].u_bool) {
            plot_hline(self, cx - x, cx + x, cy + y, color);
            plot_hline(self, cx - x, cx + x, cy - y, color);
            plot_hline(self, cx - y, cx + y, cy + x, color);
            plot_hline(self, cx - y, cx + y, cy - x, color);
        } else {
            plot_pixel(self, cx + x, cy + y, color); plot_pixel(self, cx - x, cy + y, color);
            plot_pixel(self, cx + x, cy - y, color); plot_pixel(self, cx - x, cy - y, color);
            plot_pixel(self, cx + y, cy + x, color); plot_pixel(self, cx - y, cy + x, color);
            plot_pixel(self, cx + y, cy - x, color); plot_pixel(self, cx - y, cy - x, color);
        }
        if (d < 0) d += 4 * x + 6;
        else { d += 4 * (x - y) + 10; y--; }
        x++;
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(draw_circle_obj, 1, draw_circle_wrapper);

// draw.lines(color=0, points=[], closed=False)
mp_obj_t draw_lines_wrapper(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_color, ARG_points, ARG_closed };
    static const mp_arg_t allowed_args[] = {
            { MP_QSTR_color,  MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
            { MP_QSTR_points, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
            { MP_QSTR_closed, MP_ARG_BOOL, {.u_bool = false} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    draw_obj_t *draw_ptr = MP_OBJ_TO_PTR(pos_args[0]);
    mp_display_obj_t *self = draw_ptr->display;
    size_t num_points;
    mp_obj_t *items;
    mp_obj_get_array(args[ARG_points].u_obj, &num_points, &items);
    if (num_points < 2) {
        return mp_const_none;
    }
    uint16_t color = convert_color(args[ARG_color].u_obj);
    int x_start, y_start, x_curr, y_curr;
    PARSE_COORD(items[0], x_start, y_start);
    int x_prev = x_start;
    int y_prev = y_start;
    for (size_t i = 1; i < num_points; i++) {
        PARSE_COORD(items[i], x_curr, y_curr);
        plot_line(self, x_prev, y_prev, x_curr, y_curr, color);
        x_prev = x_curr;
        y_prev = y_curr;
    }
    if (args[ARG_closed].u_bool) {
        plot_line(self, x_prev, y_prev, x_start, y_start, color);
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(draw_lines_obj, 1, draw_lines_wrapper);

// draw.polygon(color=(r,g,b), points=[], fill=False)
mp_obj_t draw_polygon_wrapper(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_color, ARG_points, ARG_fill };
    static const mp_arg_t allowed_args[] = {
            { MP_QSTR_color,  MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
            { MP_QSTR_points, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
            { MP_QSTR_fill,   MP_ARG_BOOL, {.u_bool = false} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    draw_obj_t *draw_ptr = MP_OBJ_TO_PTR(pos_args[0]);
    mp_display_obj_t *self = draw_ptr->display;
    size_t n;
    mp_obj_t *items;
    mp_obj_get_array(args[ARG_points].u_obj, &n, &items);
    if (n < 3) {
        return mp_const_none; // Многоугольник минимум из 3 точек
    }
    const uint16_t color = convert_color(args[ARG_color].u_obj);
    if (args[ARG_fill].u_bool) {
        int min_y = self->height, max_y = 0;
        int *nodes_x = m_new(int, n);
        for (size_t i = 0; i < n; i++) {
            int16_t x __attribute__((unused)), y;
            PARSE_COORD(items[i], x, y);
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
        }
        if (min_y < 0) min_y = 0;
        if (max_y >= self->height) max_y = self->height - 1;
        for (int py = min_y; py <= max_y; py++) {
            int cnt = 0;
            for (size_t i = 0; i < n; i++) {
                int16_t x1, y1, x2, y2;
                PARSE_COORD(items[i], x1, y1);
                PARSE_COORD(items[(i + 1) % n], x2, y2);
                if ((y1 < py && y2 >= py) || (y2 < py && y1 >= py)) {
                    nodes_x[cnt++] = x1 + (py - y1) * (x2 - x1) / (y2 - y1);
                }
            }
            qsort(nodes_x, cnt, sizeof(int), compare_int);
            for (int i = 0; i < cnt; i += 2) {
                plot_hline(self, nodes_x[i], nodes_x[i+1], py, color);
            }
        }
        m_del(int, nodes_x, n);
    } else {
        int x_s, y_s, x_c, y_c;
        PARSE_COORD(items[0], x_s, y_s);
        int x_p = x_s; int y_p = y_s;
        for (size_t i = 1; i < n; i++) {
            PARSE_COORD(items[i], x_c, y_c);
            plot_line(self, x_p, y_p, x_c, y_c, color);
            x_p = x_c; y_p = y_c;
        }
        plot_line(self, x_p, y_p, x_s, y_s, color);
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(draw_polygon_obj, 1, draw_polygon_wrapper);

//  ------------------------- registration -----------------------------
static const mp_rom_map_elem_t draw_locals_dict_table[] = {
//        { MP_ROM_QSTR(MP_QSTR_fill), MP_ROM_PTR(&draw_fill_obj) },
    { MP_ROM_QSTR(MP_QSTR_text), MP_ROM_PTR(&draw_text_obj) },
    { MP_ROM_QSTR(MP_QSTR_line), MP_ROM_PTR(&draw_line_obj) },
    { MP_ROM_QSTR(MP_QSTR_lines), MP_ROM_PTR(&draw_lines_obj) },
    { MP_ROM_QSTR(MP_QSTR_rect), MP_ROM_PTR(&draw_rect_obj) },
    { MP_ROM_QSTR(MP_QSTR_circle), MP_ROM_PTR(&draw_circle_obj) },
    { MP_ROM_QSTR(MP_QSTR_ellipse), MP_ROM_PTR(&draw_ellipse_obj) },
    { MP_ROM_QSTR(MP_QSTR_polygon), MP_ROM_PTR(&draw_polygon_obj) },
};
static MP_DEFINE_CONST_DICT(draw_locals_dict, draw_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
        mp_type_draw,
        MP_QSTR_Draw,
        MP_TYPE_FLAG_NONE,
        locals_dict, &draw_locals_dict
);

mp_obj_t draw_make_new(mp_display_obj_t *display) {
    draw_obj_t *self = mp_obj_malloc(draw_obj_t, &mp_type_draw);
    self->display = display;
    return MP_OBJ_FROM_PTR(self);
}