#ifndef __MSG_HEADER__
#define __MSG_HEADER__
#include "common/global.h"

#define MT_TIMER 1
#define MT_CALL 2
#define MT_RET 3
struct msg_t {
	uint8_t type;
	uint32_t from;
	uint32_t session;
	uint32_t len;
	struct msg_t *next;
};
#endif