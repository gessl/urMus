--[[
Bruno Yoshioka
Sudarshan Sivaramakrishnan
Brian Kaufman
Aditi Rajagopal
Michael McCrindle
]]--

FreeAllFlowboxes()
FreeAllRegions()
DPrint("")

local sw = ScreenWidth()
local sh = ScreenHeight()
local fs = sw --600 --desired dimension for square full size sprite, in pixels

local num_mario_effects = 7
local mario_images = {}
mario_images["coinblock"] = "block.png"
mario_images["fireflower"] = "fireflower.png"
mario_images["goomba"] = "goomba.png"
mario_images["mushroom"] = "mushroom.png"
mario_images["star"] = "star copy.png"
mario_images["koopa"] = "koopa.png"
mario_images["fireball"] = "fireball.png"

--= {"block.png", "fireflower.png", "goomba.png", "mushroom.png", "star.png", "koopa.png"}

--Set up flowboxes
dac = FBDac
sample = FlowBox(FBSample)
push_loop = FlowBox(FBPush)
push_pos = FlowBox(FBPush)
push_sound = FlowBox(FBPush)

pushRate = FlowBox(FBPush)
pushRate.Out:SetPush(sample.Rate)

--sample is an table of sound files
--order of sounds in sample is the same as order of images in self
sample:AddFile(SystemPath("SFX_coin-mono-48.wav"))
sample:AddFile(SystemPath("SFX_fireball-mono-48.wav"))
sample:AddFile(SystemPath("SFX_goomba-mono-48.wav"))
sample:AddFile(SystemPath("SFX_mushroom-mono-48.wav"))
sample:AddFile(SystemPath("SFX_powerup-mono-48.wav"))
sample:AddFile(SystemPath("SFX_bump-mono-48.wav"))
--[[
sample:AddFile(SystemPath("smb_coin.wav")) --"SFX_coin.wav"))
sample:AddFile(SystemPath("smb_fireball.wav")) --"SFX_fireball.wav"))
sample:AddFile(SystemPath("smb_stomp.wav")) --"SFX_goomba.wav"))
sample:AddFile(SystemPath("smb_1-up.wav")) --"SFX_mushroom.wav"))
sample:AddFile(SystemPath("smb_powerup.wav")) --"SFX_powerup.wav"))
sample:AddFile(SystemPath("smb_bump.wav")) --"SFX_bump.wav"))
--]]
dac.In:SetPull(sample.Out)
push_loop.Out:SetPush(sample.Loop)
push_loop:Push(-1) --set looping off
push_pos.Out:SetPush(sample.Pos)
push_sound.Out:SetPush(sample.Sample)
--pushRate:Push(0.1)

push_pos:Push(1) --hack: don't play sound at start of program

--play sound file associated with the image corresponding to self.index
function playSound(self)
	if self.sound > 0 then
		push_pos:Push(0)
		push_sound:Push(self.sound/(num_mario_effects-1))
	end
end

marioObj = Region()
marioObj.t = marioObj:Texture(0, 0, 0, 255)
--marioObj.t:SetTexture(mario_images["coinblock"])
marioObj.sound = -1
marioObj:EnableInput(true)
marioObj:SetLayer("BACKGROUND")
marioObj:SetHeight(fs) --full size height
marioObj:SetWidth(fs) --full size width
marioObj:SetAnchor("CENTER", UIParent, "CENTER", 0, 0)
marioObj.t:SetTexCoord(0,300/512,300/512,0)
marioObj:Handle("OnTouchUp", playSound)
marioObj:Show()

function realignMarioObj()
	marioObj:SetHeight(fs) --full size height
	marioObj:SetWidth(fs) --full size width
	marioObj.t:SetTexCoord(0,300/512,300/512,0)
end

function changeTexture(self)
    marioObj.t:SetSolidColor(255, 255, 255, 255)
	marioObj.t:SetTexture(SystemPath(mario_images[self.index]))
	marioObj:SetAnchor("CENTER", UIParent, "CENTER", 0, 0)
	realignMarioObj()
	marioObj.sound = self.sound
end

function blackScreen(self)
	marioObj.t:SetSolidColor(0, 0, 0, 255)
	marioObj:SetAnchor("CENTER", UIParent, "CENTER", 0, 0)
	realignMarioObj()
	marioObj.sound = -1
end

function makeLabel(region, label)
	region.tl = region:TextLabel()
	region.tl:SetLabel(label)
	region.tl:SetHorizontalAlign("CENTER")
	region.tl:SetVerticalAlign("CENTER")
	region.tl:SetColor(0, 0, 0, 255)
end

function makeControlRegion(label, x, y, index, sound, handler)
	r = Region()
	makeLabel(r, label)
	r.index = index
	r.sound = sound
	r:SetHeight(100)
	r:SetWidth(100)
	r.t = r:Texture()
	r.t:SetSolidColor(0, 255, 0, 255)
	r:SetAnchor("BOTTOMLEFT", x, y)
	r:Handle("OnTouchUp", handler)
	r:EnableInput(true)
--	r:Show()
	return r
end

star = makeControlRegion("STAR", 0, sh-50, "star", 5, changeTexture)
fireflower = makeControlRegion("FIREFLOWER", sw/4+10, sh-50, "fireflower", 5, changeTexture)
mushroom = makeControlRegion("MUSHROOM", sw/2+20, sh-50, "mushroom", 4, changeTexture)
fireball = makeControlRegion("FIREBALL", sw-50, sh-50, "fireball", 2, changeTexture)
goomba = makeControlRegion("GOOMBA", 0, 0, "goomba", 3, changeTexture)
koopa = makeControlRegion("KOOPA", sw/4+10, 0, "koopa", 6, changeTexture)
coinblock = makeControlRegion("COINBLOCK", sw/2+20, 0, "coinblock", 1, changeTexture)
black = makeControlRegion("BLACK", sw-50, 0, "black", -1, blackScreen)


