#include "env.h"
#include "gq.h"
#include "gt.h"
#include "gs.h"
#include "msg.h"
#include "common/timer/timer.h"
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static pthread_cond_t cond;
static pthread_mutex_t mutex;
int wokerNum = 1;
int wokingNum = 0;
static void *tWorker(void *ptr);
static void *tTimer(void *ptr);
static void *tSocket(void *ptr);
int main() {
	int i = 0;
	int ret = 0;
	uint32_t bootId = 0;
	pthread_t tt, ts;
	pthread_t tw[wokingNum];
	if (pthread_mutex_init(&mutex, NULL)) {
		fprintf(stderr, "Init mutex error");
		exit(1);
	}

	if (pthread_cond_init(&cond, NULL)) {
		fprintf(stderr, "Init cond error");
		exit(1);
	}
	
	time_global_reset();
	env_init();
	gq_init();
	gt_init();
	gs_init();
	ret = env_create("test", &bootId);
	assert(ret == 0);
	pthread_create(&tt, NULL, tTimer, NULL);
	pthread_create(&ts, NULL, tSocket, NULL);
	for (i=0; i<wokerNum; ++i) 
		pthread_create(&tw[i], NULL, tWorker, NULL);

	while (1) {	//todo signal hook
		usleep(1000000);
	}
	
	gs_release();
	gt_release();
	gq_release();
	env_release();
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);
	return 1;
}

void *tTimer(void *ptr) {
	for (;;) {
		usleep(10000);
		time_global_reset();
		gt_update();
		if (__sync_add_and_fetch(&wokingNum, 0) < wokerNum)
			pthread_cond_signal(&cond);
	}
	return NULL;
}

void *tSocket(void *ptr) {
	for (;;) {
		gs_update();
	}
	return NULL;
}

void *tWorker(void *ptr) {
	for (;;) {
		struct mq_t *mq = gq_pop();
		if (mq) {
			for (;;) {
				struct msg_t *msg = mq_pop(mq);
				if (msg) {
					env_process_msg(mq_env(mq), msg);
				} else {
					gq_worker_end(mq);
					break;
				}
			}
		} else {
			if (pthread_mutex_lock(&mutex)) {
				fprintf(stderr, "lock mutex error");
				exit(1);
			}
			__sync_fetch_and_sub(&wokingNum, 1);
			pthread_cond_wait(&cond, &mutex);
			__sync_fetch_and_add(&wokingNum, 1);
			if (pthread_mutex_unlock(&mutex)) {
				fprintf(stderr, "unlock mutex error");
				exit(1);
			}
		}
	}
	return NULL;
}