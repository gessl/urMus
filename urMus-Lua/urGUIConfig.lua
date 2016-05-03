-- urGUIConfig.lua
-- Created 3/4/2014
-- by Georg Essl

FreeAllRegions()

dofile(SystemPath("urColorPicker.lua"))

local fbnames = {"Accel", "SinOsc", "Dac"}

local protos = {}
local protocellheight = 80
local protocellwidth = 64
local backdropborder = 8

-- Flowbox Representation service functions

local function GetLets(cflowbox, fbtype)
    local lets
    if fbtype == 1 then
        lets = {cflowbox:Outs()}
    else
        lets = {cflowbox:Ins()}
        if cflowbox:NumOuts() > 1 then
            local outlets = { cflowbox:Outs()}
            for i=2,#outlets do
                table.insert(lets,outlets[i])
            end
        end
    end
    return lets
end

local activeproto

local function colorchanged(r,g,b,a)
	activeproto.texture:SetSolidColor(r,g,b,a)
	SetColorScheme(r,g,b,a,activeproto.fbtype)
	ImpartColorScheme()
end

colorpicker = CreatePicker(backdropborder,ScreenHeight()/2-40,200,200,4,true,true,colorchanged)

-- Backdrop selector regions for visual guidance

cpbackdrop = Region()
cpbackdrop:SetWidth(ScreenWidth())
cpbackdrop:SetHeight(256)
cpbackdrop:SetAnchor('LEFT',colorpicker,'LEFT',-10,0) 
cpbackdrop.t = cpbackdrop:Texture(64,64,64,255)
cpbackdrop:EnableInput(true)
cpbackdrop:Show()
cpbackdrop:Lower()
cpbackdrop:Lower()
cpbackdrop:Lower()
cpbackdrop:Lower()
cpbackdrop:Lower()
cpbackdrop:Lower()
cpbackdrop:Lower()
cpbackdrop:Lower()
cpbackdrop:Lower()

cpbackselect = Region()
cpbackselect:SetWidth(protocellwidth+backdropborder*2)
cpbackselect:SetHeight(102)
cpbackselect:SetAnchor('BOTTOMLEFT',cpbackdrop,'TOPLEFT',0,0) 
cpbackselect.t = cpbackselect:Texture(64,64,64,255)
cpbackselect:Show()
cpbackselect:EnableInput(true)
cpbackselect:Lower()
cpbackselect:Lower()
cpbackselect:Lower()
cpbackselect:Lower()


function selectproto(self)
	cpbackselect:SetAnchor("CENTER",self,"CENTER",0,-2)
	activeproto = self
end

local function CreateButton(x,y,col,fbtype,label,flowbox,inidx, instance)
	returnbutton=Region('region', 'sourcesel', UIParent)
	returnbutton:SetWidth(protocellwidth)
	returnbutton:SetHeight(protocellheight)
	returnbutton:SetLayer("DIALOG")
	returnbutton:SetAnchor('BOTTOMLEFT',switchbackdrop,'BOTTOMLEFT',x,y) 
	returnbutton.fbtype = fbtype
	returnbutton:Handle("OnTouchDown", selectproto)
	returnbutton.textlabel=returnbutton:TextLabel()
	returnbutton.textlabel:SetFont(urfont)
	returnbutton.textlabel:SetHorizontalAlign("CENTER")
	returnbutton.textlabel:SetLabel(label)
	returnbutton.textlabel:SetFontHeight(urfontheight)
	returnbutton.textlabel:SetColor(255,255,255,255)
	returnbutton.textlabel:SetShadowColor(0,0,0,190)
	returnbutton.textlabel:SetShadowBlur(2.0)
	returnbutton.texture = returnbutton:Texture(texturefiles.button)
	returnbutton.texture:SetBlendMode("BLEND")
	SetFlowboxColor(returnbutton.texture, fbtype)
	returnbutton:EnableInput(true)
	returnbutton:Show()
	return returnbutton
end

local function SelectTexture(self)
	texturepreview.texture:SetTexture(texturefiles.button)
	for k,v in protos do
		v.texture:SetTexture(texturefiles.button)
	end
end



textures =	{ 
			"button-unpadded.png",
			"urMus-basic-circle.png",
            "urMus-basic-faderect.png",
            "urMus-basic-hexagon.png",
            "urMus-basic-pentagon.png",
            "urMus-basic-roundedsquare.png",
            "urMus-basic-roundedsquare2.png",
            "urMus-bevel-circle.png",
            "urMus-bevel-rect.png",
            "urMus-bevel-hexagon.png",
            "urMus-bevel-pentagon.png",
            "urMus-bevel-roundedsquare.png",
            "urMus-bevel-roundedsquare2.png",
			}


local txm = 5
local txpos = ScreenWidth()/(txm-1)
local tx = (txpos-protocellwidth)/2
local txreset = tx
local ty = cpbackdrop:Bottom()-12
local idx = 1


tbackdrop = Region()
tbackdrop:SetWidth(ScreenWidth())
tbackdrop:SetHeight(ty+backdropborder)
tbackdrop:SetAnchor("BOTTOMLEFT",0,0) 
tbackdrop.t = tbackdrop:Texture(48,48,48,255)
tbackdrop:Show()

function SelectGUIConfig(self)
	for i=1,3 do
		for k,v in pairs(protofbcells[i]) do
			if v and v.texture then
				v.texture:SetTexture(self.tpath)
			end
		end
	end
	for k,v in pairs(rowregions) do
		for k2,v2 in pairs(v) do
			if v2.texture then
				v2.texture:SetTexture(self.tpath)
			end
		end
	end
	texturefiles.button = self.tpath

	for k,v in pairs(protos) do
		v.texture:SetTexture(self.tpath)
	end

end


local texselects = {}

for k,v in pairs (textures) do
	local texsel = Region()
	texsel:SetWidth(protocellwidth)
	texsel:SetHeight(protocellheight)
	texsel:SetAnchor('TOPLEFT',tx,ty) 
	idx  = idx + 1
	if idx >= txm then
		tx = txreset
		ty = ty - protocellheight-backdropborder/2
		idx = 1
	else
		tx = tx + txpos
	end
	texsel:Handle("OnTouchDown", SelectTexture)
	texsel.texture = texsel:Texture(v)
	texsel.texture:SetBlendMode("BLEND")
	texsel.tpath = v
	texsel:Handle("OnTouchDown", SelectGUIConfig)
	texsel:EnableInput(true)
	texsel:Show()
end

for fbtype = 1,3 do
	local innr = 0
	local outnr = 0
	local instancenumber = 1
	local flowbox = _G["FB"..fbnames[fbtype]]
	local object = fbnames[fbtype]
	local lets

	lets = GetLets(flowbox, fbtype)

	local inidx
	if fbtype == 1 then
	inidx = outnr
	else
	inidx = innr
	end

	local thisregion = CreateButton(x,y,col,fbtype, object, flowbox, inidx, instancenumber)
	thisregion:Handle("OnDragStop", nil)
	thisregion:EnableMoving(false)

	local label = object
	label = label .."\n"..lets[inidx+1]
	thisregion.textlabel:SetLabel(label)
	thisregion.row = row
	thisregion.column = col
	thisregion.fbtype = fbtype
	thisregion:SetAnchor("BOTTOMLEFT", cpbackdrop, "TOPLEFT", backdropborder+(fbtype-1)*100,backdropborder) 
	protos[fbtype] = thisregion
end

activeproto = protos[1]



local pagebutton = CreatePagerButton()

