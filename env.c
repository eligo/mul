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
	}
	FREE(msg);
	return 0;
}

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
	env->mMq = mq_create(env);
	env->mLvm = lua_open();
	luaL_openlibs(env->mLvm);
	luaopen_cmsgpack(env->mLvm);
	//注入c接口
	if(luaL_dostring(env->mLvm, "return require (\"lualib.env\")") != 0) {
		fprintf(stderr, "%s\n", lua_tostring(env->mLvm, -1));
		goto fail;
	}

#define INJECT_C_FUNC(func, name) lua_pushlightuserdata(env->mLvm, env); lua_pushcclosure(env->mLvm, func, 1); lua_setfield(env->mLvm, -2, name);
	INJECT_C_FUNC(c_unixMs, "unixMs");
	INJECT_C_FUNC(c_timeoutRaw, "timeoutRaw");
	INJECT_C_FUNC(c_id, "id");
	INJECT_C_FUNC(c_newEnv, "newEnv");
	INJECT_C_FUNC(c_postRaw, "postRaw");
	
	size_t plen=0;
	lua_getglobal(env->mLvm, "package");
	lua_getfield(env->mLvm, -1, "path");
	const char* path = luaL_checklstring(env->mLvm, -1, &plen);
	char* npath = MALLOC(plen + strlen(script) + 8);
	sprintf(npath, "%s;%s/?.lua", path, script);
	lua_pushstring(env->mLvm, npath);
	lua_setfield(env->mLvm, -3, "path");
	FREE(npath);
	//加载lua脚本的首个文件(文件名已定死)
	char* loadf = MALLOC(strlen(script) + sizeof("/interface.lua") + 1);
	strcpy(loadf, script);
	strcat(loadf, "/interface.lua");
	if (luaL_dofile(env->mLvm, loadf) != 0) {
		FREE(loadf);
		fprintf(stderr, "%s\n", lua_tostring(env->mLvm, -1));
		goto fail;
	}
	FREE(loadf);

#define CACHE_L_EVHANDLE(name, idx) lua_getglobal(env->mLvm, name); if (!lua_isfunction (env->mLvm, -1)) {fprintf(stderr, "cannot find event handle'%s'",name);goto fail;} else {*idx=lua_gettop(env->mLvm);}
		CACHE_L_EVHANDLE("c_onTimer", &env->idx_onTimer);
		CACHE_L_EVHANDLE("c_onPosed", &env->idx_onPosed);
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
