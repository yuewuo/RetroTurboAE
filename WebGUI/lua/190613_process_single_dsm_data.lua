
collection = "test190613_sdsm_distance"

-- ids = list_all_documents_id(collection)
-- if you wanna handle part of them, use below
-- ids = list_all_documents_id(collection, [[{"frequency":{"$numberDouble": "5333.33"}}]])
ids = list_all_documents_id(collection, [[{"BER":{"$exists": false}}]])

logln("there're " .. #ids .. " records")

record_storage = 0

-- for i = 1,1 do
for i = 1,#ids do
	tab = rt.get_file_by_id(collection, ids[i])
	record_id = tab["record_id"]
	meta = rt.get_metadata(record_id)
	dimension = meta["dimension"]["$numberInt"]
	length = meta["length"]["$numberInt"]
	typesize = meta["typesize"]["$numberInt"]
	filesize = dimension * length * typesize
	record_storage = record_storage + filesize
	ret = run("Tester/HandleData/HD_SingleDSM_Demodulate", collection, ids[i])
	if ret ~= 0 then
		logln("[" .. i .. "] error occured when demodulate")
	else
		assert(1, "demodulate failed")
		demod = rt.get_file_by_id(collection, ids[i])
		BER = tonumber(demod.BER["$numberDouble"])
		BER = math.floor(BER * 10000 + 0.5) / 10000
		-- logln(demod)
		logln("[" .. i .. "] frequency(" .. demod.frequency["$numberDouble"] .. ") combine(" .. demod.combine["$numberInt"] .. ") BER: " .. (BER*100) .. "%")
	end
end

logln("raw data occupies " .. (record_storage / 1e9) .. " GB")
