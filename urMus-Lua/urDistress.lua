--[[

d(ie)stress signal by Cameron Hejazi and Jack Byrne

We're stuck in the function "startPlay" where we can't get
the Range Flowbox (called global var "range") to map accel.Y 
[-1,1] to our [Bottom,Top] range. We want to use this mapping 
as the Rate input for our file sample played back. 

To play a sample, put a finger on each of ON_1 and ON_2 and
press either the blue or red to start/stop the sample

]]--

FreeAllRegions()
FreeAllFlowboxes()

left = false
right = false

dac = FBDac
accel = FBAccel
push = FlowBox(FBPush)
range = FlowBox(FBRange)

distressStatus = 0
destressStatus = 0

function generateAttributes(region)
	region.t = region:Texture()
	region.label = region:TextLabel()
	region.label:SetColor(0,0,0,255)
	region.label:SetFontHeight(34)
	region:EnableInput(true)
	region:Show()
end

function printStatus()
	
	if left == true and right == true then
		DPrint("ON")
	else
		DPrint("OFF")
	end

end

function enableL()
	
	left = true
	printStatus()
end

function disableL()

	left = false
	printStatus()
end

function enableR()

	right = true
	printStatus()
end

function disableR()

	right = false
	printStatus()
end



function endPlay()
	dac.In:RemovePull(sample.Out)
	push.Out:RemovePush(sample.Loop)
end

function startPlay(filename)
	sample = FlowBox(FBSample)
	
	range = FlowBox(FBRange)
    push_r = FlowBox(FBPush)
    push_r.Out:SetPush(range.Bottom)
    push_r:Push(.15)
    push_r.Out:RemovePush(range.Bottom)
    push_r.Out:SetPush(range.Top)
    push_r:Push(.35)
    push_r.Out:RemovePush(range.Top)
--	range.In:SetPull(accel.Y)
    accel.Y:SetPush(range.In)
	range.Out:SetPush(sample.Rate)
	
	sample:AddFile(SystemPath(filename..".wav"))
	push.Out:SetPush(sample.Loop)
	push:Push(-1)
	dac.In:SetPull(sample.Out)
end
	
function setDistressRun(self)
	if left == true and right == true and destressStatus ~= 1 then
		if distressStatus == 1 then
			endPlay()
			distressStatus = 0
			self.label:SetLabel("PLAY")
			DPrint("eOFF")
		else
			startPlay("distress")
			distressStatus = 1
			self.label:SetLabel("STOP")
			DPrint("eON")
		end
	end
end

function setDestressRun(self)
	if left == true and right == true and distressStatus ~= 1 then
		if destressStatus == 1 then
			endPlay()
			destressStatus = 0
			self.label:SetLabel("PLAY")
			DPrint("iOFF")
			
		else
			
			startPlay("destress")
			destressStatus = 1
			self.label:SetLabel("STOP")
			DPrint("iON")
			
		end
	end
end
	
	
signalL = Region()
generateAttributes(signalL)
signalL:SetWidth(ScreenWidth() / 2)
signalL:SetHeight(ScreenHeight()/2)
signalL:SetAnchor("BOTTOMLEFT", 0, 0)
signalL.t:SetTexture(0,100,0,255)
signalL.label:SetLabel("ON_1")
signalL:Handle("OnTouchDown", enableL)
signalL:Handle("OnTouchUp", disableL)

signalR = Region()
generateAttributes(signalR)
signalR:SetWidth(ScreenWidth() / 2)
signalR:SetHeight(ScreenHeight()/2)
signalR:SetAnchor("BOTTOMLEFT", 0, ScreenHeight() / 2)
signalR.t:SetTexture(0,100,0,255)
signalR.label:SetLabel("ON_2")
signalR:Handle("OnTouchDown", enableR)
signalR:Handle("OnTouchUp", disableR)


distress = Region()
generateAttributes(distress)
distress:SetWidth(ScreenWidth() / 2)
distress:SetHeight(ScreenHeight()/2)
distress:SetAnchor("BOTTOMLEFT", ScreenWidth() / 2, ScreenHeight() / 2)
distress.t:SetTexture(0,0,100,255)
distress.label:SetLabel("Play")
--distress:Handle("OnTouchDown", )
distress:Handle("OnTouchUp", setDistressRun)


destress = Region()
generateAttributes(destress)
destress:SetWidth(ScreenWidth() / 2)
destress:SetHeight(ScreenHeight() / 2)
destress:SetAnchor("BOTTOMLEFT", ScreenWidth() / 2, 0)
destress.t:SetTexture(100,0,0,255)
destress.label:SetLabel("Play")
--distress:Handle("OnTouchDown", enableR)
destress:Handle("OnTouchUp", setDestressRun)

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
