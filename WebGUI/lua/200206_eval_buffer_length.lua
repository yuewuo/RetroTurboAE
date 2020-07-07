-- testing the effect of channel training, by using different V

buffer_lengths = {1, 4, 16, 64, 256, 16384}

ids = {
"5E3A34D2E6490000030071F4"
}

rpc_port = 52220
collection = "200206_smalltag_distance_channel_training_eval"

for i=1,#ids do
    local id = ids[i]
    for j=1,#buffer_lengths do
        local buffer_length = buffer_lengths[j]
        local tab = rt.get_file_by_id(collection, id)
        tab.demod_buffer_length = buffer_length
        tab.evaluate_V = 3
        rt.save_file(tab)
        run("Tester/Emulation/EM_Demodulate", collection, id, ""..rpc_port)
        local tab = rt.get_file_by_id(collection, id)
        local position = tab.position
        local BER = tonumber(tab.BER["$numberDouble"])
        logln(position.." "..buffer_length.." "..(BER*100).."%")
        tab["BER_buf"..buffer_length] = tab.BER
        rt.save_file(tab)
    end
end
