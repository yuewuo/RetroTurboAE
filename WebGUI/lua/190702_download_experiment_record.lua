-- you can download experiment record using this script
-- for your convinience to move them to another database
-- this should be used in pair with `190702_upload_experiment_record.lua` which add them into local database

-- copy the following three lines to upload those records
collection = "test190621_pqam_dsm"
gridfs_id_keys = {"data_id"}
main_record_file = "record_"..collection..".json"




ids = list_all_documents_id(collection)
logln("there're " .. #ids .. " records")
assert(#ids ~= 0)  -- nothing to download

record_storage = 0
record_str = '{"create": "'..os.date("%Y-%m-%d %H:%M:%S", os.time())..'", "documents": ['

for i = 1,#ids do
-- for i = 1,1 do  -- for testing
	if (i%10 == 0) then logln("[" .. i .. "/" .. #ids .. "]") end
	tab = rt.get_file_by_id(collection, ids[i])
	record_str = record_str .. cjson.encode(tab) .. ","
	for j=1,#gridfs_id_keys do
		data_id = tab[gridfs_id_keys[j]]
		logln("downloading ["..gridfs_id_keys[j].."] "..data_id)
		run("Tester/DebugTest/DT_GetDataFromMongo", "gridfs_"..data_id, data_id)
		meta = rt.get_metadata(data_id)
		dimension = meta["dimension"]["$numberInt"]
		length = meta["length"]["$numberInt"]
		typesize = meta["typesize"]["$numberInt"]
		filesize = dimension * length * typesize
		record_storage = record_storage + filesize
		may_terminate()  -- to allow terminate in process
		record_str = record_str .. cjson.encode(meta) .. ","
	end
end

record_str = string.sub(record_str,1,-2) .. "]}"
save_to_file(main_record_file, record_str)

logln("raw data occupies " .. (record_storage / 1e9) .. " GB")
