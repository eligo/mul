--全局定时器
local class = require("lualib.class")
local posed = class.singleton("posed")
local env = class.singleton("env")
local cmsgpack = cmsgpack
function env:__init()
	self.mTimerCbs = {}
	self.mTimerSession = 0
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
	--return cmsgpack.pack(onPosed(cmsgpack.unpack(pack)))
	onPosed(cmsgpack.unpack(pack))
end
return env