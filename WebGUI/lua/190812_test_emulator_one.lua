collection = "tmp"
packet_size = 128  -- bytes
-- refs_id = "5D5232E6DB6E000021006A02"  -- this must match with the parameters below, otherwise will fail to emulate
refs_id = "5D5232DF286E0000030019A2"

-- first create document contain the parameters
local id = mongo_create_one_with_jsonstr(collection, "{}")
local tab = rt.get_file_by_id(collection, id)
logln("record file is [" .. collection .. ":" .. id .. "]")
tab.NLCD = 16
tab.ct_fast = 8
tab.ct_slow = 64
tab.combine = 1
tab.cycle = 32
tab.duty = 4
tab.bit_per_symbol = 2
tab.data = generate_random_data(packet_size)
tab.frequency = {} tab.frequency["$numberDouble"] = "4000"

tab.bias = -256
tab.effect_length = 3
tab.refs_id = refs_id
tab.noise = {} tab.noise["$numberDouble"] = "0.01"
rt.save_file(tab)

-- then call program to emulate the waveform
run("Tester/Emulation/EM_MongoEmulate", collection, id)

-- then draw it
tab = rt.get_file_by_id(collection, id)
rt.reader.plot_rx_data(tab.emulated_id)

-- next analyze it using scatter plot
tab.scatter_data_id = tab.emulated_id
rt.save_file(tab)
run("Tester/Emulation/EM_ScatterPlot", collection, id)
tab = rt.get_file_by_id(collection, id)

-- then plot scatter
tab = rt.get_file_by_id(collection, id)
rt.reader.plot_rx_data(tab.scatter_output_id)
