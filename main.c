#include "cell.h"
#include "gq.h"
#include "mt.h"
#include "msg.h"
#include "common/timer/timer.h"
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static pthread_cond_t cond;
static pthread_mutex_t mutex;
int woker_num = 0;
int woking_num = 0;

static void *_worker(void *ptr);
static void *_timer(void *ptr);
int main() {
	pthread_t tt;
	pthread_t tw;
	uint32_t id=0;
	if (pthread_mutex_init(&mutex, NULL)) {
		fprintf(stderr, "Init mutex error");
		exit(1);
	}
	if (pthread_cond_init(&cond, NULL)) {
		fprintf(stderr, "Init cond error");
		exit(1);
	}
	
	gq_init();
	mt_init();
	cell_init();
	int i = 0;
	for (i = 0; i < 100 ; i++) {
		int ret = cell_create("test", &id);
		assert(ret == 0);
	}

	pthread_create(&tt, NULL, _timer, NULL);
	pthread_create(&tw, NULL, _worker, NULL);
	pthread_create(&tw, NULL, _worker, NULL);
	pthread_create(&tw, NULL, _worker, NULL);

	while (1) {
		usleep(1000000);
	}
	
	cell_release();
	mt_release();
	gq_release();
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);
	return 1;
}

void *_timer(void *ptr) {
	for (;;) {
		usleep(10000);
		time_global_reset();
		mt_update();
		if (__sync_add_and_fetch(&woking_num, 0) < woker_num)
			pthread_cond_signal(&cond);
	}
	return NULL;
}

void *_worker(void *ptr) {
	for (;;) {
		struct mq_t *mq = gq_pop();
		if (mq) {
			for (;;) {
				struct msg_t *msg = mq_pop(mq);
				if (msg) {
					cell_process_msg(mq_cell(mq), msg);
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
			__sync_fetch_and_sub(&woking_num, 1);
			pthread_cond_wait(&cond, &mutex);
			__sync_fetch_and_add(&woking_num, 1);
			if (pthread_mutex_unlock(&mutex)) {
				fprintf(stderr, "unlock mutex error");
				exit(1);
			}
		}
	}
	return NULL;
}