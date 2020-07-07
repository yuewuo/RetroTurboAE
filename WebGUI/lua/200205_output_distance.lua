collection = "200204_smalltag_distance_dark"

bit_per_symbol = 4
results = {}

local ids = list_all_documents_id(collection, [[{"BER": {"$exists": true}, "bit_per_symbol": ]]..bit_per_symbol..[[, "terminated": {"$exists": false}}]])

for i=1,#ids do
    local id = ids[i]
    local tab = rt.get_file_by_id(collection, id)
    local position = tab.position
    local BER = tonumber(tab.BER["$numberDouble"])
    local key = position
    if results[key] == nil then
        results[key] = {}
    end
    results[key][#results[key] + 1] = BER
end

positions = {}
for position, BERs in pairs(results) do
    positions[#positions + 1] = position
end
table.sort(positions, function(a, b) return tonumber(a) < tonumber(b) end)

for _, position in pairs(positions) do
    BERs = results[position]
    assert(#BERs ~= 0, "no BER: " .. position)
    local BER_sum = 0
    for i=1,#BERs do
        BER_sum = BER_sum + BERs[i]
    end
    local BER_avr = BER_sum / #BERs
    local BER_dev = 0
    for i=1,#BERs do
        BER_dev = (BERs[i] - BER_avr) * (BERs[i] - BER_avr)
    end
    BER_dev = math.sqrt(BER_dev / #BERs)
    logln("" .. position .. " " .. BER_avr .. ", " .. BER_dev)
end
