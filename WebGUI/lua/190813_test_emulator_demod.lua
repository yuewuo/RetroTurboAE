-- this is very interesting! I,Q's noise level is not equal! this leads to possible design that use 8PAM on I and 4PAM on Q
-- very interesting!
-- you can download the data from github release

collection = "tmp"
packet_size = 128  -- bytes
refs_id = "5D5232DF286E0000030019A2"  -- this must match with the parameters below, otherwise will fail to emulate

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
tab.bit_per_symbol = 4
-- tab.bit_per_symbol = 6
tab.data = generate_random_data(packet_size)
tab.frequency = {} tab.frequency["$numberDouble"] = "4000"

tab.bias = -256
tab.effect_length = 3
tab.refs_id = refs_id
tab.noise = {} tab.noise["$numberDouble"] = "0.004"
rt.save_file(tab)

-- then call program to emulate the waveform
run("Tester/Emulation/EM_MongoEmulate", collection, id)

-- then draw it
tab = rt.get_file_by_id(collection, id)
rt.reader.plot_rx_data(tab.emulated_id)

-- next analyze it using scatter plot as well as demodulation
tab.scatter_data_id = tab.emulated_id
tab.demod_data_id = tab.emulated_id
tab.demod_buffer_length = 1
tab.demod_nearest_count = 4
rt.save_file(tab)
run("Tester/Emulation/EM_ScatterPlot", collection, id)
run("Tester/Emulation/EM_Demodulate", collection, id)

-- then plot scatter
tab = rt.get_file_by_id(collection, id)
rt.reader.plot_rx_data(tab.scatter_output_id)
