
collection = "eval_LCD_DSM_base_new"
ids = list_all_documents_id(collection)

for i=1,#ids do
-- for i=1,1 do
    logln("i: "..i)
    for j=0,15 do
        run("Tester/ExploreLCD/EL_AutoGenRef8421_HandleOne", collection, ids[i], ""..j)
    end
    run("Tester/ExploreLCD/EL_AutoGenRef8421_Combine", collection, ids[i], "refs_id", 
        "ref_id_0", "ref_id_1", "ref_id_2", "ref_id_3",
        "ref_id_4", "ref_id_5", "ref_id_6", "ref_id_7",
        "ref_id_8", "ref_id_9", "ref_id_10", "ref_id_11",
        "ref_id_12", "ref_id_13", "ref_id_14", "ref_id_15"
    )
    tab = rt.get_file_by_id(collection, ids[i])
    frequency = math.modf(tonumber(tab.frequency["$numberDouble"]))
    duty = tonumber(tab.duty["$numberInt"])
    run("tester/DebugTest/DT_GetDataFromMongo", "refs16x2_"..frequency.."_"..duty..".bin", tab["refs_id"])
end
