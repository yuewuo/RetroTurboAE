-- testing the effect of channel training, by using different V

V = 3

rpc_port = 52220
collection = "200206_smalltag_distance_channel_training_eval"
local ids = list_all_documents_id(collection, "{}")

for i=1,#ids do
    local id = ids[i]
    local tab = rt.get_file_by_id(collection, id)
    tab.effect_length = 3
    tab.evaluate_V = V
    rt.save_file(tab)
    run("Tester/Emulation/EM_Demodulate", collection, id, ""..rpc_port)
    local tab = rt.get_file_by_id(collection, id)
    local position = tab.position
    local BER = tonumber(tab.BER["$numberDouble"])
    logln(position.." "..(BER*100).."%")
    tab["BER_V"..V] = tab.BER
    rt.save_file(tab)
end
