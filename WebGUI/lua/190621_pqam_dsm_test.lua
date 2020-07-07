
collection = "test190621_pqam_dsm"
-- collection = "test190621_pqam_dsm_tmp"

combine = {1,2,4}
cycle_duty = {1,2}
bit_per_symbol = {2, 4, 6}

NLCD = 16
packet_size = 128  -- bytes
packet_cnt = 10  -- 128*8*10 = 10kbit
-- packet_cnt = 1

-- set tag tx to high-voltage mode
tag_set_en9(1)
tag_set_pwsel(1)

-- reader auto gain control
logln("gain is: " .. reader_gain_control(0.2))
sleepms(100)

-- first get reference
new_preamble = false
preamble_id = "5D0D4F264D3C0000EE0033D2"
if new_preamble then
	preamble_repeat = 50
	local id = mongo_create_one_with_jsonstr(collection, "{}")
	local tab = rt.get_file_by_id(collection, id)
	tab.NLCD = NLCD
	tab.frequency = {} tab.frequency["$numberDouble"] = "4000"
	tab.preamble_repeat = preamble_repeat
	rt.save_file(tab)
	run("Tester/HandleData/HD_PreambleAutoGen", collection, id)
	logln("preamble generated: " .. id)
	-- then record
	local record_id = set_record(1)
	sleepms(100)
	tag_send(collection, id, "frequency", "o_auto_preamble")
	sleepms(100)
	set_record(0)
	-- rt.reader.plot_rx_data(record_id)
	tab = rt.get_file_by_id(collection, id)
	tab.record_id = record_id
	rt.save_file(tab)  -- save record_id to file
	run("Tester/HandleData/HD_PreambleAutoGet", collection, id)
	tab = rt.get_file_by_id(collection, id)
	preamble_id = tab.preamble_id
	logln("preamble_id: " .. preamble_id)
	rt.reader.plot_rx_data(preamble_id)
end
load_preamble_ref(preamble_id)


-- experiment
for k=1,packet_cnt do
	logln("[" .. k .. "/" .. packet_cnt .. "]")
	for i=1,#cycle_duty do
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
				tab.NLCD = NLCD
				tab.ct_fast = 8
				tab.ct_slow = 64
				tab.combine = combine[j]
				tab.cycle = 16 * cycle_duty[i]
				tab.duty = 2 * cycle_duty[i]
				tab.bit_per_symbol = bit_per_symbol[l]
				tab.data = generate_random_data(packet_size)
				tab.frequency = {} tab.frequency["$numberDouble"] = "4000"
				tab.distance = {} tab.distance["$numberDouble"] = "1.5"
				rt.save_file(tab)

				-- then call program to modulate data
				run("Tester/HandleData/HD_FastDSM_Modulate", collection, id)
				tab = rt.get_file_by_id(collection, id)

				o_out_length = tonumber(tab.o_out_length["$numberInt"])
				reader_start_preamble(math.ceil(o_out_length / 4000 * 80000 + 0.05 * 80000), 20)
				sleepms(200)
				tag_send(collection, id, "frequency", "o_out")
				has = reader_wait_preamble(3)  -- wait for 1s
				reader_stop_preamble()

				if has then
					data_id = reader_save_preamble()
					rt.reader.plot_rx_data(data_id)
					tab = rt.get_file_by_id(collection, id)
					tab.data_id = data_id
					rt.save_file(tab)
				else 
					logln("preamble failed")
				end

			end
		end
	end
end
