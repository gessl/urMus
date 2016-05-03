--[[***READ ME***

Performance: Scary
Group Members: Spencer Maxfield, Kevin Chow, Aditi Rajagopal, Jim Rasche
Version Date: 4-15-13

How to Use:

**Designed for IPad, it may work for Iphone, but it hasn't been tested

Hold Ipad with screen facing user.  Upon startup, some different buttons are
visible.  One set of buttons contains 5 green buttons (the set closer to the
middle of the screen).  Pressing one of these buttons will load a random
sample from 1 of 5 types of sounds.  Each of these 5 sounds has 9 different
pitches, so loading a random sample loads a random pitch.  After selecting
a sample, tilt the screen away from the user until the screen is facing away
from the user and hold in that position.  At this point, the selected sample
will begin to play, and an image will appear on the screen.  The sound will play
and the image will be visible until the user returns the Ipad to the starting 
orientation.


Notes:

**We will be modifying this code slightly before class on 4/16, but functionality
is complete

(1)
There are 5 buttons located along the bottom of the screen, labeled "scary sound 1",
"scary sound 2", "scary sound 3", etc.  These buttons currently have no function.
Their functionality was removed, but we left the buttons in case we use them for
image selection.  If we decide to use only one image before the performance, the
buttons will be removed.

(2)
Some of the sounds start quietly and have a long slow attack.  It may appear
that they are not playing, but they are.  You may have to wait a few 
seconds before they are audible.

(3)
I do not have the "scary" picture we are going to use for the performance and haven't
heard back from anyone who could send it to me.  In it's place is a color gradient
image, which will be replaced before the performance.


]]--

FreeAllFlowboxes()
FreeAllRegions()

DPrint("")

state = 0
red = 255
blue = 255
green = 255
sound = 0
display = 1

dac = FBDac
accel = FBAccel
sample = FlowBox(FBSample)
push_loop = FlowBox(FBPush)
push = FlowBox(FBPush)
push_samp = FlowBox(FBPush)

sunrise_cnt = 0
oxygen_cnt = 0
mercury_cnt = 0
smooth_cnt = 0
LAP_cnt = 0

sample:AddFile(SystemPath("LAP_Ab.wav"))
sample:AddFile(SystemPath("LAP_B.wav"))
sample:AddFile(SystemPath("LAP_Bb.wav"))
sample:AddFile(SystemPath("LAP_C.wav"))
sample:AddFile(SystemPath("LAP_Db.wav"))
sample:AddFile(SystemPath("LAP_Eb.wav"))
sample:AddFile(SystemPath("LAP_F.wav"))
sample:AddFile(SystemPath("LAP_G.wav"))
sample:AddFile(SystemPath("LAP_Gb.wav"))
sample:AddFile(SystemPath("Mercury_Ab.wav"))
sample:AddFile(SystemPath("Mercury_B.wav"))
sample:AddFile(SystemPath("Mercury_Bb.wav"))
sample:AddFile(SystemPath("Mercury_C.wav"))
sample:AddFile(SystemPath("Mercury_Db.wav"))
sample:AddFile(SystemPath("Mercury_Eb.wav"))
sample:AddFile(SystemPath("Mercury_F.wav"))
sample:AddFile(SystemPath("Mercury_G.wav"))
sample:AddFile(SystemPath("Mercury_Gb.wav"))
sample:AddFile(SystemPath("Oxygen_Ab.wav"))
sample:AddFile(SystemPath("Oxygen_B.wav"))
sample:AddFile(SystemPath("Oxygen_Bb.wav"))
sample:AddFile(SystemPath("Oxygen_C.wav"))
sample:AddFile(SystemPath("Oxygen_Db.wav"))
sample:AddFile(SystemPath("Oxygen_Eb.wav"))
sample:AddFile(SystemPath("Oxygen_F.wav"))
sample:AddFile(SystemPath("Oxygen_G.wav"))
sample:AddFile(SystemPath("Oxygen_Gb.wav"))
sample:AddFile(SystemPath("Smooth_Ab.wav"))
sample:AddFile(SystemPath("Smooth_B.wav"))
sample:AddFile(SystemPath("Smooth_Bb.wav"))
sample:AddFile(SystemPath("Smooth_C.wav"))
sample:AddFile(SystemPath("Smooth_Db.wav"))
sample:AddFile(SystemPath("Smooth_Eb.wav"))
sample:AddFile(SystemPath("Smooth_F.wav"))
sample:AddFile(SystemPath("Smooth_G.wav"))
sample:AddFile(SystemPath("Smooth_Gb.wav"))
sample:AddFile(SystemPath("Sunrise_Ab.wav"))
sample:AddFile(SystemPath("Sunrise_B.wav"))
sample:AddFile(SystemPath("Sunrise_Bb.wav"))
sample:AddFile(SystemPath("Sunrise_C.wav"))
sample:AddFile(SystemPath("Sunrise_Db.wav"))
sample:AddFile(SystemPath("Sunrise_Eb.wav"))
sample:AddFile(SystemPath("Sunrise_F.wav"))
sample:AddFile(SystemPath("Sunrise_G.wav"))
sample:AddFile(SystemPath("Sunrise_Gb.wav"))

