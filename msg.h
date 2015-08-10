#ifndef __MSG_HEADER__
#define __MSG_HEADER__
//#include "common/global.h"

#define MTYPE_TIMER 1
#define MTYPE_POST 2
#define MTYPE_SOCKET_READABLE 3
#define MTYPE_SOCKET_ERROR 4
#define MTYPE_SOCKET_ACCEPTABLE 5
struct msg_t {
	uint8_t type;
	uint32_t from;
	uint32_t session;
	uint32_t len;
	struct msg_t *next;
};
#endif