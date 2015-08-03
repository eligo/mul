--这个文件主要实现供底层驱动上层的函数(c_onXxxx这种)
package.cpath = string.format("%s;%s?.so", package.cpath, './3rd/luaso/')	--设置外部c库的搜索路径
local env = require("lualib.class").singleton("env")
local posed = require("lualib.class").singleton("posed")

local ccc = 0
posed.test = function(a,b,d,e)
	if env:id() == 2 then
		ccc = ccc + 1
		print("this is b call return ", env:id() ,e.bb.dd.ee, ccc)
	end
end