dac.In:SetPull(sample.Out)
push_loop.Out:SetPush(sample.Loop)    	-- set it to non looping
push_loop:Push(-1)   
push.Out:SetPush(sample.Pos)
push:Push(1)
push_samp.Out:SetPush(sample.Sample)

r = Region()
r.t = r:Texture()
r:SetHeight(ScreenHeight())
r:SetWidth(ScreenWidth())
r2 = Region()
r2.t = r2:Texture()
r2:SetHeight(ScreenHeight())
r2:SetWidth(ScreenWidth())
r2.t:SetTexture(0,0,0,255)

function setText(r, text)
    r.tl = r:TextLabel()
    r.tl:SetLabel(text)
	r.tl:SetColor(100,100,100,255)
end

-- swipe to change between interface that illuminates face and other page that acts as a sound board for scary sound clips
function swipe(self, x, y, dx, dy)
    if (dx > 50 or dy > 50) and self == r then
		state= 1
		r2:Show()
		r:Hide()
		r2:Handle("OnMove",swipe)
		r2:EnableInput(true)
		r:EnableInput(false)
		for i=1,9 do
			btnArray[i]:EnableInput(false)
			btnArray[i]:Hide()
		end
	elseif (dx < -50 or dy < -50) and self == r2 then
		state = 0
		r2:Hide()
		r:Show()
		r:Handle("OnMove",swipe)
		r:EnableInput(true)
		r2:EnableInput(false)
		for i=1,9 do
			btnArray[i]:EnableInput(true)
			btnArray[i]:Show()
		end
	end
end

function setGlow(self)
	if self.index == 1 then
		display = 1
	elseif self.index == 2 then
		display = 2
    elseif self.index == 3 then
		display = 3
	elseif self.index == 4 then
		display = 4
	elseif self.index == 5 then
		sound = (math.random(0,8))/44
		push_samp:Push(sound)
	elseif self.index == 6 then
		sound = (9+math.random(0,8))/44
		push_samp:Push(sound)
	elseif self.index == 7 then
		sound = (18+math.random(0,8))/44
		push_samp:Push(sound)
	elseif self.index == 8 then
		sound = (27+math.random(0,8))/44
		push_samp:Push(sound)
	else
		sound = (36+math.random(0,8))/44
		push_samp:Push(sound)
	end
end

-- if we have the screen close to us the screen is dark
-- if we tilt it horizontal then it illuminates
-- if we flip it away from us, a pre-determined image shows up
function brighter(self,x,y,z)
	if (y > .6) then
		DPrint("")
		if not playing then 
			self.t:SetTexture(255,255,255,255)
			self.t:SetTexture(SystemPath("mask.png"))
			self.t:SetRotation(math.pi)
			playsound()
		end
		for i=1,9 do
			btnArray[i]:Hide()
		end
		playing = true
	else
		playing = false
		self.t:SetTexture(0,0,0,255)
		for i=1,9 do
			btnArray[i]:Show()
		end
		stopsound()
	end
end

function playsound()
	push:Push(-1)
end

function stopsound()
	push:Push(1)
end


