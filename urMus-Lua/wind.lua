FreeAllRegions()
FreeAllFlowboxes()
volume = 0
zOld = 0

DPrint("")

--Set up flowboxes
dac = FBDac
sample = FlowBox(FBSample)
push_loop = FlowBox(FBPush)
push_pos = FlowBox(FBPush)
push_sound = FlowBox(FBPush)

push_amp = FlowBox(FBPush)

--sample is an table of sound files
--order of sounds in sample is the same as order of images in self
--sample:AddFile(SystemPath("stormthunder.wav"))
sample:AddFile(SystemPath("wind.wav"))




dac.In:SetPull(sample.Out)
push_loop.Out:SetPush(sample.Loop)
push_loop:Push(1) --set looping off
push_pos.Out:SetPush(sample.Pos)
push_sound.Out:SetPush(sample.Sample)

push_amp.Out:SetPush(sample.Amp)


push_pos:Push(1) --hack: don't play sound at start of program

push_pos:Push(0)
push_sound:Push(0)
push_amp:Push(1)

--play sound file associated with the image corresponding to self.index
function playsound(self)
	push_pos:Push(0)
	push_sound:Push(0)
	push_amp:Push(volume/100)
end


function GetVolume(self, x, y, z)
--[[
	dif = math.abs(z-zOld)
	volume = 100*dif
--	DPrint(volume)
    if(volume<1) then volume = 0 end
	zOld = z
	push_amp:Push(volume/100)
    --]]
    if(math.abs(y)<0.05) then y = 0 end
    push_amp:Push(y)
end
 
function PlaySound(self)
--	play sound clip with volume = "volume"
	playsound(self)
--send networking signal wind = volume
end

r = Region()
r.t = r:Texture()
r.t:SetSolidColor(10,10,10,255)
r:SetWidth(ScreenWidth())
r:SetHeight(ScreenHeight())
r:Show()
r:Handle("OnAccelerate", GetVolume)
--r:Handle("OnUpdate", PlaySound)
r:EnableInput(true)

