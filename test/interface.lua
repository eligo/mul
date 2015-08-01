--这个文件主要实现供底层驱动上层的函数(c_onXxxx这种)
package.cpath = string.format("%s;%s?.so", package.cpath, './3rd/luaso/')	--设置外部c库的搜索路径
require("lualib.timer")
local class = require("lualib.class")	--类管理器(模板及实例)
local timer = class.singleton("timer")	--定时器
local external = class.singleton("external")

function c_onTimer(tid, erased)				--框架事件(某定时器到期触发) 定时器使用如 timer:timeout(1, 100, function() print("hello") end) 每1个滴答调用一下func, 重复100下
	timer:onTimer(tid, erased)
end

local last = 0
local function aaa()
	if external.cellid() == 1 then
		print("lua", external.unixms() - last)
		last = external.unixms()
	end
end
timer:timeout(10, 10000, aaa)

for i = 1, 1000 do
	timer:timeout(10, 10000, function() end)
end