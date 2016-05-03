-- urGUITextures.lua
-- Created 3/4/2014
-- by Georg Essl

dofile(SystemPath("urScrollList.lua"))


function SelectGUIConfig(d1,d2)
	SetPage(1)
	for i=1,3 do
		for k,v in pairs(protofbcells[i]) do
			if v and v.texture then
				v.texture:SetTexture(d2)
			end
		end
	end
	for k,v in pairs(rowregions) do
		for k2,v2 in pairs(v) do
			if v2.texture then
				v2.texture:SetTexture(d2)
			end
		end
	end
	texturefiles.button = d2
end

textures =	{ 
			"button-unpadded.png",
			"urMus-basic-circle.png",
            "urMus-basic-faderect.png",
            "urMus-basic-hexagon.png",
            "urMus-basic-pentagon.png",
            "urMus-basic-roundedsquare.png",
            "urMus-basic-roundedsquare2.png",
            "urMus-bevel-circle.png",
            "urMus-bevel-rect.png",
            "urMus-bevel-hexagon.png",
            "urMus-bevel-pentagon.png",
            "urMus-bevel-roundedsquare.png",
            "urMus-bevel-roundedsquare2.png",
			}

names = {
	"Default",
	"Flat Circle",
	"Flat Rectangle",
	"Flat Hexagon",
	"Flat Pentagon",
	"Flat Rounded Rect (1)",
	"Flat Rounded Rect (2)",
	"Raised Circle",
	"Raised Rectangle",
	"Raised Hexagon",
	"Raised Pentagon",
	"Raised Rounded Rect (1)",
	"Raised Rounded Rect (2)",
		}

	local scrollentries = {}

	for k,v in pairs(textures) do
		local entry = { names[k], nil, nil, SelectGUIConfig, {0,0,0,255},v,v}
		table.insert(scrollentries, entry)
	end


	urScrollList:OpenScrollListPage(scrollpage, "urMus GUI Button Texture", nil, nil, scrollentries)
