collection = "191020_receive_preamble_packet"
preamble_id = "5DAB859AF35F0000DB001CF2"

logln("loading preamble reference")
load_preamble_ref(preamble_id)

-- reader auto gain control
gain = reader_gain_control(0.2)
logln("gain is: " .. gain)
sleepms(100)

reader_start_preamble(math.ceil(80000 * 0.3), 20)  -- 300ms packet
has = reader_wait_preamble(3)  -- wait for 3s
reader_stop_preamble()

if has then
    data_id = reader_save_preamble()
    rt.reader.plot_rx_data(data_id)
    -- first create a database file describing how preamble should be recorded
    local id = mongo_create_one_with_jsonstr(collection, "{}")
    local tab = rt.get_file_by_id(collection, id)
    tab.data_id = data_id
    tab.gain = {} tab.gain["$numberDouble"] = "" .. gain
    rt.save_file(tab)
else 
    logln("preamble failed")
end
