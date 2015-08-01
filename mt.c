#include "mt.h"
#include "common/lock.h"
#include "common/timer/timer.h"
#include "common/global.h"
#include <stdlib.h>

struct gt_t {
	struct lock_t *mLock;
	struct timer_t *mTimer;
};

static struct gt_t *gT = NULL;
void _cb (void * ud, uint32_t tid, int erased) {

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

void mt_add(uint32_t source, uint32_t ticks, uint32_t session) {
	lock_lock(gT->mLock);
	timer_add(gT->mTimer, ticks, NULL, _cb, 0);
	lock_unlock(gT->mLock);
}

void mt_update() {
	timer_tick(gT->mTimer);
}