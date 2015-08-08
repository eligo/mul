#include "gs.h"
#include <unistd.h>  
#include <errno.h> 
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/socket.h>  
#include <sys/epoll.h>
#include <fcntl.h>

enum sostate_e {
	so_e_free = 0,
	so_e_errored,
	so_e_listening,
	so_e_connectting,
	so_e_established,
	so_e_rw,
};

struct buf_t {
	char *mBuf;
	uint32_t mCap;
	uint32_t mCur;
}

struct so_t {
	int mId;
	uint32_t mFd;
	volatile int mState;
	int mEv;
	struct lock_t *mLock;
	struct lock_t *mELock;
	struct lock_t *mRLock;
	struct lock_t *mWLock;
	struct buf_t *mRBuf;
	struct buf_t *mWBuf;
	struct epoll_event mEvs[1024]; 
}

struct gs_t {
	int mEp;	
	int mSon;
	struct so_t **mSos;
	uint32_t mIdAlloc;
};

struct gs_t *mGs = NULL;
#define _lockS(so) lock_lock(so->mLock)
#define _unlockS(so) lock_unlock(so->mLock)
#define _lockR(so) lock_lock(so->mRLock)
#define _unlockR(so) lock_unlock(so->mRLock)
#define _lockW(so) lock_lock(so->mWLock)
#define _unlockW(so) lock_unlock(so->mWLock);
#define _lockE(so) lock_lock(so->mELock)
#define _unlockE(so) lock_unlock(so->mELock)

struct buf_t *buf_new() {
	struct buf_t *buf = (struct buf_t*)MALLOC(sizeof(*buf));
	memset(buf, 0, sizeof(*buf));
	return buf;
}

void buf_reset(struct buf_t *buf) {
	buf->mCur = 0;
}

int gs_init() {
	int i=0;
	mGs = (struct gs_t *)MALLOC(sizeof(*mGs));
	memset(mGs, 0, sizeof(*mGs));
	mGs->mEp = epoll_create(1024);
	if (0 >= mGs->mEp) {
		fprintf(stderr, "create epoll fail\n");
		exit(0);
	}
	mGs->mSon = 65535;
	mGs->mSos = (struct so_t **)MALLOC(mGs->mSon * sizeof(struct so_t *));
	for (i=0; i<mGs->mSon; ++i) {
		struct so_t *so = (struct sobuf_t *)MALLOC(sizeof(*so));
		memset(so, 0, sizeof(*so));
		so->mId = i;
		so->mLock = lock_new();
		so->mRLock = lock_new();
		so->mWLock = lock_new();
		so->mRBuf = buf_new();
		so->mWbuf = buf_new();
		mGs->mSos[i] = so;
	}
	return 0;
}

void gs_release() {

}

struct so_t *_grabSAndLock(int sid) {
	struct so_t *so = NULL;
	if (sid < 0) return NULL;
	if (sid > mGs->mSos) return NULL;
	so = mgs->mSos[sid];
	_lockS(so);
	return so;
}

struct so_t *_grabRAndLock(int sid) {
	struct so_t *so = NULL;
	if (sid < 0) return NULL;
	if (sid > mGs->mSos) return NULL;
	so = mgs->mSos[sid];
	_lockR(so);
	return so;
}

struct so_t *_occupySoAndLock(int fd) {
	int i = 1;
	struct so_t *so = NULL;
	for (i = 1; i < mGs->mSon; ++i) {
		so = mGs->mSos[i];
		if (so->mFd == 0) {
			_lockS(so)
			if (buf->fd == 0) {
				buf->fd = fd;
				break;
			}
		}
	}
	return so;
}

int _evAdd(struct so_t *so, int r, int w, int let) {
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));
	ev.data.ptr = so;
	ev.events |= EPOLLERR | EPOLLHUP;
	if (let) ev.events |= EPOLLET;
	if (r) ev.events |= EPOLLIN;
	if (epoll_ctl(mGs->mEp, EPOLL_CTL_ADD, so->mFd, &ev)) 
		return -1;
	return 0;
}

void _evDel(struct so_t *so) {
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));
	epoll_ctl(mGs->mEp, EPOLL_CTL_DEL, so->mFd, &ev);
}

void _soReserve(struct so_t *so) {	//locked outside
	if (so->mFd)
		close(so->mFd);
	so->mState = 0;
	so->mEnv = 0;
	buf_reset(so->mRBuf);
	buf_reset(so->mWBuf);
}

int so_listen(uint32_t env, const char *ip, int port) {
	struct sockaddr_in addr;
	int soflags = 1;
	struct so_t *so = NULL;
	int id = 0;
	int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (fd <= 0) 
		return -1;

	bzero(&addr, sizeof(addr));  
	addr.sin_family = AF_INET;  
	addr.sin_port = htons(port);  
	addr.sin_addr.s_addr = inet_addr(ip);//INADDR_ANY;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &soflags, sizeof(soflags)) != 0) {
		close(fd);
		return -1;
	}
	
	if (0 != bind(fd,  (struct sockaddr *)&addr,  sizeof(struct sockaddr))) {
		close(fd);
		return -2;
	}
	
	if (listen(fd, 128) != 0) {
		close(fd);
		return -3;
	}

	so = _grabSAndLock(fd);
	if (!so) {
		close(fd);
		return -4;
	}

	_lockR(so);
	_lockW(so);
	_lockE(so);
	so->mFd = fd;
	if (0 != _evAdd(so, 1, 0, 0)) {
		_soReserve(so);
		_unlockE(so);
		_unlockW(so);
		_unlockR(so);
		_unlockS(so);
		return -5;
	}
	so->mState = so_e_listening;
	id = so->mId;
	_unlockE(so);
	_unlockW(so);
	_unlockR(so);
	_unlockS(so);
	return id;
}

