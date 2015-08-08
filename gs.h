#ifndef __GLOBAL_SOCKET_MGR__
#define __GLOBAL_SOCKET_MGR__
int gs_init();
void gs_release();
void gs_update();
int so_listen(uint32_t env, const char *ip, int port);
int so_accept(uint32_t env);
int so_add(uint32_t env, int sid);
int so_read(int sid);
int so_close(int sid);
#endif