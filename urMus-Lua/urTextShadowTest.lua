-- blank file to do testing
FreeAllRegions()

DPrint("")

bg = Region()
bg:SetHeight(ScreenHeight())
bg:SetWidth(ScreenWidth())
bg:Show()
bg.t = bg:Texture(00,70,40,255)

function makeFontRegion(font, label, size, topparent)
    local r
    r = Region()
    r:SetWidth(ScreenWidth())
    r:SetHeight((size or 32) + 8)
    r.tl = r:TextLabel()
    r.tl:SetHorizontalAlign("CENTER")
--    r.tl:SetOutline(1,3.5)
    r.tl:SetFont(font)
    r.tl:SetLabel(label)
    r.tl:SetFontHeight(size or 32)
    r.tl:SetShadowColor(0,0,0,255)
    r.tl:SetShadowBlur(0.05*40)
    r.tl:SetShadowOffset(10,-10)
    r:Show()
    if topparent then
        r:SetAnchor("TOPLEFT", topparent, "BOTTOMLEFT", 0,0)
    else
        r:SetAnchor("TOPLEFT", UIParent, "TOPLEFT", 0,0)
    end
    return r
end


function filesByExtension(scrollentries, path, ext)
    local findstr = "%"..ext.."$"
    for v in lfs.dir(path) do
        if v ~= "." and v ~= ".." and string.find(v,ext) then
            table.insert(scrollentries, v)  
        end
    end
end

fonts = {}
filesByExtension(fonts, DocumentPath(""),".ttf")
filesByExtension(fonts, DocumentPath(""),".otf")

fontr = {}
local parent = nil
local fontheight = 32

for k,v in pairs(fonts) do
    local newr = makeFontRegion(v,"|cffff00ffurMus font:|r ".. v, fontheight,parent)
    table.insert(fontr,newr)
    parent = newr
end

local mode = 0

function moved(self,x,y,dx,dy)
    --    r.tl:SetShadowBlur(x/ScreenWidth()*4)
    for k,v in pairs(fontr) do
        if mode == 0 then
            v.tl:SetShadowOffset(-(x/ScreenWidth()-0.5)*fontheight,-(y/ScreenHeight()-0.5)*fontheight)
        else
            v.tl:SetShadowBlur(y/ScreenHeight()*120)
--            v.tl:SetShadowColor(0,0,0,x/ScreenWidth()*255)
--            DPrint("Blur: "..y/ScreenHeight())  
            v.tl:SetOutline(math.floor(x/ScreenWidth()*4),1)            
        end            
    end
end


function toggle(self)
    mode = 1 - mode
end

bg:Handle("OnMove",moved)
bg:Handle("OnDoubleTap", toggle)
bg:EnableInput(true)

local mypagerbutton = CreatePagerButton()
