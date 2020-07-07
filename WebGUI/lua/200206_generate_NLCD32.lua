
collection = "190808_reference_collect_renew"
new_collection = "200206_reference_collect_NLCD32"

local ids = list_all_documents_id(collection, [[{"NLCD": 16}]])

for i=1,#ids do
    id = ids[i]
    local tab = rt.get_file_by_id(collection, id)
    local new_id = mongo_create_one_with_jsonstr(new_collection, "{}")
    local new_tab = rt.get_file_by_id(new_collection, new_id)
    new_tab.NLCD = 32
    new_tab.duty = tab.duty
    new_tab.frequency = tab.frequency
    new_tab.cycle = tab.cycle
    new_tab.effect_length = tab.effect_length
    new_tab.o_refs_id_32 = tab.o_refs_id
    rt.save_file(new_tab)
    sleepms(100)
    run("Tester/ExploreLCD/EL_AutoGenRef8421_Combine", new_collection, new_id, "o_refs_id", "M0.5",
        "o_refs_id_32", "o_refs_id_32"  -- concatenate two references
    )
    local new_tab = rt.get_file_by_id(new_collection, new_id)
    local o_refs_id = new_tab.o_refs_id
    logln(o_refs_id)
    local meta = rt.get_file_by_id("fs.files", o_refs_id)
    meta.metadata.NLCD = 32
    meta.metadata.duty = tab.duty
    meta.metadata.frequency = tab.frequency
    meta.metadata.cycle = tab.cycle
    meta.metadata.effect_length = tab.effect_length
    rt.save_file(meta)
end
