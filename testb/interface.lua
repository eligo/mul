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

local str, len
local function rcb(sid)
	while true do
		str, len = env.socketReadRaw(sid,1)
		if len > 0 then
			env.socketWriteRaw(sid, "wellcome")
			print("recv from",sid,str, len)
			if str == "quit\r\n" then
				env.socketCloseRaw(sid)
			end
		elseif len < 0 then
			env.socketCloseRaw(sid)
			print("close", sid)
			break
		else
			break
		end
	end
end

posed.addClient = function(sid)
	print("testb addClient "..env:id(), sid, env:socketAdd(sid, function(sid) rcb(sid) end))
	
end


	