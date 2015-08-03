#include "env.h"
#include "gq.h"
#include "gt.h"
#include "common/timer/timer.h"
#include "common/global.h"
#include "common/lock.h"
#include "common/lserial.h"
#include "msg.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include <string.h>

struct env_t {
	uint32_t mId;
	struct lua_State *mLvm;
	struct mq_t *mMq;
	int idx_onTimer;
	int idx_onPosed;
};

struct envmgr_t {
	struct lock_t *mLock;
	struct env_t **mList;
	size_t mListLen;
};

struct envmgr_t * gEnvMgr = NULL;
int env_init() {
	gEnvMgr = (struct envmgr_t*)MALLOC(sizeof(*gEnvMgr));
	memset(gEnvMgr, 0, sizeof(*gEnvMgr));
	gEnvMgr->mLock = lock_new();
	return 0;
}

void env_release() {
	lock_delete(gEnvMgr->mLock);
	free(gEnvMgr);
}

int env_post(uint32_t toEnv, struct msg_t *msg) {
	struct env_t *env = NULL;
	if (!toEnv || toEnv > gEnvMgr->mListLen) return -1;

	env = gEnvMgr->mList[toEnv];
	if (!env) return -2;

	gq_push_msg(env->mMq, msg);
	return 0;
}

int lua_error_cb(lua_State *L) {
    lua_getfield(L, LUA_GLOBALSINDEX, "debug");
    lua_getfield(L, -1, "traceback");
    lua_pushvalue(L, 1);
    lua_pushinteger(L, 2);
    lua_call(L, 2, 1);
    fprintf(stderr, "\n%s\n\n", lua_tostring(L, -1));
    return 1;
}

int env_process_msg(struct env_t* env, struct msg_t *msg) {
	switch(msg->type) {
	case MTYPE_POST: {
		struct lua_State * lvm = env->mLvm;
		int st = lua_gettop(lvm);
		lua_pushcfunction(lvm, lua_error_cb);
		lua_pushvalue(lvm, env->idx_onPosed);
		lua_pushlstring(lvm, ((char*)msg)+sizeof(*msg), msg->len);
		lua_pcall(lvm, 1, 0, -3);
		lua_settop(lvm, st);
		break;
	}
	case MTYPE_TIMER: {
		struct lua_State * lvm = env->mLvm;
		int st = lua_gettop(lvm);
		lua_pushcfunction(lvm, lua_error_cb);
		lua_pushvalue(lvm, env->idx_onTimer);
		lua_pushnumber(lvm, msg->session);
		lua_pcall(lvm, 1, 0, -3);
		lua_settop(lvm, st);
		break;
	}
	}
	FREE(msg);
	return 0;
}

/*
	external interface
*/
int c_unixMs(struct lua_State *lvm) {
	lua_pushnumber(lvm, (lua_Number)time_ms());
	return 1;
}

int c_timeoutRaw(struct lua_State *lvm) {
	struct env_t *env = lua_touserdata(lvm, lua_upvalueindex(1));
	uint32_t ticks = luaL_checkinteger(lvm, 1);
	uint32_t session = luaL_checkinteger(lvm, 2);
	gt_add(env->mId, ticks, session);
	return 0;
}

int c_id(struct lua_State *lvm) {
	struct env_t *env = lua_touserdata(lvm, lua_upvalueindex(1));
	lua_pushnumber(lvm, env->mId);
	return 1;
}

int c_newEnv(struct lua_State *lvm) {
	uint32_t id=0;
	int err = 0;
	const char *script = luaL_checkstring(lvm, 1);
	err = env_create(script, &id);
	if (err != 0)
		lua_pushnil(lvm);
	else
		lua_pushnumber(lvm, id);
	lua_pushnumber(lvm, err);
	return 2;
}

int c_postRaw(struct lua_State *lvm) {
	int ret = 0;
	size_t plen = 0;
	struct env_t *env = lua_touserdata(lvm, lua_upvalueindex(1));
	uint32_t to = luaL_checkinteger(lvm, 1);
	const char *pack = luaL_checklstring(lvm, 2, &plen);
	struct msg_t *msg = (struct msg_t *)MALLOC(sizeof(*msg) + plen);
	msg->type = MTYPE_POST;
	msg->from = env->mId;
	msg->session = 0;
	msg->len = plen;
	msg->next = NULL;
	memcpy(((char*)msg)+sizeof(*msg), pack, plen);
	ret = env_post(to, msg);
	if (ret != 0)
		FREE(msg);
	lua_pushinteger(lvm, ret);
	return 0;
}

