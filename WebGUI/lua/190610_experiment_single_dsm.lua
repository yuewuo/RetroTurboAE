collection = "test190610_sdsm"
for i=1,2 do
  id = mongo_create_one_with_jsonstr(collection, "{}")
  tab = rt.get_file_by_id(collection, id)
  logln("record file is [" .. collection .. ":" .. id .. "]")
  tab.NLCD = 16
  tab.ct_fast = 8
  tab.ct_slow = 64
  tab.combine = 1
  tab.cycle = 16
  tab.duty = 2
  tab.data = generate_random_data(128)
  tab.frequency = {} tab.frequency["$numberDouble"] = "4000.0"
  rt.save_file(tab)
  run("Tester/HandleData/HD_SingleDSM_Modulate", collection, id)
  tab = rt.get_file_by_id(collection, id)
  logln("throughput is " .. tab.o_throughput["$numberDouble"] .. " bps")

  local record_id = set_record(1)
  count, real_frequency = tag_send(collection, id, "frequency", "o_out")
  logln(count)
  logln(real_frequency)
  sleepms(20)
  set_record(0)
  rt.reader.plot_rx_data(record_id)
  tab = rt.get_file_by_id(collection, id)
  tab.record_id = record_id
  rt.save_file(tab)
end
