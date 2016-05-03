-- urNetTest.lua
-- by Georg Essl 11/9/10

function SendUp(self)
	--SendOSCMessage(host,port,"/urMus/numbers",0.0,0.25,0.5,0.75,1.0)
    SendOSCMessage(host,port,"/urMus/text","test1")
end

function SendDown(self)
	--SendOSCMessage(host,port,"/urMus/numbers",1.0,0.75,0.5,0.25,0.0)
    SendOSCMessage(host,port,"/urMus/text","test2")
end

function SendID(self)
	SendOSCMessage(host,port,"/urMus/text","test3")
end

r = Region()
r:SetWidth(ScreenWidth()/2)
r:SetHeight(ScreenHeight()/2)
r.t = r:Texture()
r.t:SetTexture(255,0,0,2955)
r:SetAnchor("CENTER",UIParent,"CENTER", 0,0)
r:Show()
r:Handle("OnTouchUp",SendUp)
r:Handle("OnTouchDown",SendDown)
r:Handle("OnDoubleTap",SendID)
r:EnableInput(true)

local tttt = 0

function gotOSC(self, num, num2, num3, num4, num5)
    DPrint((num or "nil"))
--    tttt = tttt+1
--    if num then DPrint(num) end
--[[
	if type(num) == "string" then
		DPrint("OSC String: "..num)
	else
		DPrint("OSC: ".. num.." "..(num2 or "nil").." "..(num3 or "nil").." "..(num4 or "nil").." "..(num5 or "nil"))
		r.t:SetSolidColor(255*num,255*(1-num),0,255)
	end
--]]
end

r:Handle("OnOSCMessage",gotOSC)

SetOSCPort(8888)
host,port = StartOSCListener()
--SendOSCMessage(host,port,"/urMus/text","test")
DPrint("OSC: "..host..":"..port)


if pagersize then
local pagebutton=Region('region', 'pagebutton', UIParent)
pagebutton:SetWidth(pagersize)
pagebutton:SetHeight(pagersize)
pagebutton:SetLayer("TOOLTIP")
pagebutton:SetAnchor('BOTTOMLEFT',ScreenWidth()-pagersize-4,ScreenHeight()-pagersize-4)
pagebutton:EnableClamping(true)
pagebutton:Handle("OnTouchDown", FlipPage)
pagebutton.texture = pagebutton:Texture("circlebutton-16.png")
pagebutton.texture:SetGradientColor("TOP",255,255,255,255,255,255,255,255)
pagebutton.texture:SetGradientColor("BOTTOM",255,255,255,255,255,255,255,255)
pagebutton.texture:SetBlendMode("BLEND")
pagebutton.texture:SetTexCoord(0,1.0,0,1.0)
pagebutton:EnableInput(true)
pagebutton:Show()

end