btnArray = {}

for i=1,9 do
	btnArray[i] = Region()
	btnArray[i].t = btnArray[i]:Texture()
	btnArray[i]:Show()
	btnArray[i]:Handle("OnTouchUp",setGlow)
	btnArray[i]:Handle("OnLeave",setGlow)
	btnArray[i]:EnableInput(true)
	btnArray[i].index = i
end

--These image loads will be changed before the performance
btnArray[1].image = SystemPath("mask.png")  
btnArray[2].image = SystemPath("demonclownmask.png")
btnArray[3].image = SystemPath("saw.png")
btnArray[4].image = SystemPath("saw.png")

btnArray[1].t:SetTexture(50,0,0,255)
btnArray[2].t:SetTexture(0,0,50,255)
btnArray[3].t:SetTexture(0,50,0,255)
btnArray[4].t:SetTexture(50,50,0,255)
btnArray[5].t:SetTexture(0,50,0,255)
btnArray[6].t:SetTexture(0,50,0,255)
btnArray[7].t:SetTexture(0,50,0,255)
btnArray[8].t:SetTexture(0,50,0,255)
btnArray[9].t:SetTexture(0,50,0,255)
btnArray[1]:SetAnchor("BOTTOMLEFT",0,0)
btnArray[2]:SetAnchor("BOTTOMLEFT",ScreenWidth()/4,0)
btnArray[3]:SetAnchor("BOTTOMLEFT",ScreenWidth()/2,0)
btnArray[4]:SetAnchor("BOTTOMLEFT",3*ScreenWidth()/4,0)
btnArray[5]:SetAnchor("BOTTOMLEFT",0+15,ScreenHeight()/4)
btnArray[6]:SetAnchor("BOTTOMLEFT",ScreenWidth()/5 +15,ScreenHeight()/4)
btnArray[7]:SetAnchor("BOTTOMLEFT",2*ScreenWidth()/5+15,ScreenHeight()/4)
btnArray[8]:SetAnchor("BOTTOMLEFT",3*ScreenWidth()/5+15,ScreenHeight()/4)
btnArray[9]:SetAnchor("BOTTOMLEFT",4*ScreenWidth()/5+15,ScreenHeight()/4)

for i=5,9 do
	btnArray[i]:SetWidth(ScreenWidth()/6)
end

-- each button will lead to a different sound being played, ATM each button press changes the color of the illumination screen
-- this is for testing purposes without sound
-- we have a lot of creative license with this framework, in terms of what we want to do in this performace.
--For example, rather than playing a sound we can display a scary image. The options are limitless
setText(btnArray[1],"scary sound 1")   
setText(btnArray[2],"scary sound 2")		-- sunrise, oxygen, mercury, lap
setText(btnArray[3],"scary sound 3")
setText(btnArray[4],"scary sound 4")
setText(btnArray[5],"LAP")   
setText(btnArray[6],"mercury")
setText(btnArray[7],"oxygen")
setText(btnArray[8],"smooth")
setText(btnArray[9],"sunrise")

r.t:SetTexture(0,0,0,255)
r:Show()

-- the dark screen will transition into illumination when put in horizontal position
r:Handle("OnAccelerate", brighter)

r:EnableInput(true)

local pagebutton=Region('region', 'pagebutton', UIParent)
pagebutton:SetWidth(pagersize)
pagebutton:SetHeight(pagersize)
pagebutton:SetLayer("TOOLTIP")
pagebutton:SetAnchor('BOTTOMLEFT',ScreenWidth()-pagersize-4,ScreenHeight()-pagersize-4)
pagebutton:EnableClamping(true)
pagebutton:Handle("OnTouchDown", MyFlipPage)
pagebutton.texture = pagebutton:Texture("circlebutton-16.png")
pagebutton.texture:SetGradientColor("TOP",255,255,255,255,255,255,255,255)
pagebutton.texture:SetGradientColor("BOTTOM",255,255,255,255,255,255,255,255)
pagebutton.texture:SetBlendMode("BLEND")
pagebutton.texture:SetTexCoord(0,1.0,0,1.0)
pagebutton:EnableInput(true)
pagebutton:Show()

