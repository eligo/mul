#ifndef __GLOBAL_SOCKET_MGR__
#define __GLOBAL_SOCKET_MGR__
#include <stdint.h>
#include <stdio.h>

int gs_init();
void gs_release();
void gs_update();
int so_listen(uint32_t env, const char *ip, int port);
int so_accept(uint32_t env, int sid);
int so_add(uint32_t env, int sid);
int so_read(int sid, char *buf, int cap);
int so_write(int sid, const char *buf, size_t len);
int so_close(uint32_t env, int sid);
#endif