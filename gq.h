#ifndef __env_GQ_HEADER__
#define __env_GQ_HEADER__
struct env_t;
struct mq_t;
struct msg_t;

struct mq_t *mq_create(struct env_t *cell);
void mq_release(struct mq_t *mq);
struct msg_t *mq_pop(struct mq_t *mq);
struct env_t *mq_cell(struct mq_t *mq);

int  gq_init();
void gq_release();
void gq_worker_end(struct mq_t *mq);
void gq_push_msg(struct mq_t *mq, struct msg_t *msg);
struct mq_t *gq_pop();
#endif