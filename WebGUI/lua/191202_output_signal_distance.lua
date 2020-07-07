collection = "191202_signal_noise_distance"

for distance=2,15 do
    local ids = list_all_documents_id(collection, [[{"distance": ]]..distance..[[}]])
    local sum = 0
    for i=1,#ids do
        id = ids[i]
        local tab = rt.get_file_by_id(collection, id)
        dac_volt = tab.dac_volt["$numberDouble"]
        AVV = 1000 / 50 * 1 / ( 1 + math.exp( (0.85 - dac_volt) / 0.09 ) )
        amp = tab.amp["$numberDouble"]
        sensor_signal = amp / AVV
        tab.sensor_signal = {} tab.sensor_signal["$numberDouble"] = "" .. sensor_signal
        rt.save_file(tab)
        sum = sum + tab.sensor_signal["$numberDouble"]
    end
    local avr = sum / #ids
    logln(distance .. " " .. avr)
end
