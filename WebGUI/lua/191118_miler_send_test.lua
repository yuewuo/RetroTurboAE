-- you can click buttons below to select a demo code
-- or click above to use library code
collection = "191118_test"

length = 8
reverse = 0
base_frequency = 2000.0
rate = 1
real_frequency = base_frequency / (2 ^ (3 - rate))

tag_set_en9(1)
tag_set_pwsel(0) --use 5V voltage

reader_gain_control(0.2)

id = mongo_create_one_with_jsonstr(collection, "{}")
local tab = rt.get_file_by_id(collection, id)
tab.length = length
tab.reverse = reverse
tab.real_frequency = {} tab.real_frequency["$numberDouble"] = "" .. real_frequency
tab.base_frequency = {} tab.base_frequency["$numberDouble"] = "" .. base_frequency
tab.rate = rate
rt.save_file(tab)
run("Tester/Archive/AR_MillerModulateRecordData", collection, id)

record_id = set_record(1)
count, real_frequency = tag_send(collection, id, "base_frequency", "packet")
sleepms(50)
set_record(0)
rt.reader.plot_rx_data(record_id)
logln("record_id: " .. record_id)

tab = rt.get_file_by_id(collection, id)
tab.demod_data_id = record_id
tab.channel = 1
rt.save_file(tab)
run("Tester/Emulation/EM_DemodulateMiller", collection, id)