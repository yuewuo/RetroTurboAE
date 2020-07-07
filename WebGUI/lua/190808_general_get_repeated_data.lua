collection = "190808_test_basic_DSM"
data_id_keys = {"data_id"}
base_sequence_key = "base_sequence"
bias = -256  -- this is fine tuned for specific preamble, just change this by plotting

ids = list_all_documents_id(collection)
logln("there're " .. #ids .. " records")
assert(#ids ~= 0)  -- nothing to download

for i = 1,#ids do
-- for i = 1,1 do  -- for testing
    if (i%10 == 0) then logln("[" .. i .. "/" .. #ids .. "]") end
    id = ids[i]
    for j=1,#data_id_keys do
        run("Tester/ExploreLCD/EL_General_Repeat_Record_Get", collection, id, ""..bias, data_id_keys[j], base_sequence_key)
    end
    local tab = rt.get_file_by_id(collection, id)
    for j=1,#data_id_keys do
        local fid = tab["o_"..data_id_keys[j]]
        run("Tester/DebugTest/DT_GetDataFromMongo", tab["name"]..".bin", fid)
    end
end
