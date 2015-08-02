#include "mt.h"
#include "msg.h"
#include "env.h"
#include "common/lock.h"
#include "common/timer/timer.h"
#include "common/global.h"
#include <stdlib.h>
#include <stdio.h>
struct gt_t {
	struct lock_t *mLock;
	struct timer_t *mTimer;
};

struct tud_t {
	uint32_t from;
	uint32_t session;
};

static struct gt_t *gT = NULL;
static void _cb (void * ud, uint32_t tid, int erased) {
	struct msg_t *msg = (struct msg_t *)MALLOC(sizeof(*msg));
	msg->type = MTYPE_TIMER;
	msg->from = 0;
	msg->session = ((struct tud_t*)ud)->session;
	msg->len = 0;
	msg->next = NULL;
	if (env_post(((struct tud_t*)ud)->from, msg) != 0)
		FREE(msg);
	FREE(ud);
}

int mt_init() {
	gT = (struct gt_t*)MALLOC(sizeof(*gT));
	gT->mLock = lock_new();
	gT->mTimer = timer_new(100*60*10);
	return 0;
}

void mt_release() {
	lock_delete(gT->mLock);
	timer_destroy(gT->mTimer);
	free(gT);
}

void mt_add(uint32_t from, uint32_t ticks, uint32_t session) {
	struct tud_t *ud = (struct tud_t*)MALLOC(sizeof(*ud));
	ud->from = from;
	ud->session = session;
	lock_lock(gT->mLock);
	timer_add(gT->mTimer, ticks, ud, _cb, 1);
	lock_unlock(gT->mLock);
}

void mt_update() {
	lock_lock(gT->mLock);
	timer_tick(gT->mTimer);
	lock_unlock(gT->mLock);
}