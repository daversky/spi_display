#include "display.h"
#include "py/mphal.h"

static const uint8_t init_seq[] = {
    0x01, 0, 150,
    0x11, 0, 0,
    0x00, 0, 120,
    0xB1, 3, 0x01, 0x2C, 0x2D,
    0xB2, 3, 0x01, 0x2C, 0x2D,
    0xB3, 6, 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D,
    0xB4, 1, 0x07,
    0xC0, 3, 0xA2, 0x02, 0x84,
    0xC1, 1, 0xC5,
    0xC2, 2, 0x0A, 0x00,
    0xC3, 2, 0x8A, 0x2A,
    0xC4, 2, 0x8A, 0xEE,
    0xC5, 1, 0x0E,
    0x20, 0, 0,
    0xE0, 16, 0x0F, 0x1A, 0x0F, 0x18, 0x2F, 0x28, 0x20, 0x22, 0x1F, 0x1B, 0x23, 0x37, 0x00, 0x07, 0x02, 0x10,
    0xE1, 16, 0x0F, 0x1B, 0x0F, 0x17, 0x33, 0x2C, 0x29, 0x2E, 0x30, 0x30, 0x39, 0x3F, 0x00, 0x07, 0x03, 0x10,
    0x3A, 1, 0x55,
    0x29, 0, 0,
    0x00, 0, 10,
};

static void st7735_init(mp_display_obj_t *self) {
    if (self->rst != MP_OBJ_NULL) display_reset_hw(self);
    const uint8_t *seq = init_seq;
    size_t seq_len = sizeof(init_seq);
    for (size_t i = 0; i < seq_len; ) {
        uint8_t cmd = seq[i++];
        uint8_t len = seq[i++];
        if (len == 0) {
            uint16_t delay_ms = seq[i] | (seq[i+1] << 8);
            i += 2;
            if (delay_ms > 0) mp_hal_delay_ms(delay_ms);
            if (cmd == 0x00) continue;
            display_send_cmd(self, cmd);
        } else {
            display_send_cmd_data(self, cmd, &seq[i], len);
            i += len;
            if (i < seq_len && seq[i] == 0x00) {
                i++;
                uint16_t delay_ms = seq[i] | (seq[i+1] << 8);
                i += 2;
                if (delay_ms > 0) mp_hal_delay_ms(delay_ms);
            }
        }
    }
}

static void st7735_set_rotation(mp_display_obj_t *self, uint8_t rot) {
    self->rotation = rot;
    uint8_t madctl = self->bgr ? 0x08 : 0x00;
    switch (rot) {
        case 0: madctl |= 0x00; break;
        case 1: madctl |= 0x60; break;
        case 2: madctl |= 0xC0; break;
        case 3: madctl |= 0xA0; break;
    }
    display_send_cmd_data(self, 0x36, &madctl, 1);
}

static mp_obj_t st7735_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    mp_obj_t display_obj = display_make_new_base(type, n_args, n_kw, all_args);
    mp_display_obj_t *self = MP_OBJ_TO_PTR(display_obj);
    self->set_rotation_func = st7735_set_rotation;
    st7735_init(self);
    st7735_set_rotation(self, self->rotation);
    return display_obj;
}

static const mp_rom_map_elem_t st7735_locals_dict_table[] = { };
static MP_DEFINE_CONST_DICT(st7735_locals_dict, st7735_locals_dict_table);

const mp_obj_type_t mp_type_st7735 = {
    .base = { &mp_type_type },
    .name = MP_QSTR_ST7735,
    .flags = MP_TYPE_FLAG_NONE,
    .make_new = st7735_make_new,
    .locals_dict = (mp_obj_dict_t*)&st7735_locals_dict,
    .parent = &mp_type_display,
};