#include "opl.h"

#include <rp6502.h>

#include "xram.h"

void opl_write(uint8_t reg, uint8_t value) {
    xram1_poke8(XRAM_OPL + reg, value);
}

void opl_init(void) {
    xram1_set(XRAM_OPL + 0x01, 0x00, 0xF5);
    opl_write(0x01, 0x20);
}
