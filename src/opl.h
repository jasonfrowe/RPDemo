#ifndef OPL_H
#define OPL_H

#include <stdint.h>

void opl_init(void);
void opl_write(uint8_t reg, uint8_t value);

#endif
