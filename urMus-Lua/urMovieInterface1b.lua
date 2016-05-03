--dofile(DocumentPath("urVen2_urColorWheel.lua"))


FreeAllRegions()

local regions={}

border = Region()
border:Texture(255,255,255,0)

local oldx1, oldx2,oldy1,oldy2
local scale = 1
local distancetoscale = 1 -- Change this to change pinch zoom sensitivity
local color_w = 256
local color_h = 256
local record = 0

local minfps = 100
local maxfps = 0
local meanfps = 0



function handleTouchUp(self)
    oldx1 = nil   
    --DPrint("up")
end

--Make border resize with region
--When region is locked, resize as zoom
function HandleResize(self,x1,y1,x2,y2)
    border:SetWidth(self:Width()+10)
    border:SetHeight(self:Height()+10)  
    
    if self.lock == 1 then
        DPrint("Zoom")
        if oldx1 then
            local olddist = math.sqrt((oldx2-oldx1)*(oldx2-oldx1)+(oldy2-oldy1)*(oldy2-oldy1))
            local dist = math.sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1))
            local ddist = dist-olddist
            scale = scale - ddist/self:Width()/distancetoscale
            DPrint(x1.." "..y1..", "..x2.." "..y2..":"..scale.." ("..ddist..")")
            if scale > 0.5 then scale = 0.5 end
            if scale < 0 then scale = 0 end
            self.t:SetTexCoord(-scale+0.5,0.5+scale,scale+0.5,0.5-scale)
        end
        oldx1 = x1
        oldx2 = x2
        oldy1 = y1
        oldy2 = y2
    end
    
end


--Check to make sure region won't be out of the frame
function CheckBounds(self,elapsed)
    b = self:Bottom()
    l = self:Left()
    if self:Bottom()< menubar:Top()+10 then
        self:SetAnchor("BOTTOMLEFT",l,menubar:Top()+10)
    end 
    if self:Left()<10 then
        self:SetAnchor("BOTTOMLEFT",10,b)
    end
    if self:Right()>ScreenWidth()-10 then
        self:SetAnchor("BOTTOMRIGHT",ScreenWidth()-10,b)
    end
    if self:Top()> ScreenHeight()-10 then
        --DPrint("TOP")
        self:SetAnchor("TOPLEFT",UIParent,"TOPLEFT",l,-10)
    end
    --for i=1, #regions do
    --if regions[i]~= self and regions[i]:RegionOverlap(self) then
    --DPrint("Overlap")
    --end
    --end        
end

--Camera filter
function SetParams(self,x,y,dx,dy)
    DPrint(activefilter.effect.."\n"..x/self:Width()*2-1)
    SetCameraFilterParameter(x/self:Width()*2-1)
end

--Lock the region
function DisableMove(self)
    if self.lock == 0 then
        DPrint("Lock")
        self.lock = 1
        self:EnableMoving(false)
        self:EnableResizing(true)
        self:Handle("OnTouchUp", handleTouchUp)
        self:Handle("OnMove",SetParams)
    else
        DPrint("Unlock")
        self.lock = 0
        self:EnableMoving(true)
        self:EnableResizing(true)
        self:Handle("OnMove",nil)                        
    end
    border:Hide()
    selectindex = nil
end


--Select region with white border
function SelectRegion(self)
    border:MoveToTop()
    border:SetHeight(self:Height()+10)
    border:SetWidth(self:Width()+10)
    border:SetParent(self)
    border:SetAnchor("CENTER", self, "CENTER", 0, 0)
    border:Show()
    self:MoveToTop()
    if option then
        for i=1,4 do
            option[i]:MoveToTop()
        end
    end
    for i=1, #regions do
        if regions[i] == self then
            selectindex = i
        end
    end
end



