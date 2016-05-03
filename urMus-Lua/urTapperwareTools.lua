-- ========================
-- = Misc Tools & Helpers =
-- ========================

-- table deletion of specific object
function tableRemoveObj( t, obj )
	for i = 1, #t do
		if t[i] == obj then
			table.remove(t, i)
		end
	end
end

-- check if table contains obj
function tableHasObj( t, obj )
	for i = 1, #t do
		if t[i] == obj then
			return true
		end
	end
	return false
end