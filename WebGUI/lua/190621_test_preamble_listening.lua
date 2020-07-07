
preamble_id = "5D0D3BF493150000DA001A92"

load_preamble_ref(preamble_id)

-- then generate some data with preamble
collection = "tmp"
NLCD = 16
preamble_repeat = 1
-- first create a database file describing how preamble should be recorded
local id = mongo_create_one_with_jsonstr(collection, "{}")
local tab = rt.get_file_by_id(collection, id)
tab.NLCD = NLCD
tab.frequency = {} tab.frequency["$numberDouble"] = "4000"
tab.preamble_repeat = preamble_repeat
rt.save_file(tab)
run("Tester/HandleData/HD_PreambleAutoGen", collection, id)
logln("preamble generated: " .. id)

logln("gain is: " .. reader_gain_control(0.2))
sleepms(100)

local record_id = set_record(1)
reader_start_preamble(0.1*80000, 20)
sleepms(200)
tag_send(collection, id, "frequency", "o_auto_preamble")
has = reader_wait_preamble(3)  -- wait for 1s
reader_stop_preamble()
set_record(0)
logln(has)

data_id = reader_save_preamble()
rt.reader.plot_rx_data(data_id)

-- rt.reader.plot_rx_data(record_id)
