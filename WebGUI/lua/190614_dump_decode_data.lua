
collection = "test190613_sdsm_distance"

function get_ids(frequency, combine, distance)
	return list_all_documents_id(collection, [[{"BER":{"$exists": true}, "removed":{"$exists": false}, "frequency":]] .. frequency .. [[,"combine":]] .. combine .. [[,"distance":]] .. distance .. [[}]])
end

distances = {1.5, 2, 2.5, 3, 3.5, 4, 4.5, 5, 5.5, 6, 6.5, 7}
combines = {1, 2, 4}
frequencys = {4000, 5333.33, 2666.67, 2000, 1000}

-- logln("first evaluate how frequency effect SNR performance, use combine=1")
-- for j=1,#frequencys do frequency = frequencys[j] logln("frequency: " .. frequency)
-- 	for i=1,#distances do distance = distances[i] --logln("distance: " .. distance)
-- 		ids = get_ids(frequency, 1, distance)
-- 		-- log("" .. #ids .. ": ")
-- 		BERsum = 0
-- 		BER2sum = 0
-- 		for k=1,#ids do
-- 			demod = rt.get_file_by_id(collection, ids[k])
-- 			BER = tonumber(demod.BER["$numberDouble"])
-- 			BER = math.floor(BER * 10000 + 0.5) / 10000
-- 			BERsum = BERsum + BER
-- 			BER2sum = BER2sum + BER * BER
-- 			-- log("" .. (BER*100) .. "% ")
-- 		end
-- 		-- log("\n")
-- 		BERavr = BERsum / #ids
-- 		BERdev = (BER2sum - 2 * BERavr * BERsum + BERavr * BERavr) / #ids
-- 		-- logln("" .. distance .. " " .. BERavr)
-- 		-- logln("" .. distance .. " " .. BERavr .. " " .. BERdev)
-- 		logln("" .. distance .. " " .. BERavr .. " " .. BERavr .. " 0")
-- 	end
-- end

logln("second evaluate how frequency effect SNR performance, use frequency=4000")
for j=1,#combines do combine = combines[j] logln("combine: " .. combine)
	for i=1,#distances do distance = distances[i] --logln("distance: " .. distance)
		ids = get_ids(4000, combine, distance)
		-- log("" .. #ids .. ": ")
		BERsum = 0
		BER2sum = 0
		for k=1,#ids do
			demod = rt.get_file_by_id(collection, ids[k])
			BER = tonumber(demod.BER["$numberDouble"])
			BER = math.floor(BER * 10000 + 0.5) / 10000
			BERsum = BERsum + BER
			BER2sum = BER2sum + BER * BER
			-- log("" .. (BER*100) .. "% ")
		end
		-- log("\n")
		BERavr = BERsum / #ids
		BERdev = (BER2sum - 2 * BERavr * BERsum + BERavr * BERavr) / #ids
		-- logln("" .. distance .. " " .. BERavr)
		-- logln("" .. distance .. " " .. BERavr .. " " .. BERdev)
		logln("" .. distance .. " " .. BERavr .. " " .. BERavr .. " 0")
	end
end
