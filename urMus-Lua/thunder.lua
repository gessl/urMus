FreeAllFlowboxes()
FreeAllRegions()

DPrint("")

--Networking stuff Change host2 to device with images
local host2 = "67.194.112.234" 
function SendUp(self)
    SendOSCMessage(host2,8888,"/urMus/numbers",0.0)
end

function SendDown(self)
    SendOSCMessage(host2,8888,"/urMus/numbers",1.0)
end


--Set up flowboxes
dac = FBDac
sample = FlowBox(FBSample)
push_loop = FlowBox(FBPush)
push_pos = FlowBox(FBPush)
push_sound = FlowBox(FBPush)

--sample is an table of sound files
--order of sounds in sample is the same as order of images in self
sample:AddFile(SystemPath("stormthunder copy.wav"))

dac.In:SetPull(sample.Out)
push_loop.Out:SetPush(sample.Loop)
push_loop:Push(-1) --set looping off
push_pos.Out:SetPush(sample.Pos)
push_sound.Out:SetPush(sample.Sample)

push_pos:Push(1) --hack: don't play sound at start of program

--play sound file associated with the image corresponding to self.index
function playsound(self)
	push_pos:Push(0)
	push_sound:Push(0)
end

function Storm(self)
	self.t:SetSolidColor(255,255,255,25)
	playsound(self)
	SendDown(self)
	--send networking signal (thunder on)
end

function StormOff(self)
	self.t:SetSolidColor(10,10,10,255)
	SendUp(self)
	--stop playing sound clip
	--send networking signal (thunder off)
end

r = Region()
r.t = r:Texture()
r.t:SetSolidColor(10,10,10,255)
r:SetWidth(ScreenWidth())
r:SetHeight(ScreenHeight())
r:Show()
r:Handle("OnTouchDown", Storm)
r:Handle("OnTouchUp", StormOff)
r:EnableInput(true)


