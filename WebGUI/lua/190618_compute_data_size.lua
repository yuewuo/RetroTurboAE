
collection = "test190618_pqam_dsm_distance"

ids = list_all_documents_id(collection)
logln("there're " .. #ids .. " records")

record_storage = 0

for i = 1,#ids do
	if (i%10 == 0) then logln("[" .. i .. "/" .. #ids .. "]") end
	tab = rt.get_file_by_id(collection, ids[i])
	record_id = tab["record_id"]
	meta = rt.get_metadata(record_id)
	dimension = meta["dimension"]["$numberInt"]
	length = meta["length"]["$numberInt"]
	typesize = meta["typesize"]["$numberInt"]
	filesize = dimension * length * typesize
	record_storage = record_storage + filesize
end

logln("raw data occupies " .. (record_storage / 1e9) .. " GB")
