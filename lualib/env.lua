--全局定时器
local class = require("lualib.class")
local posed = class.singleton("posed")
local env = class.singleton("env")
local cmsgpack = cmsgpack
function env:__init()
	self.mTimerCbs = {}
	self.mTimerSession = 0
	self.mSockets = {}
end

local sess
function env:timeout(ticks, repeatn, func, ...)
	assert(type(func) == 'function')
	if repeatn then 
		assert(type(repeatn) == 'number')
	end
	sess = self.mTimerSession + 1
	while 1 do
		if sess > 0xffffffff then
			sess = 0
		end
		if not self.mTimerCbs[sess] then 
			break
		end
		sess = sess + 1
	end
	self.mTimerSession = sess
	self.mTimerCbs[self.mTimerSession] = {func, ticks, repeatn or 1, ...}
	env.timeoutRaw(ticks, self.mTimerSession)
	return self.mTimerSession
end

local postpack
function env:post(tar, fname, ...)
	postpack = cmsgpack.pack(fname, ...)
	return env.postRaw(tar, postpack)
end

function env:socketListen(ip, port, callback)
	assert(type(callback) == "function")
	local lid = env.socketListenRaw(ip, port)
	if lid > 0 then
		assert(not self.mSockets[lid])
		self.mSockets[lid] = callback
	end
	return lid
end

function env:socketAdd(sid, readcb)
	assert(not self.mSockets[lid])
	assert(type(readcb) == "function")
	local err = self.socketAddRaw(sid)
	if err == 0 then
		self.mSockets[sid] = readcb
	else
		return err
	end
end
--[[
	framework events
]]

local callback
function c_onTimer(sess)
	callback = env.mTimerCbs[sess]
	if callback then
		callback[3] = callback[3] - 1
		if callback[3] <= 0 then
			env.mTimerCbs[sess] = nil
		else
			env.timeoutRaw(callback[2], sess)
		end
		callback[1](unpack(callback, 4))
	else
		assert(nil, string.format("error timer id %d", sess))
	end
end

local hdl
local function onPosed(name, ...)
	return posed[name](...)
end

function c_onPosed(pack)
	onPosed(cmsgpack.unpack(pack))
end

local nid
local ecode
local ermsg
local acb
function c_onAcceptable(sid)
	acb = env.mSockets[sid]
	while true do
		nid = env.socketAcceptRaw(sid)
		if nid > 0 then
			ecode, ermsg = pcall(acb, nid)
			if not ecode then
				env.socketCloseRaw(nid)
				print(ermsg)
			end
		elseif nid == 0 then 
			break
		else
			assert()
		end
	end
end

local readcb
function c_onReadable(sid)
	readcb = env.mSockets[sid]
	assert(readcb)
	readcb(sid)
end

return env