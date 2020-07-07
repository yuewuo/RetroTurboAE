-- archive distance test

collection = "ArchiveTest"

throughput = {4000, 2000, 1000}
packet_size = 100  -- bits
packet_cnt = 100
-- packet_cnt = 1  -- for debug

distance = 3



reader_gain_control(0.2)

for i=1,packet_cnt do
	for j=1,#throughput do
		id = mongo_create_one_with_jsonstr(collection, "{}")
		tab = rt.get_file_by_id(collection, id)
		logln("record file is [" .. collection .. ":" .. id .. "]")
		tab.length = packet_size
		tab.reverse = 1  -- common state is discharged
		tab.frequency = {} tab.frequency["$numberDouble"] = "" .. (2*throughput[j])
		tab.distance = distance
		rt.save_file(tab)

		run("Tester/Archive/AR_MillerModulate", collection, id)
		local record_id = set_record(1)
		count, real_frequency = tag_send(collection, id, "frequency", "packet")
		sleepms(20)
		set_record(0)
		-- rt.reader.plot_rx_data(record_id)  -- only enable this for debug
		tab = rt.get_file_by_id(collection, id)
		tab.record_id = record_id
		rt.save_file(tab)
	end
end
