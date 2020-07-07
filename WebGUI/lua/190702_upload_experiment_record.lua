-- copy the following three lines from download script to here
collection = "test190621_pqam_dsm"
gridfs_id_keys = {"data_id"}
main_record_file = "record_"..collection..".json"

replace_same_id = true  -- false will cause error when insert, true will just warning


records = cjson.decode(get_from_file(main_record_file))
logln("record is downloaded at "..records["create"])
assert(#records.documents % (1 + #gridfs_id_keys) == 0)  -- or there is error
documents_cnt = #records.documents // (1 + #gridfs_id_keys)
logln("there are ".. documents_cnt .." documents in the record")

acc = 1 + #gridfs_id_keys
for i = 1,documents_cnt do
	if (i%10 == 0) then logln("[" .. i .. "/" .. documents_cnt .. "]") end
	main_doc = records.documents[acc*(i-1) + 1]
	ret = mongo_create_one_with_jsonstr(collection, cjson.encode(main_doc))
	assert(replace_same_id == true or ret ~= nil, "document with same ID exists, modify code")
	if ret == nil then logln("[warning]: replace document " .. main_doc._id["$oid"]) end
	for j = 1,#gridfs_id_keys do
		data_id = main_doc[gridfs_id_keys[j]]
		logln("uploading ["..gridfs_id_keys[j].."] "..data_id)
		run("Tester/DebugTest/DT_SaveDataToMongo", "gridfs_"..data_id, collection, main_doc._id["$oid"], gridfs_id_keys[j])
		-- because cannot upload file with specific ID (due to mongoc driver bug, this will modify the data_id)
		new_data_id = cjson.decode(mongo_get_one_as_jsonstr(collection, '{"_id":{"$oid":"'..main_doc._id["$oid"]..'"}}', '{"limit": 1}'))[gridfs_id_keys[j]]
		grid_data = cjson.decode(mongo_get_one_as_jsonstr("fs.files", '{"_id":{"$oid":"'..new_data_id..'"}}', '{"limit": 1}'))
		grid_data.metadata = records.documents[acc*(i-1) + 1 + j]
		grid_data.metadata["origin_id"] = data_id
		mongo_replace_one_with_jsonstr("fs.files", '{"_id":{"$oid":"'..new_data_id..'"}}', cjson.encode(grid_data))
	end
end
