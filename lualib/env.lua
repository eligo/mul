--全局定时器
local class = require("lualib.class")
local env = class.singleton("env")

function env:__init()
	self.mTimerCbs = {}
	self.mTimerSession = 0
end

function env:timeout(ticks, repeatn, func, ...)
	assert(type(func) == 'function')
	if repeatn then 
		assert(type(repeatn) == 'number')
	end
	while 1 do
		self.mTimerSession = self.mTimerSession + 1
		if not self.mTimerCbs[self.mTimerSession] then 
			break
		end
	end
	self.mTimerCbs[self.mTimerSession] = {func, ticks, repeatn or 1, ...}
	env.timeoutRaw(ticks, self.mTimerSession)
	return self.mTimerSession
end

local cb
function c_onTimer(sess)
	cb = env.mTimerCbs[sess]
	if cb then
		cb[3] = cb[3] - 1
		if cb[3] <= 0 then
			env.mTimerCbs[sess] = nil
		else
			env.timeoutRaw(cb[2], sess)
		end
		cb[1](unpack(cb, 4))
	else
		assert(nil, string.format("error timer id %d", sess))
	end
end

return env