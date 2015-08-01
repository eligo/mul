#ifndef __M_T_HEADER__
#define __M_T_HEADER__
#include <stdint.h>

int  mt_init();
void mt_add(uint32_t source, uint32_t ticks, uint32_t session);
void mt_release();
void mt_update();
#endif