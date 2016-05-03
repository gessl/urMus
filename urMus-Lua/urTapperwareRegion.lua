-- =================
-- = region object =
-- =================

-- rewrite of the urVen region code as OO Lua

-- using the prototype facility in lua to do OO, a bit painful yes:
	
	
	
-- ===================
-- = Region Creation =
-- ===================

-- constructor
function Region:new (o)
   o = o or {}   -- create object if user does not provide one
   setmetatable(o, self)
   self.__index = self
   return o
end

function CreateorRecycleregion(ftype, name, parent)
    local region
    if #recycledregions > 0 then
        region = regions[recycledregions[#recycledregions]]
        table.remove(recycledregions)
        region:EnableMoving(true)
        region:EnableResizing(true)
        region:EnableInput(true)
        region.usable = 1
				region.t:SetTexture("tw_roundrec.png")	-- reset texture
    else
        region = VRegion(ftype, name, parent, #regions+1)
        table.insert(regions,region)
    end
    region:SetAlpha(0)
		region.shadow:SetAlpha(0)
    region:MoveToTop()
    return region
end

-- initialize a new
function VRegion(ttype,name,parent,id) -- customized initialization of region
	
	-- add a visual shadow as a second layer	
	local r_s = Region(ttype,"drops"..id,parent)
	r_s.t = r_s:Texture("tw_shadow.png")
	r_s.t:SetBlendMode("BLEND")
  r_s:SetWidth(INITSIZE+70)
  r_s:SetHeight(INITSIZE+70)
	-- r_s:EnableMoving(true)
	r_s:SetLayer("LOW")
	r_s:Show()

  local r = Region(ttype,"Region "..id,parent)
  r.tl = r:TextLabel()
  r.t = r:Texture("tw_roundrec.png")
	r:SetLayer("LOW")
	r.shadow = r_s
	r.shadow:SetAnchor("CENTER",r,"CENTER",0,0) 
  -- initialize for regions{} and recycledregions{}
  r.usable = 1
  r.id = id
  PlainVRegion(r)

  r:EnableMoving(true)
  r:EnableResizing(true)
  r:EnableInput(true)

  r:Handle("OnDoubleTap",VDoubleTap)
  r:Handle("OnTouchDown",VTouchDown)
  r:Handle("OnTouchUp",VTouchUp)
  r:Handle("OnDragStop",VDrag)
	r:Handle("OnUpdate",VUpdate)

  return r
end

-- reset region to initial state
function PlainVRegion(r) -- customized parameter initialization of region, events are initialized in VRegion()
		r.alpha = 1	--target alpha for animation
		r.menu = nil	--contextual menu
		r.counter = 0	--if this is a counter
		r.isHeld = false -- if the r is held by tap currently

		-- event handling
		r.links = {}	
    r.links["OnTouchDown"] = {}
		r.links["OnTouchUp"] = {}
    r.links["OnDoubleTap"] = {}
	
    -- initialize for events and signals
    r.eventlist = {}
    r.eventlist["OnTouchDown"] = {HoldTrigger}
    r.eventlist["OnTouchUp"] = {DeTrigger} 
    r.eventlist["OnDoubleTap"] = {} --{CloseSharedStuff,OpenOrCloseKeyboard} 
    r.eventlist["OnUpdate"] = {} 
    r.eventlist["OnUpdate"].currentevent = nil
 
		r.t:SetBlendMode("BLEND")
    r.tl:SetLabel(r:Name())
    r.tl:SetFontHeight(16)
		r.tl:SetFont("AvenirNext-Medium.ttf")
    r.tl:SetColor(0,0,0,255) 
    r.tl:SetHorizontalAlign("JUSTIFY")
    r.tl:SetVerticalAlign("MIDDLE")
    r.tl:SetShadowColor(100,100,100,255)
    r.tl:SetShadowOffset(1,1)
    r.tl:SetShadowBlur(1)
    r:SetWidth(INITSIZE)
    r:SetHeight(INITSIZE)

end
