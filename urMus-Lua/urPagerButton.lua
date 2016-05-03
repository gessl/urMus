-- urPagerButton.lua
-- Created: Long time ago
-- Modularized: 9/23/2013
-- By Georg Essl

-- Default FlipPage destroys any patches you created, override this if that is not your preferred behavior
local function MyFlipPage(self)
    FreeAllFlowboxes()
    FlipPage()
end

-- Use: mypager = CreatePagerButton(), arguments is optional for custom page flipper function, and add doubletap functionality
-- set newMyFlipPage to nil if you only want doubletap
function CreatePagerButton(newMyFlipPage, myDoubleTap)
	pagebutton=Region()
	pagebutton:SetWidth(pagersize)
	pagebutton:SetHeight(pagersize)
	pagebutton:SetLayer("TOOLTIP")
	pagebutton:SetAnchor('BOTTOMLEFT',ScreenWidth()-pagersize-4,ScreenHeight()-pagersize-4)
	pagebutton:EnableClamping(true)
	if not (not newMyFlipPage and myDoubleTap) then
		pagebutton:Handle("OnTouchDown", newMyFlipPage or MyFlipPage)
	end
	if myDoubleTap then
		pagebutton:Handle("OnDoubleTap", myDoubleTap)
	end
	pagebutton.texture = pagebutton:Texture("circlebutton-16.png")
	pagebutton.texture:SetGradientColor("TOP",255,255,255,255,255,255,255,255)
	pagebutton.texture:SetGradientColor("BOTTOM",255,255,255,255,255,255,255,255)
	pagebutton.texture:SetBlendMode("BLEND")
	pagebutton.texture:SetTexCoord(0,1.0,0,1.0)
	pagebutton:EnableInput(true)
	pagebutton:Show()
	return pagebutton
end