--Create region when touch down
function CreateRegionAt(x,y)
    DPrint(#regions)
    local region
    region = Region()
    region.lock = 0
    region:SetHeight(ScreenHeight()/4)
    region:SetWidth(ScreenHeight()/4)
    region:EnableMoving(true)
    region:EnableResizing(true)
    region:EnableInput(true)
    region:SetAnchor("CENTER", x, y)
    region.t=region:Texture(math.random(0,255),math.random(0,255),math.random(0,255),255)
    region.t:UseCamera()
    region.t:SetBlendMode("BLEND")
    region.t:SetTiling()
    --region.t=region:Texture(205,201,201,255)
    region:Show()
    --region:Handle("OnUpdate",CheckBounds)
    region:Handle("OnTouchDown", SelectRegion)
    region:Handle("OnDoubleTap", DisableMove)
    region:Handle("OnSizeChanged", HandleResize)
    region:EnableClamping(true)
    region:SetClampRegion(10,timelinebar:Top()+10,ScreenWidth()-20, ScreenHeight()-timelinebar:Top()-20)
    region:MoveToTop()
    
    --region:EnableClamping(doEnableClamping(true))
    table.insert(regions, region)
    return region
end     


--Get x,y when touch down
function TouchDown(self)
    DPrint("TD")
    local x,y = InputPosition()
    CreateRegionAt(x,y)
end


--Remove selected region
function RemoveRegion(self)
    if selectindex ~= nil then
        DPrint("removed")
        regions[selectindex]:Hide()
        regions[selectindex]:EnableInput(false)
        table.remove(regions, selectindex)
        selectindex = nil
    else
        DPrint("no region selected")
    end
end


function ShowFPS(self,elapsed)
    local fps = 1.0/elapsed
    if minfps > fps then minfps = fps end
    if maxfps < fps then maxfps = fps end
    meanfps = meanfps*24/25 + fps/25.0
    local str = "Current: "..fps.."\nMean: "..meanfps.."\nMin: "..minfps.."\nMax: "..maxfps
    DPrint(str)
end

local cam = 1

local fmode = 1
local fmodes = { "NONE", "SATURATION", "CONTRAST", "BRIGHTNESS", "EXPOSURE", "RGB", "SHARPEN", "UNSHARPMASK", 
    "TRANSFORM", "TRANSFORM3D", "CROP", "MASK", "GAMMA", "TONECURVE", "HAZE", "SEPIA", "COLORINVERT", "GRAYSCALE", 
    "THRESHOLD", "ADAPTIVETHRESHOLD", "PIXELLATE", "POLARPIXELLATE", "CROSSHATCH", "SOBELEDGEDETECTION",
    "PREWITTEDGEDETECTION", "CANNYEDGEDETECTION", "XYGRADIENT", --[["HARRISCORNERDETECTION", "NOBLECORNERDETECTION", 
    "SHITOMASIFEATUREDETECTION",--]] "SKETCH", "TOON", "SMOOTHTOON", "TILTSHIFT", "CGA", "POSTERIZE", "CONVOLUTION", 
    "EMBOSS", --[["KUWAHARA",--]] "VIGNETTE", "GAUSSIAN", "GAUSSIAN_SELECTIVE", "FASTBLUR", "BOXBLUR", "MEDIAN", "BILATERAL",
    "SWIRL", "BULGE", "PINCH", "STRETCH", "DILATION", "EROSION", "OPENING", "CLOSING", 
    "PERLINNOISE", --[["VORONI",--]]  "MOSAIC", "DISSOLVE", "CHROMAKEY", "MULTIPLY", "OVERLAY", "LIGHTEN", "DARKEN", "COLORBURN",
    "COLORDODGE", "SCREENBLEND", "DIFFERENCEBLEND", "SUBTRACTBLEND", "EXCLUSIONBLEND", "HARDLIGHTBLEND", "SOFTLIGHTBLEND", 
    --[["CUSTOM",--]] "FILTERGROUP" }

function SwitchEffect(self)
    activefilter.t:SetSolidColor(80,80,80);
    self.t:SetSolidColor(160,160,160);
    SetCameraFilter(self.effect)
    DPrint(self.effect)
    activefilter = self
end

effects_open = 0
lastbutton = 1
buttons =  {}
local bborder = 1
for k,v in pairs(fmodes) do
    buttons[lastbutton] = Region()
    buttons[lastbutton]:SetWidth(ScreenWidth()/8-bborder)
    buttons[lastbutton]:SetHeight(ScreenWidth()/16-bborder)
    buttons[lastbutton].t = buttons[lastbutton]:Texture(80,80,80)
    --buttons[lastbutton]:Show();
    buttons[lastbutton]:SetAnchor("BOTTOMLEFT",((lastbutton-1) % 8)*ScreenWidth()/8, math.floor((lastbutton-1) / 8)*ScreenWidth()/16+200)
    buttons[lastbutton].tl = buttons[lastbutton]:TextLabel()
    buttons[lastbutton].tl:SetLabel(v)
    buttons[lastbutton].effect = v
    buttons[lastbutton]:Handle("OnTouchDown", SwitchEffect)
    --buttons[lastbutton]:EnableInput(true)
    buttons[lastbutton]:SetLayer("HIGH")
    lastbutton = lastbutton + 1
end

activefilter = buttons[1];
buttons[1].t:SetSolidColor(160,160,160);

function SwitchCamera(self)
    DPrint("Camera "..cam)
    if cam == 1 then cam = 2 else cam = 1 end
    SetActiveCamera(cam)
    SetCameraAutoBalance(cam-1)
end

function OpenEffect(self)
    if effects_open == 0 then
        lastbutton = 1
        DPrint("Open effect")
        for k,v in pairs(fmodes) do
            buttons[lastbutton]:Show()
            buttons[lastbutton]:EnableInput(true)
            buttons[lastbutton]:MoveToTop()
            lastbutton = lastbutton + 1
        end
        effects_open = 1
    else
        lastbutton = 1
        DPrint("Close effect")
        for k,v in pairs(fmodes) do
            buttons[lastbutton]:Hide()
            buttons[lastbutton]:EnableInput(false)
            lastbutton = lastbutton + 1
        end
        effects_open = 0
    end
end



local background = Region()
background:SetHeight(ScreenHeight()*6/7-50)
background:SetWidth(ScreenWidth())
background:EnableInput(true)
background:SetAnchor("TOPLEFT",UIParent,"TOPLEFT",0,0)
background.t=background:Texture(240,230,140,255)
background:Show()
background:Handle("OnTouchDown", TouchDown)


--Reset background color
function ResetBackground(self)
    DPrint("background reset")
    background.t=background:Texture(240,230,140,255)
end


--Open color wheel to change background color
function ChangeColor(background)
    if color_wheel.open == 0 then
        DPrint("color wheel open")
        color_wheel.open = 1
        color_wheel:Show()
        color_wheel:MoveToTop()
        color_wheel.region = background
        color_wheel:EnableInput(true)
    else
        DPrint("color wheel closed")
        color_wheel.open = 0
        color_wheel:Hide()
        color_wheel:EnableInput(false)
    end        
end


--Update color
function UpdateColor(self,x,y,dx,dy)
    local xx = (x+dx)*1530/color_w
    local yy = y+dy
    local r=0
    local g=0
    local b=0
    self.region.a=yy-1
    
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
    
    self.region.r = r*self.region.a/255
    self.region.g = g*self.region.a/255
    self.region.b = b*self.region.a/255
    
    background.t:SetTexture(self.region.r,self.region.g,self.region.b,self.region.a)
    DPrint("r"..math.floor(self.region.r).." g"..math.floor(self.region.g).." b"..math.floor(self.region.b).." a"..math.floor(self.region.a)..". Double tap to close.")
end

--function CloseColorWheel(self)
--    DPrint("color wheel closed")
--    color_wheel.open = 0
--    self:Hide()
--    self:EnableInput(false)
--end

color_wheel = Region('region','colorwheel',UIParent)
color_wheel.t = color_wheel:Texture()
color_wheel.t:SetTexture('color_map.png')
color_wheel.open = 0
color_wheel:SetWidth(color_w)
color_wheel:SetHeight(color_h)
color_wheel:SetAnchor("BOTTOMLEFT",UIParent,"BOTTOMLEFT") 
color_wheel:EnableInput(false)
color_wheel:EnableMoving(false)
color_wheel:EnableResizing(false)
color_wheel:Handle("OnMove",UpdateColor)
--color_wheel:Handle("OnDoubleTap",CloseColorWheel)



menubar = Region()
menubar:SetHeight(ScreenHeight()/7)
menubar:SetWidth(ScreenWidth())
menubar:EnableInput(true)
menubar:SetAnchor("BOTTOMLEFT",UIParent,"BOTTOMLEFT",0,0)
menubar:Texture(70,130,180,255)
menubar:Show()
menubar.tl = menubar:TextLabel()
--menubar.tl:SetLabel("MENU")
--menubar.tl:SetFontHeight(25)
--menubar.tl:SetColor(0,0,0,255)
--menubar.tl:SetHorizontalAlign("JUSTIFY")
--menubar:Handle("OnTouchDown",OpenMenu)

optionnames = {"Reset Background","Change Background","Select Effect","Switch Camera","Remove"}


--Open menu when touch down
function OpenMenu(self)
    if not option then
        option = {}
        
        for i=1,5 do
            option[i] = Region()
            option[i]:SetHeight(40)
            option[i]:SetWidth(150)
            option[i]:SetAnchor("BOTTOMLEFT", 450, i*40+60)
            option[i]:Texture(65,105,225,255)
            option[i]:EnableInput(true)
            option[i].t1 = option[i]:TextLabel()
            option[i].t1:SetLabel(optionnames[i])
            option[i].t1:SetFontHeight(15)
            option[i].t1:SetColor(0,0,0,255)
            --option[i]:Show()
        end
    end 
    
    if menu1.open == 0 then
        DPrint("open menu")
        for i=1,5 do
            option[i]:Show()
            option[i]:MoveToTop()
        end
        menu1.open = 1
    else
        DPrint("close menu")
        for i=1,5 do
            option[i]:Hide()
        end
        menu1.open = 0
    end
    
    option[1]:Handle("OnTouchDown",ResetBackground)
    option[2]:Handle("OnTouchDown",ChangeColor)
    option[3]:Handle("OnTouchDown",OpenEffect)
    option[4]:Handle("OnTouchDown",SwitchCamera)
    option[5]:Handle("OnTouchDown",RemoveRegion)
end

menu1 = Region()
menu1:SetHeight(60)
menu1:SetWidth(150)
menu1:EnableInput(true)
menu1:SetAnchor("BOTTOMLEFT",450,40)
menu1:Texture(65,105,225,255)
--menu1:Texture:SetBlendMode("BLEND")
menu1:Show()
menu1.tl = menu1:TextLabel()
menu1.tl:SetLabel("MENU")
menu1.tl:SetFontHeight(25)
menu1.tl:SetColor(0,0,0,255)
--menu1.tl:SetHorizontalAlign("JUSTIFY")
menu1.open = 0
menu1:Handle("OnTouchDown",OpenMenu)


function PlaybackRecord(self)
    DPrint("recording movie")
    WriteMovie("moviefilename.mp4", background:Left(),background:Bottom(),background:Right(),background:Top())
end


function StopRecord(self)
    DPrint("finish movie")
    FinishMovie()
    newregion = CreateRegionAt(ScreenWidth()/3,ScreenHeight()*2/3)
    newregion.t:SetTexture(DocumentPath("moviefilename.mp4"))
    
end


function RegionRecord(self)
    DPrint(record)
    if record == 0 then
        if selectindex ~= nil then
            DPrint("recording region")
--            regions[selectindex].t:WriteMovie("regionfilename.mp4")
            record = 1
        else
            DPrint("no region selected")
        end 
    else
        DPrint("finish region")
--        regions[selectindex].t:FinishMovie()
        record = 0
    end
end


menu_buttons = {}
pics = {"playback.png", "pause.png", "stop.png", "record.png"}

for i=1, 4 do
    menu_buttons[i] = Region()
    menu_buttons[i]:SetHeight(60)
    menu_buttons[i]:SetWidth(60)
    menu_buttons[i]:SetAnchor("BOTTOMLEFT", 100*i-50, 40)
    menu_buttons[i].t=menu_buttons[i]:Texture()
    menu_buttons[i].t:SetTexture(DocumentPath(pics[i]))  
    menu_buttons[i].t:SetBlendMode("BLEND")   
    menu_buttons[i]:EnableInput(true)
    menu_buttons[i]:Show()
end


menu_buttons[1]:Handle("OnTouchDown",PlaybackRecord)
menu_buttons[3]:Handle("OnTouchDown",StopRecord)
menu_buttons[4]:Handle("OnTouchDown",RegionRecord)


timelinebar = Region()
timelinebar:SetHeight(50)
timelinebar:SetWidth(ScreenWidth())
timelinebar:SetAnchor("BOTTOMLEFT", 0, ScreenHeight()/7)
timelinebar:Texture(190,190,190,255)
timelinebar:Show()

border1 = Region()
border1:SetHeight(5)
border1:SetWidth(ScreenWidth())
border1:SetAnchor("BOTTOMLEFT",0,ScreenHeight()/7)
border1:Texture(255,255,255,0)
border1:Show()

border2 = Region()
border2:SetHeight(5)
border2:SetWidth(ScreenWidth())
border2:SetAnchor("BOTTOMLEFT",0,ScreenHeight()/7+timelinebar:Height()-5)
border2:Texture(255,255,255,0)
border2:Show()

timelinescroll = Region()
timelinescroll:SetHeight(30)
timelinescroll:SetWidth(60)
timelinescroll:SetAnchor("BOTTOMLEFT", 3, ScreenHeight()/7+10)
timelinescroll:Texture(105,105,105,255)
timelinescroll:Show()
--timelinescroll:EnableHorizontalScroll(true)
timelinescroll:EnableInput(true)
timelinescroll:EnableMoving(true)
timelinescroll:EnableClamping(true)
timelinescroll:SetClampRegion(10,timelinebar:Bottom()+10,ScreenWidth()-20, timelinebar:Height()-20)

timeline = {}
timeline[1] = {}
​