collection = "190817_5_yaw_roll_test"

positions = {700, 800, 900}
rolls = {}
for i=0,90,5 do rolls[#rolls+1] = i end
-- for i=1,#rolls do
--     logln("" .. rolls[i])
-- end

results = {}
for i=1,#positions do results[#results+1] = {} end

for x1=1,#rolls do
    roll = rolls[x1]
    for x2=1,#positions do
        position = positions[x2]
        result = results[x2]

        local ids = list_all_documents_id(collection, [[{"roll": ]]..roll..[[, "position": ]]..position..[[, "terminated": {"$exists": false}}]])
        local BERs = {}
        for i=1,#ids do
            id = ids[i]
            local tab = rt.get_file_by_id(collection, id)
            local BER = tab.BER
            if BER then
                BERs[#BERs+1] = tonumber(BER["$numberDouble"])
            end
        end

        -- assert(#BERs ~= 0, "no BER: " .. roll .. ", " .. position)

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
            logln("" .. roll .. ", " .. position .. ": " .. BER_avr .. ", " .. BER_dev)
            result[#result+1] = "" .. roll .. " " .. BER_avr .. " " .. BER_dev
        end

    end
end

for x2=1,#positions do
    position = positions[x2]
    result = results[x2]
    logln("position: " .. position)
    filecontent = ""
    for i=1,#result do
        logln(result[i])
        filecontent = filecontent .. result[i] .. "\n"
    end
    save_to_file("roll_bps"..position..".txt", filecontent)
end
