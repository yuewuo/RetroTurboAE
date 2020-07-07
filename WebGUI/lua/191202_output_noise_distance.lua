collection = "191202_signal_noise_distance"

for distance=2,15 do
    local ids = list_all_documents_id(collection, [[{"distance": ]]..distance..[[}]])
    local square1s_sum = 0
    local square0s_sum = 0
    for i=1,#ids do
        id = ids[i]
        local tab = rt.get_file_by_id(collection, id)
        dac_volt = tab.dac_volt["$numberDouble"]
        AVV = 1000 / 50 * 1 / ( 1 + math.exp( (0.85 - dac_volt) / 0.09 ) )
        square1s = tab.square1s["$numberDouble"] / dac_volt
        square0s = tab.square0s["$numberDouble"] / dac_volt
        -- logln(distance .. ": " .. square1s .. " " .. square0s .. " " .. dac_volt)
        square1s_sum = square1s_sum + square1s
        square0s_sum = square0s_sum + square0s
        rt.save_file(tab)
    end
    local x = square1s_sum/#ids
    local y = square0s_sum/#ids
    local r = math.sqrt(x*x + y*y)
    logln(distance .. " " .. x .. " " .. y .. " " .. r .. " " .. dac_volt .. " " .. AVV)
end
