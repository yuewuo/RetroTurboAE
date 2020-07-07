-- 190809_handle_ref_renew
-- this is used to handle the output of "190808_reference_collect_renew" 
-- and further output for "Tester/Emulation/AutoGenRefFile.cpp" to use

collection = "190808_reference_collect_renew"
data_id_keys = {}
for i=1,32 do data_id_keys[i] = "data_id_" .. (i-1) end
base_sequence_key = "base_0"
bias = -256  -- this is fine tuned for specific preamble, just change this by plotting
-- *** with positive bias, the curve is moving left ***

ids = list_all_documents_id(collection)
logln("there're " .. #ids .. " records")
assert(#ids ~= 0)  -- nothing to download

for i = 1,#ids do
-- for i = 1,1 do  -- for testing
-- for i = 10,10 do  -- for time alignment debug
    logln("[" .. i .. "/" .. #ids .. "]")
    id = ids[i]
    for j=1,#data_id_keys do
        run("Tester/ExploreLCD/EL_General_Repeat_Record_Get", collection, id, ""..bias, data_id_keys[j], base_sequence_key)
    end
end
