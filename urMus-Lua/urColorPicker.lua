-- urColorPicker.lua
-- Created 3/4/2014
-- by Georg Essl

--SetPage(9)
--FreeAllRegions()

local function makeBorder(self,bordersize, r,g,b,a)
	local pickborder = Region()
	pickborder:SetWidth(self:Width()+bordersize)
	pickborder:SetHeight(self:Height()+bordersize)
	if not r then
		r = 255
		g = 255
		b = 255
		a = 255
	end
	pickborder.t = pickborder:Texture(r,g,b,a)
	pickborder:Show()
	pickborder:SetAnchor("CENTER",self,"CENTER",0,0)
	pickborder:Lower()
	return pickborder
end

function applySaturation(r,g,b,s)
	r = math.min(r+s,255)
	g = math.min(g+s,255)
	b = math.min(b+s,255)
	return r,g,b
end

local function alphaChange(self,x,y)
	alpha = y/self:Height()*255
--	DPrint(y.." "..self:Height().." "..alpha)
	r,g,b,a,s  = self.pick.r, self.pick.g, self.pick.b, self.pick.a, self.pick.s
	r,g,b = applySaturation(r,g,b,s)
	self.t:SetSolidColor(r,g,b,alpha)
	self.pick.a = alpha
	if self.pick.callback then
		self.pick.callback(r,g,b,self.pick.a)
	end
end

local function saturationChange(self,x,y)
	saturation = y/self:Height()*255
--	DPrint(y.." "..self:Height().." "..alpha)
	r,g,b,a = self.pick.r, self.pick.g, self.pick.b, self.pick.a

	r,g,b = applySaturation(r,g,b,saturation)

	self.t:SetSolidColor(r,g,b,255)
	self.pick.s = saturation
	if self.pick.callback then
		self.pick.callback(r,g,b,a)
	end
end	

local function colorChange(self,x,y)
   local xx = x*1530/self:Width()
    local yy = y
    local r=0
    local g=0
    local b=0
    local a=yy/self:Height()
    
    if xx < 255 then
        r = 255
        g = xx
        b = 0
    elseif xx < 510 then
        r = 510 - xx
        g = 255
        b = 0
    elseif xx < 765 then
        r = 0
        g = 255
        b = xx - 510
    elseif xx < 920 then
        r = 0 
        g = 920 - xx
        b = 255
    elseif xx < 1175 then
        r = xx - 920
        g = 0
        b = 255
    else
        r = 255
        g = 0
        b = 1530 - xx
    end

    r = r*a
    g = g*a
    b = b*a
    
--	DPrint(r.." "..g.." ".. b.." "..a)

	self.r = r
	self.b = b
	self.g = g

	if self.callback then
		self.callback(r,g,b,self.alpha)
	end

	r,g,b = applySaturation(r,g,b,self.s)
	self.pickalpha.t:SetSolidColor(r,g,b,self.alpha)
	self.picksat.t:SetSolidColor(r,g,b,255)
end


local function SetColor(self,r,g,b,a,s)
	self.r = r
	self.b = b
	self.g = g
	self.a = a
	self.s = s
end

local function GetColor(self,r,g,b,a)
	return applySaturation(self.r, self.g, self.b), self.a
end

local function HidePicker(self)
	self:Hide()
	if self.pickalpha then
		self.pickalpha:Hide()
	end
end

local function ShowPicker(self)
	self:Show()
	if self.pickalpha then
		self.pickalpha:Show()
	end
end


function makeSlider(pick,parent,w,h,bordersize,r,g,b,a,callback)
		local pickalphabg = Region()
		pickalphabg:SetWidth(0.25*w)
		pickalphabg:SetHeight(h)
		pickalphabg:SetAnchor("LEFT", parent, "RIGHT", 0.125*w, 0)
		pickalphabg.t = pickalphabg:Texture(0,0,0,255)
		pickalphabg:Show()
		local pickalpha = Region()
		pickalpha:SetWidth(0.25*w)
		pickalpha:SetHeight(h)
		pickalpha:SetAnchor("LEFT", parent, "RIGHT", 0.125*w, 0)
		pickalpha.t = pickalpha:Texture(pick.r,pick.b,pick.g,pick.a)
		pickalpha:Show()
		pickalpha.t:SetBlendMode("BLEND")
		if bordersize > 0 then
			pickalphaborder = makeBorder(pickalpha, bordersize,255,255,255,255)
			pickalphaborder:Lower()
		end
		pickalpha:EnableInput(true)
		pickalpha:Handle("OnTouchDown", callback)
		pickalpha:Handle("OnMove", callback)
		return pickalpha
end

function CreatePicker(x,y,w,h,bordersize,showalpha,showsaturation,callback,r,g,b,a)
	local pick = Region()
	pick:SetWidth(w)
	pick:SetHeight(h)
	pick.t = pick:Texture("color_map.png")
	pick:Show()
	pick:SetAnchor("BOTTOMLEFT",x,y)
	if bordersize > 0 then
		makeBorder(pick, bordersize)
	end
	pick:Handle("OnTouchDown", colorChange)
	pick:Handle("OnMove", colorChange)
	pick:EnableInput(true)
	pick.alpha = 255
	
	pick.r = 255
	pick.b = 255
	pick.g = 255
	pick.a = 255
	pick.s = 0

	if r then pick.r = r end
	if b then pick.b = b end
	if g then pick.g = g end  
	if a then pick.a = a end

	local leftparent  = pick

	if showsaturation then
		local picksat = makeSlider(pick,pick,w,h,bordersize,pick.r,pick.b,pick.g,pick.a,saturationChange)
		pick.picksat = picksat
		picksat.pick = pick
		leftparent = picksat
	end

	if showalpha then
		local pickalpha = makeSlider(pick,leftparent,w,h,bordersize,pick.r,pick.b,pick.g,pick.a,alphaChange)
		pick.pickalpha = pickalpha
		pickalpha.pick = pick
	end
	pick.SetColor = SetColor
	pick.GetColor = GetColor
	pick.HidePicker = HidePicker
	pick.ShowPicker = ShowPicker
	pick.callback = callback
	return pick
end

--function colorschanged(r,g,b,a)
--end

--picker = CreatePicker(20,100,200,200,4,true,true,colorschanged)
