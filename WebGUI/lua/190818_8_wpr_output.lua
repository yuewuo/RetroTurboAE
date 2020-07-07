
collection = "190817_3_distance_eval"
rpc_port = 52229

bit_per_symbols = {2,4,6}
positions = {}
for i=280,1100,20 do positions[#positions+1] = i end
for i=1150,1800,50 do positions[#positions+1] = i end

-- -- compute
-- for x1=1,#positions do
--     position = positions[x1]

--     for x2=1,#bit_per_symbols do
--         bit_per_symbol = bit_per_symbols[x2]

--         local ids = list_all_documents_id(collection, [[{"position": ]]..position..[[, "bit_per_symbol": ]]..bit_per_symbol..[[, "terminated": {"$exists": false}}]])
--         logln("position: " .. position .. ", " .. bit_per_symbol .. ": " .. #ids)
--         for i=1,#ids do
--             id = ids[i]
--             run("Tester/Emulation/EM_ScatterComputeStdDev", collection, id, ""..rpc_port)
--         end
--     end
-- end

-- download all the files
for x1=1,#positions do
    position = positions[x1]

    for x2=1,#bit_per_symbols do
        bit_per_symbol = bit_per_symbols[x2]

        local ids = list_all_documents_id(collection, [[{"position": ]]..position..[[, "bit_per_symbol": ]]..bit_per_symbol..[[, "terminated": {"$exists": false}}]])
        logln("position: " .. position .. ", " .. bit_per_symbol .. ": " .. #ids)
        for i=1,#ids do
            id = ids[i]
            local tab = rt.get_file_by_id(collection, id)
            if tab.wpr_output_id then
                run("Tester/DebugTest/DT_GetDataFromMongo", "wpr_"..bit_per_symbol.."_"..position.."_"..i..".bin", tab.wpr_output_id)
            end
        end
    end
end
