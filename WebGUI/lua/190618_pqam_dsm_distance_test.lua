
collection = "test190618_pqam_dsm_distance"
-- collection = "test190618_pqam_dsm_distance_tmp"

combine = {1,2,4}
frequency = {4000, 2000}
bit_per_symbol = {2, 4, 8}
packet_size = 128  -- bytes
packet_cnt = 100  -- 128*8*10 = 10kbit
-- packet_cnt = 1

distance = 1.8

remote_host = "192.168.0.127"
timeout = 20.0

-- set tag tx to high-voltage mode
assert(rt.proxy_run([[
	tag_set_en9(1)
	tag_set_pwsel(1)
]], "", remote_host, timeout), "proxy run failed, terminate")
-- reader auto gain control
-- logln("gain is: " .. reader_gain_control(0.2))
-- sleepms(100)

for k=1,packet_cnt do
	logln("[" .. k .. "/" .. packet_cnt .. "]")
	for i=1,#frequency do
	-- for i=1,1 do
		for j=1,#combine do
		-- for j=1,1 do
			for l=1,#bit_per_symbol do
			-- for l=1,1 do
				-- first create document contain the parameters
				id = mongo_create_one_with_jsonstr(collection, "{}")
				local tab = rt.get_file_by_id(collection, id)
				-- logln("record file is [" .. collection .. ":" .. id .. "]")
				-- logln("" .. i .. ", " .. j .. ", " .. l)
				tab.NLCD = 8
				tab.ct_fast = 8
				tab.ct_slow = 64
				tab.combine = combine[j]
				tab.cycle = 16
				tab.duty = 2
				tab.bit_per_symbol = bit_per_symbol[l]
				tab.data = generate_random_data(packet_size)
				tab.frequency = {} tab.frequency["$numberDouble"] = "" .. frequency[i]
				tab.distance = {} tab.distance["$numberDouble"] = "" .. distance
				rt.save_file(tab)

				-- then call program to modulate data
				run("Tester/HandleData/HD_FastDSM_Modulate", collection, id)
				tab = rt.get_file_by_id(collection, id)

				-- -- start recording data and then start tag data sending process
				local record_id = set_record(1)
				assert(rt.proxy_run([[
					tag_send("]] .. collection .. [[", "]] .. id .. [[", "frequency", "o_out")
				]], "", remote_host, timeout), "proxy run failed, terminate")
				sleepms(100)
				set_record(0)
				rt.reader.plot_rx_data(record_id)
				tab = rt.get_file_by_id(collection, id)
				tab.record_id = record_id
				rt.save_file(tab)

			end
		end
	end
end
