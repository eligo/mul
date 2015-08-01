#ifndef __CELL_HEADER__
#define __CELL_HEADER__
#include <stdint.h>
struct cell_t;
struct msg_t;

int cell_init();
void cell_release();
int cell_create(const char *script, uint32_t *id);
int cell_post(uint32_t tocell, struct msg_t *msg);
int cell_process_msg(struct cell_t* cell, struct msg_t *msg);
#endif