collection = "191122_distance_1k"

positions = {}
for i=280,1100,20 do positions[#positions+1] = i end
-- for i=1,#positions do
--     logln("" .. positions[i])
-- end

result = {}

for x1=1,#positions do
    position = positions[x1]

    local ids = list_all_documents_id(collection, [[{"distance": ]]..position..[[, "terminated": {"$exists": false}}]])
    local SNRs = {}
    for i=1,#ids do
        id = ids[i]
        local tab = rt.get_file_by_id(collection, id)
        local SNR = tab.SNR
        if SNR then
            SNRs[#SNRs+1] = tonumber(SNR["$numberDouble"])
        end
    end

    -- assert(#SNRs ~= 0, "no SNR: " .. position .. ", " .. bit_per_symbol)

    if #SNRs ~= 0 then
        local SNR_sum = 0
        for i=1,#SNRs do
            SNR_sum = SNR_sum + SNRs[i]
        end
        local SNR_avr = SNR_sum / #SNRs
        local SNR_dev = 0
        for i=1,#SNRs do
            SNR_dev = (SNRs[i] - SNR_avr) *  (SNRs[i] - SNR_avr)
        end
        SNR_dev = math.sqrt(SNR_dev / #SNRs)
        logln("" .. position .. ": " .. SNR_avr .. ", " .. SNR_dev)
        result[#result+1] = "" .. position .. " " .. SNR_avr .. " " .. SNR_dev
    end

end


filecontent = ""
for i=1,#result do
    logln(result[i])
    filecontent = filecontent .. result[i] .. "\n"
end
save_to_file("distance_1Kbps.txt", filecontent)

