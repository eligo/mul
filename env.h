#ifndef __env_HEADER__
#define __env_HEADER__
#include <stdint.h>
struct env_t;
struct msg_t;
/*
	enviroment of actor
*/
int  env_init();
void env_release();
int  env_create(const char *script, uint32_t *id);
int  env_post(uint32_t tocell, struct msg_t *msg);
int  env_process_msg(struct env_t* cell, struct msg_t *msg);
#endif