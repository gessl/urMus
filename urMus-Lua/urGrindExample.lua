FreeAllRegions()
FreeAllFlowboxes()

local useplucked = true

local dac = FBDac
local mic = FBMic
local grinder = FlowBox(FBGrinder)
local plucked = FlowBox(FBPlucked)
local sample = FlowBox(FBSample)
local square = FlowBox(FBSQ)
sample:AddFile("Clap.wav")

mic.Out:SetPush(grinder.In)

if useplucked then
dac.In:SetPull(plucked.Out)
grinder.Out:SetPush(plucked.Trigger)
grinder.ZeroX:SetPush(plucked.Freq)
--mic.Out:SetPush(plucked.Trigger)
		
else

dac.In:SetPull(sample.Out)
grinder.Out:SetPush(square.In)
grinder.Out:SetPush(sample.Amp)
square.Out:SetPush(sample.Pos)
grinder.ZeroX:SetPush(sample.Rate)

end