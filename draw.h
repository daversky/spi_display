// spi_displays/draw.h
#ifndef MICROPY_INCLUDED_DRAW_H
#define MICROPY_INCLUDED_DRAW_H
#include "py/obj.h"
#include "display.h"

typedef struct _draw_obj_t
{
    mp_obj_base_t base;
    mp_display_obj_t* display;
} draw_obj_t;

mp_obj_t draw_make_new(mp_display_obj_t* display);

extern const mp_obj_type_t mp_type_draw;

#endif
