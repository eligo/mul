#include "cell.h"
#include "gq.h"
#include "mt.h"
#include "common/timer/timer.h"
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
	int idx_timer;
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

int lua_error_cb(lua_State *L) {
    lua_getfield(L, LUA_GLOBALSINDEX, "debug");
    lua_getfield(L, -1, "traceback");
    lua_pushvalue(L, 1);
    lua_pushinteger(L, 2);
    lua_call(L, 2, 1);
    fprintf(stderr, "\n%s\n\n", lua_tostring(L, -1));
    return 1;
}

int cell_process_msg(struct cell_t* cell, struct msg_t *msg) {
	if (msg->type == MTYPE_TIMER) {
		struct lua_State * lvm = cell->mLvm;
		int st = lua_gettop(lvm);
		lua_pushcfunction(lvm, lua_error_cb);
		lua_pushvalue(lvm, cell->idx_timer);
		lua_pushnumber(lvm, msg->session);
		lua_pcall(lvm, 1, 0, -3);
		lua_settop(lvm, st);
	}
	FREE(msg);
	return 0;
}

int c_unixms(struct lua_State *lvm) {
	lua_pushnumber(lvm, (lua_Number)time_ms());
	return 1;
}

int c_timeout(struct lua_State *lvm) {
	struct cell_t *cell = lua_touserdata(lvm, lua_upvalueindex(1));
	uint32_t ticks = luaL_checkinteger(lvm, 1);
	uint32_t session = luaL_checkinteger(lvm, 2);
	mt_add(cell->mId, ticks, session);
	return 0;
}

int c_cellid(struct lua_State *lvm) {
	struct cell_t *cell = lua_touserdata(lvm, lua_upvalueindex(1));
	lua_pushnumber(lvm, cell->mId);
	return 1;
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
		id = gCm->mListLen == 0 ? 1 : gCm->mListLen;
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
	cell->mLvm = lua_open();
	luaL_openlibs(cell->mLvm);
	//注入c接口
	if(luaL_dostring(cell->mLvm, "local class = require (\"lualib.class\") return class.singleton(\"external\")") != 0) {
		fprintf(stderr, "%s\n", lua_tostring(cell->mLvm, -1));
		goto fail;
	}
#define INJECT_C_FUNC(func, name) lua_pushlightuserdata(cell->mLvm, cell); lua_pushcclosure(cell->mLvm, func, 1); lua_setfield(cell->mLvm, -2, name);
	INJECT_C_FUNC(c_unixms, "unixms");
	INJECT_C_FUNC(c_timeout, "timeout");
	INJECT_C_FUNC(c_cellid, "cellid");
	size_t plen=0;
	lua_getglobal(cell->mLvm, "package");
	lua_getfield(cell->mLvm, -1, "path");
	const char* path = luaL_checklstring(cell->mLvm, -1, &plen);
	char* npath = MALLOC(plen + strlen(script) + 8);
	sprintf(npath, "%s;%s/?.lua", path, script);
	lua_pushstring(cell->mLvm, npath);
	lua_setfield(cell->mLvm, -3, "path");
	FREE(npath);
	//加载lua脚本的首个文件(文件名已定死)
	char* loadf = MALLOC(strlen(script) + sizeof("/interface.lua") + 1);
	strcpy(loadf, script);
	strcat(loadf, "/interface.lua");
	if (luaL_dofile(cell->mLvm, loadf) != 0) {
		FREE(loadf);
		fprintf(stderr, "%s\n", lua_tostring(cell->mLvm, -1));
		goto fail;
	}
	FREE(loadf);
#define CACHE_L_EVHANDLE(name, idx) lua_getglobal(cell->mLvm, name); if (!lua_isfunction (cell->mLvm, -1)) {fprintf(stderr, "cannot find event handle'%s'",name);goto fail;} else {*idx=lua_gettop(cell->mLvm);}
		CACHE_L_EVHANDLE("c_onTimer", &cell->idx_timer);
	*idR = id;
	return 0;
fail:
	if (cell) {
		if (cell->mLvm)
			lua_close(cell->mLvm);
		if (cell->mMq)
			mq_release(cell->mMq);
		FREE(cell);
	}
	return -1;
}
