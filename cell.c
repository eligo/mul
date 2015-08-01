#include "cell.h"
#include "gq.h"
#include "common/global.h"
#include "common/lock.h"
#include "msg.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include <string.h>

struct cell_t {
	uint32_t mId;
	struct lua_State *mLvm;
	struct mq_t *mMq;
};

/*  ****************************************************  */
struct cellmgr_t {
	struct lock_t *mLock;
	struct cell_t **mList;
	size_t mListLen;
};

struct cellmgr_t * gCm = NULL;
int cell_init() {
	gCm = (struct cellmgr_t*)MALLOC(sizeof(*gCm));
	memset(gCm, 0, sizeof(*gCm));
	gCm->mLock = lock_new();
	return 0;
}

void cell_release() {
	lock_delete(gCm->mLock);
	free(gCm);
}

int cell_create(const char *script, uint32_t *idR) {
	size_t i;
	uint32_t id = 0;
	struct cell_t *cell = NULL;
	lock_lock(gCm->mLock);
	for(i=1; i<gCm->mListLen; ++i) {
		if (!gCm->mList[i]) {
			id = i;
			break;
		}
	}
	if (!id) {
		id = gCm->mListLen;
		size_t n = gCm->mListLen + 1024;
		gCm->mList = (struct cell_t **)REALLOC((void*)gCm->mList, n*sizeof(struct cell_t *));
		for (i=gCm->mListLen; i<n; ++i)
			gCm->mList[i] = NULL;
		gCm->mListLen = n;
	}
	cell = (struct cell_t *)MALLOC(sizeof(*cell));
	memset(cell, 0, sizeof(*cell));
	cell->mId = id;
	gCm->mList[id] = cell;
	lock_unlock(gCm->mLock);
	cell->mMq = mq_create(cell);
	//TODO load scripts
	*idR = id;
	return 0;
}

int cell_post(uint32_t tocell, struct msg_t *msg) {
	struct cell_t *cell = NULL;
	if (!tocell || tocell > gCm->mListLen)
		return -1;

	cell = gCm->mList[tocell];
	if (!cell)
		return -2;

	gq_push_msg(cell->mMq, msg);
	return 0;
}

int cell_process_msg(struct cell_t* cell, struct msg_t *msg) {
	return 0;
}