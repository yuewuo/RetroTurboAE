
collection = "tmp"
NLCD = 16
preamble_repeat = 50

-- first create a database file describing how preamble should be recorded
local id = mongo_create_one_with_jsonstr(collection, "{}")
local tab = rt.get_file_by_id(collection, id)
tab.NLCD = NLCD
tab.frequency = {} tab.frequency["$numberDouble"] = "4000"
tab.preamble_repeat = preamble_repeat
rt.save_file(tab)
run("Tester/HandleData/HD_PreambleAutoGen", collection, id)
logln("preamble generated: " .. id)

-- then record 
logln("gain is: " .. reader_gain_control(0.2))
sleepms(100)
local record_id = set_record(1)
sleepms(100)
tag_send(collection, id, "frequency", "o_auto_preamble")
sleepms(100)
set_record(0)
-- rt.reader.plot_rx_data(record_id)
tab = rt.get_file_by_id(collection, id)
tab.record_id = record_id
rt.save_file(tab)  -- save record_id to file

-- get preamble
run("Tester/HandleData/HD_PreambleAutoGet", collection, id)
tab = rt.get_file_by_id(collection, id)
preamble_id = tab.preamble_id
logln("preamble_id: " .. preamble_id)
rt.reader.plot_rx_data(preamble_id)
