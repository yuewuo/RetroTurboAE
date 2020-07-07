
distances = {1, 2, 3, 4, 5}
frequencys = {2000}
suffixs = {"multi", "single"}

function get_collection(distance, suffix)
	return "ArchiveTest_20190716_" .. distance .. "m_" .. suffix
end

function get_ids(distance, frequency, suffix)
	collection = get_collection(distance, suffix)
	return list_all_documents_id(collection, [[{"BER":{"$exists": true}, "removed":{"$exists": false}, "frequency":]] .. frequency .. [[,"distance":]] .. distance .. [[}]])
end

for i_=1,#distances do
-- for i=1,1 do
	for j=1,#frequencys do
	-- for j=1,1 do
		for k=1,#suffixs do
		-- for k=1,1 do
			distance = distances[i_]
			frequency = frequencys[j]
			suffix = suffixs[k]
			-- logln("distance: " .. distance)
			-- logln("frequency: " .. frequency)
			-- logln("suffix: " .. suffix)
			lst = get_ids(distance, frequency, suffix)
			-- logln("has " .. #lst .. " documents")
			BERs = {}
			for i = 1, #lst do
				doc = rt.get_file_by_id(get_collection(distance, suffix), lst[i])
				BER = tonumber(doc["BER"]["$numberDouble"])
				-- logln(i .. ":" .. BER)
				BERs[i] = BER
			end
			sum = 0
			for i = 1, #lst do
				sum = sum + BERs[i]
			end
			avr = sum / #lst
			-- logln('average BER: ' .. (avr * 100) .. ' %')
			stddev = 0
			for i = 1, #lst do
				stddev = stddev + (BERs[i] - avr) ^ 2
			end
			stddev = (stddev / #lst) ^ 0.5
			-- logln('deviation: ' ..  (stddev * 100) .. ' %')
			logln("distance: " .. distance .. "m, " .. suffix .. ", BER = " .. (avr * 100) .. '(±' .. (stddev * 100) .. ')% from ' .. #lst .. " documents")
		end
	end
end
