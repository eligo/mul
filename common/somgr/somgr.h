//连接管理
#ifndef __SOMGR_HEADER__
#define __SOMGR_HEADER__
#include <stdint.h>

typedef int (*soacb) (void* ud, int lid, int nid);					//accept callback
typedef int (*sorcb) (void* ud, int id, char * data, int len);		//read callback
typedef int (*soecb) (void* ud, int id, int ui);					//error callback
typedef int (*soccb) (void* ud, int id, int ui); 					//connect success callback
struct somgr_t;

struct somgr_t* somgr_new(void* ud, soacb a, sorcb r, soecb e, soccb c);		//创建一个socket管理器
void somgr_destroy(struct somgr_t* somgr);										//销毁一个socket管理器
void somgr_runonce(struct somgr_t* somgr, int wms);								//外界调用来驱动一次管理器
int somgr_listen(struct somgr_t* somgr, const char* ip, int port);				//向本地发起端口侦听
int somgr_connect(struct somgr_t* somgr, const char* ip, int port, int ud);		//向远程发起连接
int somgr_write(struct somgr_t* somgr, int32_t id, char* data, uint32_t dlen);	//向某连接推送数据, 未必马上到达
int somgr_kick(struct somgr_t* somgr, int32_t id);		//踢掉一条连接
void somgr_notify_s(struct somgr_t* somgr);				//唤醒service
void somgr_notify_g(struct somgr_t* somgr);				//唤醒gate
void somgr_notify_wait_g(struct somgr_t* somgr, int ms);//service模块调来于等待gate事件, gate模块可以随时调用somgr_notify_s来唤醒它
#endif