-- collection = "tmp"
collection = "200114_model_1ms_17mseq"

-- length_millisecond = 1000
length_millisecond = 61 * 60 * 1000  -- 1 hour + 1min

logln("gain is: " .. reader_gain_control(0.2))

id = mongo_create_one_with_jsonstr(collection, "{}")
local tab = rt.get_file_by_id(collection, id)

record_id = rt.reader.record_rx_data(length_millisecond)

tab.record_id = record_id
tab.length_millisecond = length_millisecond
rt.save_file(tab)

-- plot(record_id)
