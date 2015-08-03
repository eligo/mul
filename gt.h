#ifndef __M_T_HEADER__
#define __M_T_HEADER__
#include <stdint.h>
/*
	global timer
*/
int  gt_init();
void gt_release();
void gt_update();
void gt_add(uint32_t source, uint32_t ticks, uint32_t session);
#endif