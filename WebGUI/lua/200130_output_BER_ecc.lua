collection = "200206_8kbps_2bytes" --IMPORTANT: manually check it
filename = "8kbps_ecc_2bytes.txt" --IMPORTANT: manually check it
bitrate = 8.0 * (128 - 2) / 128 --IMPORTANT: manually check it

positions = {}
for i=10,100,1 do positions[#positions+1] = i end
-- for i=1,#positions do
--     logln("" .. positions[i])
-- end

result = {}

for x1=1,#positions do
    position = positions[x1]

    -- local ids = list_all_documents_id(collection, [[{"SNR_dB": ]]..position..[[, "terminated": {"$exists": false}}]])
    local ids = list_all_documents_id(collection, [[{"SNR_dB": ]]..position..[[, "terminated": {"$exists": false}, "packet_lost": {"$exists": true}}]])
    local BERs = {}
    local repaired_BERs = {}
    local packet_losts = 0
    for i=1,#ids do
        id = ids[i]
        local tab = rt.get_file_by_id(collection, id)
        local BER = tab.BER
        local repaired_BER = tab.Repaired_BER
        local packet_lost = tab.packet_lost
        if BER then
            BERs[#BERs+1] = tonumber(BER["$numberDouble"])
        end
        if repaired_BER then
            repaired_BERs[#repaired_BERs+1] = tonumber(repaired_BER["$numberDouble"])
        end
        -- if tonumber(BER["$numberDouble"]) > 0.0001 then
        --     packet_losts = packet_losts + 1
        -- end
        packet_losts = packet_losts + tonumber(packet_lost["$numberInt"])
        
        -- if packet_lost then
        --     packet_losts = packet_losts + tonumber(packet_lost["$numberInt32"])
        --     logln(tonumber(packet_lost["$numberInt"]))
        -- end
    end

    -- assert(#BERs ~= 0, "no BER: " .. position .. ", " .. bit_per_symbol)

    if #BERs ~= 0 then
        local BER_sum = 0
        for i=1,#BERs do
            BER_sum = BER_sum + BERs[i]
        end
        local BER_avr = BER_sum / #BERs

        local repaired_BER_sum = 0
        for i=1,#repaired_BERs do
            repaired_BER_sum = repaired_BER_sum + repaired_BERs[i]
        end
        local repaired_BER_avr = repaired_BER_sum / #repaired_BERs

        local PLR = packet_losts / #ids
        local goodput = (1.0 - PLR) * bitrate

        result[#result+1] = "" .. position .. " " .. BER_avr .. " " .. repaired_BER_avr .. " " .. PLR .. " " .. goodput
        -- result[#result+1] = "" .. position .. " " .. BER_avr .. " " .. BER_avr .. " " .. PLR .. " " .. goodput
    end

end


filecontent = ""
for i=1,#result do
    logln(result[i])
    filecontent = filecontent .. result[i] .. "\n"
end
save_to_file(filename, filecontent)

