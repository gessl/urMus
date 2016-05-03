-----------------------------------------------------------
--                    Moving Menu                        --
-----------------------------------------------------------


hitZone = Region('region','dialog',UIParent)
hitZone.t = hitZone:Texture(255,255,255,255)
hitZone:SetWidth(ScreenWidth())
hitZone:SetHeight(100)
hitZone:SetAnchor("BOTTOM",UIParent,"CENTER",0,ScreenHeight()/2)
hitZone:Show()


​