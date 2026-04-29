// spi_displays/st7796s.c
#include "display.h"
#include "py/mphal.h"


static const display_init_cmd_t st7796s_init_sequence[] = {
        {0x01, 0, 120, {}},          // Soft Reset
        {0x11, 0, 120, {}},          // Sleep Out
        {0xF0, 1, 0, {0xC3}},        // Command Set Control (Enable extension commands)
        {0xF0, 1, 0, {0x96}},        // Command Set Control
        {0x36, 1, 0, {0x48}},        // Memory Data Access Control (MX, BGR)
        {0x3A, 1, 0, {0x05}},        // Interface Pixel Format (16-bit)
        {0xB4, 1, 0, {0x01}},        // Display Inversion Control (1-dot inversion)
        {0xB7, 1, 0, {0xC6}},        // Entry Mode Set
        {0xB6, 2, 0, {0x80, 0x02}},  // Display Function Control
        {0xC5, 1, 0, {0x24}},        // VCOM Control
        {0xE4, 1, 0, {0x31}},        // Power Control
        {0xE8, 8, 0, {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33}}, // Display Timing Control
        // --- Gamma Correction ---
        {0xE0, 14, 0, {0xF0, 0x09, 0x13, 0x12, 0x12, 0x2B, 0x3C, 0x44, 0x4B, 0x1B, 0x18, 0x17, 0x1D, 0x21}}, // Positive Gamma
        {0xE1, 14, 0, {0xF0, 0x09, 0x13, 0x0C, 0x0D, 0x27, 0x3B, 0x44, 0x4D, 0x0B, 0x17, 0x17, 0x1D, 0x21}}, // Negative Gamma
        // ------------------------
        {0xF0, 1, 0, {0x3C}},        // Command Set Control (Disable extension commands)
        {0xF0, 1, 0, {0x69}},        // Command Set Control
        {0x21, 0, 0, {}},            // Display Inversion ON (часто нужен для IPS)
        {0x29, 0, 100, {}},          // Display ON
        {0x00, 0, 0, {}}             // End of list
};


static mp_obj_t st7796s_make_new(const mp_obj_type_t* type, size_t n_args, size_t n_kw, const mp_obj_t* all_args) {
    mp_obj_t display_obj = display_make_new_base(&mp_type_display_core, n_args, n_kw, all_args);
    mp_display_obj_t* self = MP_OBJ_TO_PTR(display_obj);
    display_init(self, st7796s_init_sequence);
    self->base.type = type;
    return display_obj;
}

static const mp_rom_map_elem_t st7796s_locals_dict_table[] = {};
static MP_DEFINE_CONST_DICT(st7796s_locals_dict, st7796s_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_st7796s,
    MP_QSTR_st7796s,
    MP_TYPE_FLAG_NONE,
    make_new, st7796s_make_new,
    locals_dict, &st7796s_locals_dict,
    attr, display_attr,
    parent, &mp_type_display
);