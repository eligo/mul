#include "gq.h"
#include "common/global.h"
#include "common/lock.h"
#include "msg.h"
#include <assert.h>
#include <string.h>

struct mq_t {
	struct lock_t *mLock;
	struct msg_t *mHead;
	struct msg_t *mTail;
	struct cell_t *mCell;
	int mIsG;
	struct mq_t *mNext;
};

struct mq_t *mq_create(struct cell_t *cell) {
	struct mq_t *mq = (struct mq_t *)MALLOC(sizeof(*mq));
	memset(mq, 0, sizeof(*mq));
	mq->mLock = lock_new();
	mq->mCell = cell;
	return mq;
}

void mq_release(struct mq_t *mq) {
	lock_delete(mq->mLock);
	free(mq);
}

void mq_lock(struct mq_t *mq) {
	lock_lock(mq->mLock);
}

void mq_unlock(struct mq_t *mq) {
	lock_unlock(mq->mLock);
}

void mq_push_raw(struct mq_t *mq, struct msg_t *msg) {
	if (mq->mTail) {
		assert(!mq->mTail->next);
		mq->mTail->next = msg;
		msg->next = NULL;
		mq->mTail = msg;
	} else {
		assert(!mq->mHead);
		mq->mHead = msg;
		mq->mTail = msg;
		msg->next = NULL;
	}
}

struct msg_t *mq_pop(struct mq_t *mq) {
	struct msg_t *msg = NULL;
	lock_lock(mq->mLock);
	msg = mq->mHead;
	if (msg) {
		mq->mHead = msg->next;
		if (!mq->mHead) {
			assert(mq->mTail == msg);
			mq->mTail = NULL;
		}
	}
	lock_unlock(mq->mLock);
	return msg;
}

int mq_empty(struct mq_t *mq) {
	return NULL == mq->mHead ? 1 : 0;
}

struct cell_t *mq_cell(struct mq_t *mq) {
	return mq->mCell;
}

/*----------------------------------------------*/
struct gq_t {
	struct mq_t *mHead;
	struct mq_t *mTail;
	struct lock_t *mLock;
};

struct gq_t * mGq;
int gq_init() {
	mGq = (struct gq_t *)MALLOC(sizeof(*mGq));
	memset(mGq, 0, sizeof(*mGq));
	mGq->mLock = lock_new();
	return 0;
}

void gq_release() {
	lock_delete(mGq->mLock);
	free(mGq);
}

struct mq_t *gq_pop() {
	struct mq_t *mq = NULL;
	lock_lock(mGq->mLock);
	lock_unlock(mGq->mLock);
	return mq;
}

void gq_push_mq_raw(struct mq_t *mq) {
	mq->mNext = NULL;
	if (mGq->mTail) {
		assert(!mGq->mTail->mNext);
		mGq->mTail->mNext = mq;
		mGq->mTail = mq;
	} else {
		assert(!mGq->mHead);
		mGq->mHead = mq;
		mGq->mTail = mq;
	}
}

void gq_push_msg(struct mq_t *mq, struct msg_t *msg) {
	mq_lock(mq);
	mq_push_raw(mq, msg);
	if (!mq->mIsG) {
		assert(!mq->mNext);
		lock_lock(mGq->mLock);
		gq_push_mq_raw(mq);
		lock_unlock(mGq->mLock);
		mq->mIsG = 1;
	}
	mq_unlock(mq);
}

void gq_worker_end(struct mq_t *mq) {
	mq_lock(mq);
	assert(mq->mIsG);
	if (mq_empty(mq)) {
		lock_lock(mGq->mLock);
		gq_push_mq_raw(mq);
		lock_unlock(mGq->mLock);
	} else
		mq->mIsG = 0;
	mq_unlock(mq);
}