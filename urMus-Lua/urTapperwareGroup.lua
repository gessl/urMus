-- ==============================================
-- = Group Regions and menus for group commands =
-- ==============================================

BUTTONSIZE = 54	-- on screen size in points/pixels
BUTTONOFFSET = 3
BUTTONIMAGESIZE = 80		-- size of the square icon image

recycledGroupMenu = {}

function ToggleLockGroup(regions)
	-- TODO change later: now just create a group, we can't delete yet
	newGroup = Group:new()
	newGroup:SetRegions(regions)
	newGroup:Draw()
end

function initGroupMenu()
	local groupMenu = {}
	-- label, func, anchor relative to region, image file, draggable or not
	-- groupMenu.cmdList = {
	-- 	{"", ToggleLockGroup, "tw_unlock.png"}
	-- }

	local r = Region('region','menu',UIParent)
	r.t = r:Texture("tw_unlock.png")	--TODO change this later
	r.t:SetTexCoord(0,BUTTONIMAGESIZE/128,BUTTONIMAGESIZE/128,0)
	r.t:SetBlendMode("BLEND")
	r:SetLayer("TOOLTIP")
	r:SetHeight(BUTTONSIZE)
	r:SetWidth(BUTTONSIZE)
	r:MoveToTop()
	r:Hide()
	r.parent = groupMenu
	r.func = ToggleLockGroup
	
	groupMenu.item = r
	groupMenu.selectedRegions = {}
	
	return groupMenu
end

function CallGroupFunc(self)
	-- use this to call function on the link menu button
	-- because we have reference to the link, not just one region
	CloseGroupMenu(self.parent)
	self.func(self.parent.selectedRegions)
end


-- ============================
-- = public link menu methods =
-- ============================
function newGroupMenu()
	-- recycling constructor
	local groupMenu
	if # recycledGroupMenu > 0 then
		groupMenu = table.remove(recycledGroupMenu, 1)
	else
		groupMenu = initGroupMenu()
	end
	
	return groupMenu
end
	
function OpenGroupMenu(menu, x, y, selectedRegions)
	-- shows the actual menu
	menu.item:Show()
  menu.item:EnableInput(true)
  menu.item:Handle("OnTouchUp", CallGroupFunc)
  menu.item:MoveToTop()
	menu.selectedRegions = selectedRegions
	menu.item:SetAnchor("CENTER", x, y)	   
end
	
function deleteGroupMenu(menu)
	menu.selectedRegions = nil
	CloseGroupMenu(menu)
	table.insert(recycledLinkMenu, menu)
end

-- ==============================
-- = private group menu methods =
-- ==============================

function CloseGroupMenu(self)
	self.item:Hide()
	self.item:EnableInput(false)
end


-- ======================
-- = Grouping code here =
-- ======================

Group = {}	-- the class
groups = {}	-- the list of groups

function Group:new(o)
	o = o or {}   -- create object if user does not provide one
	setmetatable(o, self)
	self.__index = self
	 
	o.menu = newGroupMenu()
	o.r = Region('region', 'backdrop', UIParent)
	o.r.t = o.r:Texture()
	o.r.t:Clear(0,0,0,100)
	o.r.t:SetBlendMode("BLEND")
	o.r:EnableInput(false)
	o.r:EnableMoving(false)
	o.r:Hide()
	o.regions = {}

	table.insert(groups, o)
	return o
end

function Group:SetRegions(listOfRegions)
	self.regions = listOfRegions
	
	-- assign groups to each region in the list
	for i = 1, #self.regions do
		self.regions[i].group = self
	end
end

function Group:Draw()
	-- find out how big the group region needs to be, then resize and draw
	minX = -1
	minY = -1
	maxX = -1
	maxY = -1
	
	for i = 1, #self.regions do
		x,y = self.regions[i]:Center()
		w = self.regions[i]:Width()
		h = self.regions[i]:Height()
		
		if minX < 0 or minX > x - w/2 then
			minX = x - w/2
		end
		if minY < 0 or minY > y - h/2 then
			minY = y - h/2
		end
		maxX = math.max(maxX, x + w/2)
		maxY = math.max(maxY, y + h/2)
	end
	-- self.r.t:Rect(minX, minY, maxX - minX, maxY - minY)
	self.r:SetWidth(maxX - minX)
	self.r:SetHeight(maxY - minY)	
	self.r:SetAnchor('CENTER', (maxX+minX)/2, (maxY+minY)/2)
	-- now show everything
	self.r:Show()
	OpenGroupMenu(self.menu, minX, minY, nil) -- TODO: self.regions functionality
	self.menu.item:SetAnchor('CENTER', self.r, 'BOTTOMLEFT', 0, 0)
	self.menu.item:EnableInput(false)
end

function Group:Hide()
	self.r:Hide()
end

function Group:Destroy()
	--// not sure if this is needed or actually works yet //--
	self:Hide()
	for i = 1, #self.regions do
		self.regions[i].group = nil
	end
	
	self.regions = nil
	self = nil
end

lassoGroupMenu = newGroupMenu()
