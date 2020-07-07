-- output union curve file for further anaylzing

preamble_ref_id = "5D2F1BCC7C75000054005072"

function get_collection(distance, suffix)
	return "ArchiveTest_20190716_" .. distance .. "m_" .. suffix
end

function get_ids(distance, frequency, suffix)
	collection = get_collection(distance, suffix)
	return list_all_documents_id(collection, [[{"data":{"$exists": true}, "removed":{"$exists": false}, "frequency":]] .. frequency .. [[,"distance":]] .. distance .. [[}]])
end

distances = {1, 2, 3, 4, 5}
frequencys = {2000}
suffixs = {"multi", "single"}

for i=1,#distances do
-- for i=1,1 do
	for j=1,#frequencys do
	-- for j=1,1 do
		for k=1,#suffixs do
		-- for k=1,1 do
			distance = distances[i]
			frequency = frequencys[j]
			suffix = suffixs[k]
			logln("distance: " .. distance)
			logln("frequency: " .. frequency)
			logln("suffix: " .. suffix)
			ids = get_ids(distance, frequency, suffix)
			logln("has " .. #ids .. " logs")
			for l=1,#ids do
			-- for l=1,1 do
				if (l%10 == 0) then logln("[" .. l .. "/" .. #ids .. "]") end
				id = ids[l]
				collection = get_collection(distance, suffix)
				run("Tester/Archive/AR_Output_Preamble_UnionCurve", preamble_ref_id, collection, id, "" .. distance .. "m_", "." .. suffix)
			end
		end
	end
end
