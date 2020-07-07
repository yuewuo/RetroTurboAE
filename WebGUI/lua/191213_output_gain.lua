collection = "191212_signal_noise_distance_4deg"
distance = 6

local ids = list_all_documents_id(collection, [[{"distance": ]]..distance..[[}]])
for i=1,#ids do
    id = ids[i]
    local tab = rt.get_file_by_id(collection, id)
    local gain_dac_x100 = math.floor(tab.gain_dac["$numberDouble"] * 100 + 0.5)
    amp = tab.amp["$numberDouble"]
    local x = tab.square1s["$numberDouble"]
    local y = tab.square0s["$numberDouble"]
    local noise = math.sqrt(x*x + y*y)
  	local SNR = amp / noise
    logln((gain_dac_x100/100) .. " " .. amp .. " " .. noise .. " " .. SNR)
end
