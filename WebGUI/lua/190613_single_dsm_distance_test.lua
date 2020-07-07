
collection = "test190613_sdsm_distance"
-- collection = "test190613_sdsm_distance_tmp"

combine = {1,2,4}
frequency = {4000, 5333.33, 2666.67, 2000, 1000}
packet_size = 128  -- bytes
packet_cnt = 10  -- 128*8*10 = 10kbit
-- packet_cnt = 1

distance = 7.0

logln("gain is: " .. reader_gain_control(0.2))
sleepms(100)

for i=1,#frequency do
-- for i=1,1 do
	for j=1,#combine do
  -- for j=1,1 do
    for k=1,packet_cnt do

      -- first create document contain the parameters
      id = mongo_create_one_with_jsonstr(collection, "{}")
      local tab = rt.get_file_by_id(collection, id)
      -- logln("record file is [" .. collection .. ":" .. id .. "]")
      logln("" .. i .. ", " .. j .. ", " .. k)
      tab.NLCD = 16
      tab.ct_fast = 8
      tab.ct_slow = 64
      tab.combine = combine[j]
      tab.cycle = 16
      tab.duty = 2
      tab.data = generate_random_data(packet_size)
      tab.frequency = {} tab.frequency["$numberDouble"] = "" .. frequency[i]
      tab.distance = {} tab.distance["$numberDouble"] = "" .. distance
      rt.save_file(tab)

      -- then call program to modulate data
      run("Tester/HandleData/HD_SingleDSM_Modulate", collection, id)
      tab = rt.get_file_by_id(collection, id)

      -- start recording data and then start tag data sending process
      local record_id = set_record(1)
      count, real_frequency = tag_send(collection, id, "frequency", "o_out")
      sleepms(100)
      set_record(0)
      rt.reader.plot_rx_data(record_id)
      tab = rt.get_file_by_id(collection, id)
      tab.record_id = record_id
      tab.real_frequency = {} tab.real_frequency["$numberDouble"] = "" .. real_frequency
      rt.save_file(tab)

    end
	end
end
