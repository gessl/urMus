function CreateInstructionText(title, text)
	local r = Region()
	r:SetAnchor("CENTER",UIParent, "CENTER",0,-52)
	r:SetWidth(ScreenWidth())
	r:SetHeight(128)
	r.tl = r:TextLabel()
	r.tl:SetFontHeight(24)
	r.tl:SetLabel("|cffff00ff"..title.."|r\n|cffffffff"..text.."|r")
	r:Show()
end

