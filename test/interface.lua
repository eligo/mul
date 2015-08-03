--这个文件主要实现供底层驱动上层的函数(c_onXxxx这种)
package.cpath = string.format("%s;%s?.so", package.cpath, './3rd/luaso/')	--设置外部c库的搜索路径
local env = require("lualib.class").singleton("env")

local last = env.unixMs()
local function aaa()
	if env.id() == 1 then
		local a = env.unixMs()
		print("lua", a - last)
		last = a--env.unixMs()
	end
end
env:timeout(1, 10000, aaa)
--[[
for i = 1, 300 do
	timer:timeout(10, 10000, function() end)
end]]