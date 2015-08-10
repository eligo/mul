package.cpath = string.format("%s;%s?.so", package.cpath, './3rd/luaso/')	--设置外部c库的搜索路径
local env = require("lualib.class").singleton("env")

local bs = {}
for i = 1, 10 do
	bs[i] = env.newEnv("testb")
end

local last = env.unixMs()
local function aaa()
	--[[if env.id() == 1 then
		local a = env.unixMs()
		print("lua", a - last)
		last = a--env.unixMs()
	end]]
	for k, v in pairs(bs) do
		env:post(v, "test", 1,2,3,{aa=1,bb={cc="aaaaaaaa",dd={ee=111}}})
	end
end
--env:timeout(1, 10000, aaa)



--[[
for i = 1, 300 do
	timer:timeout(10, 10000, function() end)
end]]

local i = 1
local function _acceptfunc(sid)
	print("accept", sid)
	env:post(bs[i], "addClient", sid)
	i = (i + 1) % 10
	if i == 0 then i = 1 end
end

local sid = env:socketListen("0.0.0.0", 10000, function(sid) _acceptfunc(sid) end)

print("listen ret", sid)