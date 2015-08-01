--全局定时器
local class = require("lualib.class")
local timer = class.singleton("timer")	--单例
local external = class.singleton("external")

function timer:__init()
	self._cbs = {}
end

local session = 0
function timer:timeout(ticks, repeatn, func, ...)
	assert(type(func) == 'function')
	if repeatn then 
		assert(type(repeatn) == 'number')
	end
	while 1 do
		session = session + 1
		if not self._cbs[session] then 
			break
		end
	end
	self._cbs[session] = {func, ticks, repeatn or 1, ...}
	external.timeout(ticks, session)
	return session
end

local cb
function timer:onTimer(sess)
	cb = self._cbs[sess]
	if cb then
		cb[3] = cb[3] - 1
		if cb[3] <= 0 then
			self._cbs[sess] = nil
		else
			external.timeout(cb[2], sess)
		end
		cb[1](unpack(cb, 4))
	else
		assert(nil, string.format("error timer id %d", sess))
	end
end
