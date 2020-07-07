collection = "191123_angle_2m"

positions = {}
for i=0,30,1 do positions[#positions+1] = i end
-- for i=1,#positions do
--     logln("" .. positions[i])
-- end

result = {}

for x1=1,#positions do
    position = positions[x1]

    local ids = list_all_documents_id(collection, [[{"angle": ]]..position..[[, "terminated": {"$exists": false}}]])
    local BERs = {}
    for i=1,#ids do
        id = ids[i]
        local tab = rt.get_file_by_id(collection, id)
        local BER = tab.BER
        if BER then
            BERs[#BERs+1] = tonumber(BER["$numberDouble"])
        end
    end

    -- assert(#BERs ~= 0, "no BER: " .. position .. ", " .. bit_per_symbol)

    if #BERs ~= 0 then
        local BER_sum = 0
        for i=1,#BERs do
            BER_sum = BER_sum + BERs[i]
        end
        local BER_avr = BER_sum / #BERs
        local BER_dev = 0
        for i=1,#BERs do
            BER_dev = (BERs[i] - BER_avr) *  (BERs[i] - BER_avr)
        end
        BER_dev = math.sqrt(BER_dev / #BERs)
        logln("" .. position .. ": " .. BER_avr .. ", " .. BER_dev)
        result[#result+1] = "" .. position .. " " .. BER_avr .. " " .. BER_dev
    end

end


filecontent = ""
for i=1,#result do
    logln(result[i])
    filecontent = filecontent .. result[i] .. "\n"
end
save_to_file("Angle_2m.txt", filecontent)

