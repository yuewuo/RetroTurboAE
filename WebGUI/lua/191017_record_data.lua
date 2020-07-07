collection = "191017_record_data"

local record_id = set_record(1)
sleepms(5000)
set_record(0)
rt.reader.plot_rx_data(record_id)
logln("record_id: " .. record_id)
run("Tester/DebugTest/DT_GetDataFromMongo", "191017_"..record_id, record_id)
