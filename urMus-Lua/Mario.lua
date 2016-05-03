--[[
Bruno Yoshioka
Sudarshan Sivaramakrishnan
Brian Kaufman
Aditi Rajagopal
Michael McCrindle
]]--
FreeAllRegions()
FreeAllFlowboxes()
DPrint("")

local sw = ScreenWidth()
local sh = ScreenHeight()
local fs = sw --600 --desired dimension for square full size sprite, in pixels

--Set up flowboxes
dac = FBDac
sample = FlowBox(FBSample)
push_loop = FlowBox(FBPush)
push_pos = FlowBox(FBPush)
push_sound = FlowBox(FBPush)
push_rate = FlowBox(FBPush)

--sample is an table of sound files
sample:AddFile(SystemPath("smb_jump-small.wav"))
sample:AddFile(SystemPath("smb_gameover.wav"))
sample:AddFile(SystemPath("smb_mariodie.wav"))
sample:AddFile(SystemPath("smb_pipe.wav"))
sample:AddFile(SystemPath("smb_stage_clear.wav"))


dac.In:SetPull(sample.Out)
push_loop.Out:SetPush(sample.Loop)
push_loop:Push(-1) --set looping off
push_pos.Out:SetPush(sample.Pos)
push_sound.Out:SetPush(sample.Sample)
push_rate.Out:SetPush(sample.Rate)
push_rate:Push(0.1)

push_pos:Push(1) --hack: don't play sound at start of program

function playSound(index)
	push_pos:Push(0)
	push_sound:Push(index/5)
end

function playPipe()
	playSound(4)
end

function noLives()
	playSound(2)
end

function stageClear()
	playSound(5)
end


mario = Region()
mario.t = mario:Texture(SystemPath("mario-idle.png"))
mario.t:SetSolidColor(255, 255, 255, 255)
mario.texture = "idle"
mario.type = "mario"
mario.death = 3
mario.direction = "right"
mario:EnableInput(true)
mario:SetLayer("BACKGROUND")
mario:SetHeight(fs) --full size height
mario:SetWidth(fs) --full size width
mario:SetAnchor("CENTER", UIParent, "CENTER", 0, 0)
mario.t:SetTexCoord(0,300/512,300/512,0)
mario:Handle("OnDoubleTap", playPipe)
mario:Show()

function realignMario()
	mario:SetHeight(fs) --full size height
	mario:SetWidth(fs) --full size width
	mario.t:SetTexCoord(0,300/512,300/512,0)
end

function checkDirection()
	if mario.direction == "left" then
		mario.t:SetTexCoord(300/512,0,300/512,0)
	end
end

function runR()
    mario.t:SetSolidColor(255, 255, 255, 255)
	mario.direction = "right"
	if mario.texture == "idle" then
		mario.texture = "walk"
		mario.t:SetTexture(SystemPath(mario.type.."-walk.png"))
	else
		mario.texture = "idle"
		mario.t:SetTexture(SystemPath(mario.type.."-idle.png"))
	end
	mario:SetAnchor("CENTER", UIParent, "CENTER", 0, 0)
	realignMario()
	checkDirection()
end

function runL()
    mario.t:SetSolidColor(255, 255, 255, 255)
	mario.direction = "left"
	if mario.texture == "idle" then
		mario.texture = "walk"
		mario.t:SetTexture(SystemPath(mario.type.."-walk.png"))
	else
		mario.texture = "idle"
		mario.t:SetTexture(SystemPath(mario.type.."-idle.png"))
	end
	mario:SetAnchor("CENTER", UIParent, "CENTER", 0, 0)
	realignMario()
	checkDirection()
end

function jump()
    mario.t:SetSolidColor(255, 255, 255, 255)
	if mario.texture ~= "jump" then
		mario.texture = "jump"
		mario.t:SetTexture(SystemPath(mario.type.."-jump.png"))
		mario:SetAnchor("CENTER", UIParent, "CENTER", 0, sw/3)--160)
		playSound(0)
	else
		mario.texture = "idle"
		mario.t:SetTexture(SystemPath(mario.type.."-idle.png"))
		mario:SetAnchor("CENTER", UIParent, "CENTER", 0, 0)
	end
	realignMario()
	checkDirection()
end

function changeMario(self)
	mario.type = self.type
	mario.texture = "idle"
    mario.t:SetSolidColor(255, 255, 255, 255)
	mario.t:SetTexture(SystemPath(mario.type.."-idle.png"))
	mario:SetAnchor("CENTER", UIParent, "CENTER", 0, 0)
	realignMario()
	checkDirection()
end

function gameOver()
    mario.t:SetSolidColor(255, 255, 255, 255)
	mario.t:SetTexture(SystemPath("mario-game-over.png"))
	mario:SetAnchor("CENTER", UIParent, "CENTER", 0, 0)
	realignMario()
	playSound(mario.death)
end

function makeLabel(region, label)
	region.tl = region:TextLabel()
	region.tl:SetLabel(label)
	region.tl:SetHorizontalAlign("CENTER")
	region.tl:SetVerticalAlign("CENTER")
	region.tl:SetColor(0, 0, 0, 255)
end

function blackScreen(self)
	mario.t:SetSolidColor(0, 0, 0, 255)
	mario:SetAnchor("CENTER", UIParent, "CENTER", 0, 0)
	mario.texture = "walk"
	realignMario()
end

function makeControlRegion(label, x, y, handler)
	r = Region()
	makeLabel(r, label)
	r:SetHeight(100)
	r:SetWidth(100)
	r.t = r:Texture()
	r.t:SetSolidColor(0, 255, 0, 255)
	r:SetAnchor("BOTTOMLEFT", x, y)
	r:Handle("OnTouchUp", handler)
	r:EnableInput(true)
	--r:Show()
	return r
end

black = makeControlRegion("black", 0, 0, blackScreen)
jumpr = makeControlRegion("JUMP", sw-100 ,0, jump)
g_over = makeControlRegion("Game Over", sw/2-50, 0, gameOver)
g_over:Handle("OnDoubleTap", noLives)

reg = makeControlRegion("Mario", 0, sh-100, changeMario)
reg.type = "mario"
reg:Handle("OnDoubleTap", stageClear)

fire = makeControlRegion("FIRE", sw/2-50, sh-100, changeMario)
fire.type = "fire-mario"

sup = makeControlRegion("Super Mario", sw-100, sh-100, changeMario)
sup.type = "super-mario"

runRight = makeControlRegion("Run Right", sw-200, sh/2-100, runR)
runRight:SetHeight(200)
runRight:SetWidth(200)
runLeft = makeControlRegion("Run Left", 0, sh/2-100, runL)
runLeft:SetHeight(200)
runLeft:SetWidth(200)

