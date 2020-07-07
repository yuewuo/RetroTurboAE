collection = "191212_signal_noise_distance_4deg"
-- collection = "191212_signal_noise_distance_4deg_lighton"

for distance=2,24 do
    local ids = list_all_documents_id(collection, [[{"distance": ]]..distance..[[, "gain_dac": 2.0}]])
    local sum = 0
    if #ids ~= 0 then
        id = ids[1]
        local tab = rt.get_file_by_id(collection, id)
        amp = tab.amp["$numberDouble"]
        local x = tab.square1s["$numberDouble"]
        local y = tab.square0s["$numberDouble"]
        local noise = math.sqrt(x*x + y*y)
        logln(distance .. " " .. amp .. " " .. noise)
    end
end
