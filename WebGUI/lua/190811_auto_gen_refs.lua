collection = "190808_reference_collect_renew"

ids = list_all_documents_id(collection)
logln("there're " .. #ids .. " records")
assert(#ids ~= 0)  -- nothing to download

for i = 1,#ids do
-- for i = 1,1 do  -- for testing
    logln("[" .. i .. "/" .. #ids .. "]")
    id = ids[i]
    run("Tester/Emulation/EM_AutoGenRefFile", collection, id)
    local tab = rt.get_file_by_id(collection, id)
    local o_refs_id = tab.o_refs_id
    local meta = rt.get_metadata(o_refs_id)
    NLCD = meta.NLCD["$numberInt"]
    duty = meta.duty["$numberInt"]
    cycle = meta.cycle["$numberInt"]
    frequency = math.floor(meta.frequency["$numberDouble"] + 0.5)
    effect_length = meta.effect_length["$numberInt"]
    filename = "turborefs_" .. NLCD .. "_" .. duty .. "_" .. cycle .. "_" .. frequency .. "_" .. effect_length .. ".bin"
    logln("save reference to file: " .. filename)
    run("Tester/DebugTest/DT_GetDataFromMongo", filename, o_refs_id)
end