int so_accept(uint32_t env, int sid) {
	struct so_t* nso = NULL;
	struct sockaddr addr;
	socklen_t addrlen = sizeof(addr);
	int fd = 0;
	int id = 0;
	struct so_t *lso = _grabSAndLock(sid);
	if (!lso) return -1;
	if (lso->mState != so_e_listening) {
		_unlockS(lso);
		return -2;
	}
acce:
	fd = accept(lso->mFd, &addr, &addrlen);
	if (fd == -1) {
		switch (errno) {
		case EINTR:
			goto acce;
		case EAGAIN:
			_unlockS(lso);
			return 0;
		case EMFILE:
			_unlockS(lso);
			return 0;					//可以留到下一次尝试
		}
		goto erro;
		return -5;
	} else if (fd == 0) {
		goto erro;
	}
	nso = _occupyAndLock(fd);
	if (!nso) {
		_unlockS(lso);
		return -7;
	}
	_unlockS(lso);
	_lockR(nso);
	_lockW(nso);
	_lockE(nso);
	nso->mEnv = env;
	nso->mState = so_e_established;
	id = nso->mId;
	_unlockE(nso);
	_unlockW(nso);
	_unlockR(nso);
	_unlockS(nso);
	return id;
erro:
	_lockE(lso);
	if (lso->mState != so_e_errored) {
		_evDel(so);
		lso->mState = so_e_errored;
	}
	_unlockE(lso);
	_soUnlock(lso);
	return -8;
}

int so_close(uint32_t env, int sid) {
	struct so_t *so = _grabSAndLock(sid);
	if (!so) 
		return -1;
	
	_lockR(so);
	_lockW(so);
	_lockE(so);
	if (so->mState == so_e_free) {
		_unlockE(so);
		_unlockW(so);
		_unlockR(so);
		_unlockS(so);
		return -2;
	}

	if (so->mState != so_e_established && so->mState != so_e_errored) {
		_evDel(so);
		so->mState = so_e_errored;
	}
	_soReserve(so);
	_unlockE(so);
	_unlockW(so);
	_unlockR(so);
	_unlockS(so);
	return 0;
}

int so_add(uint32_t env, int sid) {
	struct so_t *so = _querySAndLock(sid);
	if (!so) 
		return -1;

	_lockE(so);
	if (so->mState != so_e_established) {
		_unlockE(so);
		_unlockS(so);
		return -2;
	}

	if (0 != _evAdd(so, 1, 0, 1)) {
		so->mState = so_e_errored;
		_unlockE(so);
		_unlockS(so);
		return -3;
	}
	nso->mEnv = env;
	so->mState = so_e_rw;
	_unlockE(so);
	_unlockS(so);
	return 0;
}

int so_read(int sid, char *buf, size_t cap) {
	int rn = 0;
	int r  = 0; 
	struct so_t *so = _lockR(sid);
	if (!so) 
		return -1;

	if (so->mState != so_e_rw)
		return -2;

	for (;;) {
		r = read(so->fd, buf, cap);	//操作系统读取调用
		if (r > 0) {
			rn += r;
		} else if (r < 0) {
			switch (errno) {
			case EAGAIN:
				goto succ;
			case EINTR:
				continue;
			}
			goto fail;
		} else 
			goto fail;
	}
fail:
	_lockE(so);
	if (so->mState != so_e_errored) {
		_evDel(so);
		so->mState = so_e_errored;
	}
	_unlockE(so);
	_unlockR(so);
	return rn;
succ:
	_unlockR(so);
	return rn;
}

void gs_update() {
	int i = 0;
	int en = epoll_wait(mGs->mEp, mGs->mEvs, 1024, 1000);
	if (en < 0) {
		sprintf(stderr, "epoll_wait ret %d\n", en);
	}
	for (i=0; i<en; ++i) {
		struct so_t* so = mGs->mEvs[i].data.ptr;
		assert(so);
		_lockS(so);
		_lockE(so);
		if (so->mState == so_e_errored) {
			_unlockE(so);
			_unlockS(so);
			continue;
		}

		if (mGs->mEvs[i].events & (EPOLLHUP | EPOLLERR)) {
				if (so->mState != so_e_established)
					_evDel(so);
				so->mState = so_e_errored;
				_notifyErrored(so);
		} else {
			switch(so->mState) {
			case so_e_free:
			case so_e_established:
			case so_e_connectting:
				assert(0);
				break;
			case so_e_listening:
				_notifyAcceptable(so);
				break;
			case so_e_rw: {
				if (mGs->mEvs[i].events & EPOLLIN)
					_notifyReadable(so);
				if (mGs->mEvs[i].events & EPOLLOUT)
					_notifyWritable(so);
				break;
			}
			}
		}
		_unlockE(so);
		_unlockS(so);
	}
}