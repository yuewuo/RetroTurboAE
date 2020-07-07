collection = "191122_XXXXX"

positions = {}
for i=280,1100,10 do positions[#positions+1] = i end
-- for i=1,#positions do
--     logln("" .. positions[i])
-- end

result = {}
byte_per_packet = 4

for x1=1,#positions do
    position = positions[x1]

    local ids = list_all_documents_id(collection, [[{"distance": ]]..position..[[, "terminated": {"$exists": false}}]])
    local decoded_list = {}
    local data_list = {}
    
    for i=1,#ids do
        id = ids[i]
        local tab = rt.get_file_by_id(collection, id)
        local decoded = tab.decoded
        local data = tab.data
        decoded_list[#decoded_list+1] = decoded
        data_list[#data_list+1] = data        
    end

    -- assert(#SNRs ~= 0, "no SNR: " .. position .. ", " .. bit_per_symbol)

    local PLR_sum = 0
    for p=1,#decoded_list do
        local decoded = decoded_list[p]
        local data = data_list[p]
        for start=1,13,4 do
            curr_decoded = string.sub(decoded, start, start + 4)
            curr_data = string.sub(data, start, start + 4)
            if curr_data ~= curr_decoded then
                PLR_sum = PLR_sum + 1
            end
        end
    end
    local PLR = PLR_sum / 100
    if #decoded_list ~= 0 then
        logln("" .. position .. ": " .. PLR)
        result[#result+1] = "" .. position .. " " .. PLR 
    end
end


filecontent = ""
for i=1,#result do
    logln(result[i])
    filecontent = filecontent .. result[i] .. "\n"
end
save_to_file("XXX.txt", filecontent)