/*
	create abount
*/
typedef int (*clFunc)(struct lua_State *);
static int _inject(struct env_t *env, clFunc func, const char *fname) {
	lua_pushlightuserdata(env->mLvm, env); 
	lua_pushcclosure(env->mLvm, func, 1); 
	lua_setfield(env->mLvm, -2, fname);
	return 0;
}

static int _boot(struct env_t *env, const char *script) {
	size_t plen=0;
	const char *path = NULL;
	char *tmpstr = NULL;
	lua_getglobal(env->mLvm, "package");
	lua_getfield(env->mLvm, -1, "path");
	path = luaL_checklstring(env->mLvm, -1, &plen);
	tmpstr = MALLOC(plen + strlen(script) + 16 + sizeof("/interface.lua"));
	sprintf(tmpstr, "%s;%s/?.lua", path, script);
	lua_pushstring(env->mLvm, tmpstr);
	lua_setfield(env->mLvm, -3, "path");
	strcpy(tmpstr, script);
	strcat(tmpstr, "/interface.lua");
	if (luaL_dofile(env->mLvm, tmpstr) != 0) {
		FREE(tmpstr);
		fprintf(stderr, "%s\n", lua_tostring(env->mLvm, -1));
		return -1;
	}
	FREE(tmpstr);
	return 0;
}

static int _locate(struct env_t *env, const char *name, int *idx) {
	lua_getglobal(env->mLvm, name); 
	if (!lua_isfunction (env->mLvm, -1)) {
		fprintf(stderr, "cannot locate framework handle:'%s'",name);
		return -1;
	} else 
		*idx=lua_gettop(env->mLvm);
	return 0;
}

int env_create(const char *script, uint32_t *idR) {
	size_t i;
	uint32_t id = 0;
	struct env_t *env = NULL;
	lock_lock(gEnvMgr->mLock);
	for(i=1; i<gEnvMgr->mListLen; ++i) {
		if (!gEnvMgr->mList[i]) {
			id = i;
			break;
		}
	}
	if (!id) {
		id = gEnvMgr->mListLen == 0 ? 1 : gEnvMgr->mListLen;
		size_t n = gEnvMgr->mListLen + 1024;
		gEnvMgr->mList = (struct env_t **)REALLOC((void*)gEnvMgr->mList, n*sizeof(struct env_t *));
		for (i = gEnvMgr->mListLen; i<n; ++i)
			gEnvMgr->mList[i] = NULL;
		gEnvMgr->mListLen = n;
	}
	env = (struct env_t *)MALLOC(sizeof(*env));
	memset(env, 0, sizeof(*env));
	env->mId = id;
	gEnvMgr->mList[id] = env;
	lock_unlock(gEnvMgr->mLock);
	//init env
	env->mMq = mq_create(env);
	env->mLvm = lua_open();
	luaL_openlibs(env->mLvm);
	luaopen_cmsgpack(env->mLvm);
	if(luaL_dostring(env->mLvm, "return require (\"lualib.env\")") != 0) {
		fprintf(stderr, "%s\n", lua_tostring(env->mLvm, -1));
		goto fail;
	}
	//inject c func
	_inject(env, c_unixMs, "unixMs");
	_inject(env, c_timeoutRaw, "timeoutRaw");
	_inject(env, c_id, "id");
	_inject(env, c_newEnv, "newEnv");
	_inject(env, c_postRaw, "postRaw");
	//locate framework func
	if (_locate(env, "c_onTimer", &env->idx_onTimer)) goto fail;
	if (_locate(env, "c_onPosed", &env->idx_onPosed)) goto fail;
	//boot entry script
	if (_boot(env, script))	goto fail;

	*idR = id;
	return 0;
fail:
	if (env) {
		if (env->mLvm)
			lua_close(env->mLvm);
		if (env->mMq)
			mq_release(env->mMq);
		FREE(env);
	}
	return -1;
}
