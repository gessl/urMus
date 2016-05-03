FreeAllRegions()
local host2 = "67.194.78.121" 

function SendUp(self)
    SendOSCMessage(host2,8888,"/urMus/numbers",0.0)
end

function SendDown(self)
    SendOSCMessage(host2,8888,"/urMus/numbers",1.0)
end

r = Region()

r:SetWidth(ScreenWidth()/2)
r:SetHeight(ScreenHeight()/2)

r:SetAnchor("CENTER",UIParent,"CENTER", 0,0)
r:Handle("OnTouchUp",SendUp)
r:Handle("OnTouchDown",SendDown)
r:EnableInput(true)

r2 = Region()
r2:SetWidth(ScreenWidth()/2)
r2:SetHeight(ScreenHeight()/2)
r2:SetAnchor("CENTER",UIParent,"CENTER", 0,0)
--r2.t = r2:Texture(DocumentPath("Static-TreeHill.png"))
r2.t = r2:Texture("Static-TreeHill.png")
r2:Show()

r3 = Region()
r3:SetWidth(ScreenWidth()/2)
r3:SetHeight(ScreenHeight()/2)
r3:SetAnchor("CENTER",UIParent,"CENTER", 0,0)
--r3.t = r3:Texture(DocumentPath("LightningFlash.png"))
r3.t = r3:Texture("LightningFlash.png")
r3:Hide()


function gotOSC(self, num)
    DPrint("OSC: ".. num)
	if(num==1) then
	    r2:Hide()
		r3:Show()
	else
		r2:Show()
		r3:Hide()
	end
end

r:Handle("OnOSCMessage",gotOSC)

SetOSCPort(8888)
host, port = StartOSCListener()

