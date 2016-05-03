-- ===============================
-- = Visual Links between regions=
-- ===============================

-- methods and appearances and menus for deleting / editing
-- assumes urTapperwareMenu.lua is already processed

linkLayer = {}

function linkLayer:Init()
	self.list = {}
	-- this is actually a dictionary, key = sender, value = {list of receivers}
	self.potentialLinkList = {}
	self.menus = {}
	
	-- one region for drawing formed links
	self.links = Region('region', 'backdrop', UIParent)
	self.links:SetWidth(ScreenWidth())
	self.links:SetHeight(ScreenHeight())
	self.links:SetLayer("TOOLTIP")
	self.links:SetAnchor('BOTTOMLEFT',0,0)
	self.links.t = self.links:Texture()
	self.links.t:Clear(0,0,0,0)
	self.links.t:SetTexCoord(0,ScreenWidth()/1024.0,1.0,0.0)
	self.links.t:SetBlendMode("BLEND")
	self.links:EnableInput(false)
	self.links:EnableMoving(false)
	
	self.links:MoveToTop()
	self.links:Show()
	
	-- set up another region for drawing guides or potential links
	self.linkGuides = Region('region', 'backdrop', UIParent)
	self.linkGuides:SetWidth(ScreenWidth())
	self.linkGuides:SetHeight(ScreenHeight())
	self.linkGuides:SetLayer("TOOLTIP")
	self.linkGuides:SetAnchor('BOTTOMLEFT',0,0)
	self.linkGuides.t = self.linkGuides:Texture()
	self.linkGuides.t:Clear(0,0,0,0)
	self.linkGuides.t:SetTexCoord(0,ScreenWidth()/1024.0,1.0,0.0)
	self.linkGuides.t:SetBlendMode("BLEND")
	self.linkGuides:EnableInput(false)
	self.linkGuides:EnableMoving(false)
	
	self.linkGuides:MoveToTop()
	self.linkGuides:Show()
	
end

-- add links to our list
function linkLayer:Add(r1, r2)
	if self.list[r1] == nil then
		self.list[r1] = {r2}
	else
		table.insert(self.list[r1], r2)
	end
	local	menu = newLinkMenu(r1, r2)
	table.insert(self.menus, menu)
end

-- remove links
function linkLayer:Remove(r1, r2)	
	if self.list[r1] ~= nil then
		
		for i = 1, #self.list[r1] do
			if self.list[r1][i] == r2 then
				table.remove(	self.list[r1], i)
			end
		end
	end
	
	for i,menu in ipairs(self.menus) do
		if menu.sender == r1 and menu.receiver == r2 then
			table.remove(self.menus, i)
			deleteLinkMenu(menu)
		end
	end
end

-- draw a line between linked regions, also draws menu
function linkLayer:Draw()
	self.links.t:Clear(0,0,0,0)
	self.links.t:SetBrushColor(100,255,240,200)
	self.links.t:SetBrushSize(8)
	
	for sender, receivers in pairs(self.list) do
		X1, Y1 = sender:Center()		
		for _, r in ipairs(receivers) do
			
			X2, Y2 = r:Center()
			self.links.t:Line(X1,Y1,X2,Y2)			
		end
	end

	-- draw the link menu (close button), it will compute centroid using
	-- region locations	
	for _,menu in ipairs(self.menus) do
		OpenLinkMenu(menu)
	end	
end

function linkLayer:DrawPotentialLink(region, draglet)
	self.linkGuides.t:Clear(0,0,0,0)
	self.linkGuides.t:SetBrushColor(100,255,240,100)
	self.linkGuides.t:SetBrushSize(12)
	
	rx, ry = region:Center()
	posx, posy = draglet:Center()
	self.linkGuides.t:Line(rx,ry,posx,posy)
end

function linkLayer:ResetPotentialLink()
	self.linkGuides.t:Clear(0,0,0,0)
end

function linkLayer:SendMessageToReceivers(sender, message)
	for _, r in pairs(self.list[sender]) do
		-- sender:message
		
	end
end
