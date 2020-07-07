
collection = "test190613_sdsm_distance"

ids = list_all_documents_id(collection, [[{"BER":{"$exists": false}}]])

logln("there're " .. #ids .. " records failed decode")

for i = 1,#ids do
	tab = rt.get_file_by_id(collection, ids[i])
	record_id = tab["record_id"]
	logln("id(" .. ids[i] .. ") record(" .. record_id .. ")")
end
