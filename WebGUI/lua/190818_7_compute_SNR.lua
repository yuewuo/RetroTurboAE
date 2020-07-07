-- you can click buttons below to select a demo code
-- or click above to use library code

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
--         logln("position: " .. position .. ": " .. #ids)
--         for i=1,#ids do
--             id = ids[i]
--             run("Tester/Emulation/EM_ScatterComputeStdDev", collection, id, ""..rpc_port)
--         end
--     end
-- end

-- get those computed ones
results = {}
for i=1,#bit_per_symbols do results[#results+1] = {} end
for x1=1,#positions do
    position = positions[x1]
    for x2=1,#bit_per_symbols do
        bit_per_symbol = bit_per_symbols[x2]
        result = results[x2]

        local ids = list_all_documents_id(collection, [[{"position": ]]..position..[[, "bit_per_symbol": ]]..bit_per_symbol..[[, "terminated": {"$exists": false}}]])

        local stddev_sum = 0
        local stddev_count = 0
        for i=1,#ids do
            id = ids[i]
            local tab = rt.get_file_by_id(collection, id)
            local stddev = tab.scatter_stddev
            if stddev then
                stddev_sum = stddev_sum + tonumber(stddev["$numberDouble"])
                stddev_count = stddev_count + 1
            end
        end

        if stddev_count ~= 0 then
            stddev_avr = stddev_sum / stddev_count
            -- logln("" .. position .. ": " .. stddev_avr .. ", " .. stddev_count)
            result[#result+1] = "" .. position .. " " .. stddev_avr .. " " .. stddev_count
        end
    end
end

for x2=1,#bit_per_symbols do
    bit_per_symbol = bit_per_symbols[x2]
    result = results[x2]
    logln("bit_per_symbol: " .. bit_per_symbol)
    filecontent = ""
    for i=1,#result do
        logln(result[i])
        filecontent = filecontent .. result[i] .. "\n"
    end
    save_to_file("noise_level_"..bit_per_symbol..".txt", filecontent)
end
