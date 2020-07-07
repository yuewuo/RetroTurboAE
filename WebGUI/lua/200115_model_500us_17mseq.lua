-- collection = "tmp"
collection = "200115_model_500us_17mseq"

-- length_millisecond = 1000
length_millisecond = 35 * 60 * 1000  -- 35 min

logln("gain is: " .. reader_gain_control(0.2))

id = mongo_create_one_with_jsonstr(collection, "{}")
local tab = rt.get_file_by_id(collection, id)

record_id = rt.reader.record_rx_data(length_millisecond)

tab.record_id = record_id
tab.length_millisecond = length_millisecond
rt.save_file(tab)

-- plot(record_id)
