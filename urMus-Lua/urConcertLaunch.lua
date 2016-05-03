FreeAllRegions()
FreeAllFlowboxes()
DPrint("")
function CreateRegion(x,y,w,h,label,red,g,b,a)
	local r = Region()
	r:SetWidth(w)
	r:SetHeight(h)
	r:SetAnchor("BOTTOMLEFT",x,y)
	r.t = r:Texture(red,g,b,a)
	r.tl = r:TextLabel()
	r.tl:SetFontHeight(ScreenHeight()/20)
	r.tl:SetColor(255,255,255,255)
	r.tl:SetLabel(label)
	r:Show()
	return r
end	
bg = CreateRegion(0,0,ScreenWidth(),ScreenHeight(),"",128,128,128,255)

bh = ScreenHeight()/7-16
bhg = 8
bhf = ScreenHeight()/7

p1 = CreateRegion(0,ScreenHeight()-bh-bhg,ScreenWidth(),bh,"Distress Signal",0,0,0,255)
p21 = CreateRegion(0,ScreenHeight()-bhf-bh-bhg,ScreenWidth()/3-8,bh,"Rain",0,0,0,255)
p22 = CreateRegion(ScreenWidth()/3,ScreenHeight()-bhf-bh-bhg,ScreenWidth()/3-8,bh,"Thunder",0,0,0,255)
p23 = CreateRegion(ScreenWidth()/3*2,ScreenHeight()-bhf-bh-bhg,ScreenWidth()/3-8,bh,"Wind",0,0,0,255)
p3 = CreateRegion(0,ScreenHeight()-2*bhf-bh-bhg,ScreenWidth(),bh,"Swarm",0,0,0,255)
p4 = CreateRegion(0,ScreenHeight()-3*bhf-bh-bhg,ScreenWidth(),bh,"Scary",0,0,0,255)
p51 = CreateRegion(0,ScreenHeight()-4*bhf-bh-bhg,ScreenWidth()/4-8,bh,"TPadL",0,0,0,255)
p52 = CreateRegion(ScreenWidth()/4,ScreenHeight()-4*bhf-bh-bhg,ScreenWidth()/4-8,bh,"TPadS",0,0,0,255)
p53 = CreateRegion(ScreenWidth()/4*2,ScreenHeight()-4*bhf-bh-bhg,ScreenWidth()/4-8,bh,"TPodL",0,0,0,255)
p54 = CreateRegion(ScreenWidth()/4*3,ScreenHeight()-4*bhf-bh-bhg,ScreenWidth()/4-8,bh,"TPodS",0,0,0,255)
p61 = CreateRegion(0,ScreenHeight()-5*bhf-bh-bhg,ScreenWidth()/2-8,bh,"Mario",0,0,0,255)
p62 = CreateRegion(ScreenWidth()/2,ScreenHeight()-5*bhf-bh-bhg,ScreenWidth()/2-8,bh,"Mario Enemy",0,0,0,255)
p7 = CreateRegion(0,ScreenHeight()-6*bhf-bh-bhg,ScreenWidth(),bh,"Falling",0,0,0,255)

p1.launch = "distress.lua"
p21.launch = "rainstick.lua"
p22.launch = "thunder.lua"
p23.launch = "wind.lua"
p3.launch = "urSwarm.lua"
p4.launch = "scary4-15.lua"
p51.launch = "theremin_ipad_light.lua"
p52.launch = "theremin_ipad_sound.lua"
p53.launch = "theremin_ipod_light.lua"
p54.launch = "theremin_ipod_sound.lua"
p61.launch = "Mario.lua"
p62.launch = "MarioObj.lua"
p7.launch = "urFalling.lua"

function doLaunch(self)
	SetPage(2)
	dofile(SystemPath(self.launch))
end

function highlight(self)
	self.t:SetSolidColor(60,60,60,255)
end

function dehighlight(self)
	self.t:SetSolidColor(0,0,0,255)
end

function addHandles(self)
	self:Handle("OnTouchDown",highlight)
	self:Handle("OnTouchUp",doLaunch)
	self:Handle("OnLeave",dehighlight)
	self:EnableInput(true)
end

StartAudio()

addHandles(p1)
addHandles(p21)
addHandles(p22)
addHandles(p23)
addHandles(p3)
addHandles(p4)
addHandles(p51)
addHandles(p52)
addHandles(p53)
addHandles(p54)
addHandles(p61)
addHandles(p62)
addHandles(p7)

StartHTTPServer()
local host,port = HTTPServer()
if host and port then -- Only advertise the web server if it is launched
	DPrint("http://"..host..":"..port.."/\n\n")
end
