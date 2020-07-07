
collection = "eval_speed_attribute_LCD_single_DSM"

cycle = 16
frequency_start = 125
frequency_end = 500
-- frequency_end = 125  -- only for testing
EVERY_2_DIVIDE = 2
frequency_step = math.exp(math.log(2) * (1/ (EVERY_2_DIVIDE) ))

duty_start = 1
duty_end = 8

MIN_TIME_0 = 16e-3
MIN_TIME_1 = 4e-3

TEST_CNT = 30

-- set tag tx to high-voltage mode
tag_set_en9(1)
tag_set_pwsel(1)

-- reader auto gain control
logln("gain is: " .. reader_gain_control(0.2))
sleepms(100)

frequency = frequency_start
while frequency <= frequency_end + 1 do
	for duty = duty_start,duty_end do
		log("testing duty: " .. duty .. "/" .. cycle .. " , frequency: " .. frequency)
		-- construct the tag send sequence, first add a preamble which is long enough
		local COUNT_0 = math.ceil(MIN_TIME_0 * frequency * cycle)
		local COUNT_1 = math.ceil(MIN_TIME_1 * frequency * cycle)
		local send_sequence = {
			"FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF:" .. COUNT_1,
			"00000000000000000000000000000000:" .. COUNT_0,
		}
		for i=1,TEST_CNT do
			-- send 110100
			table.insert(send_sequence, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF:" .. duty)  -- 1
			table.insert(send_sequence, "00000000000000000000000000000000:" .. (cycle-duty))
			table.insert(send_sequence, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF:" .. duty)  -- 1
			table.insert(send_sequence, "00000000000000000000000000000000:" .. (cycle-duty))
			table.insert(send_sequence, "00000000000000000000000000000000:" .. duty)  -- 0
			table.insert(send_sequence, "00000000000000000000000000000000:" .. (cycle-duty))
			table.insert(send_sequence, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF:" .. duty)  -- 1
			table.insert(send_sequence, "00000000000000000000000000000000:" .. (cycle-duty))
			table.insert(send_sequence, "00000000000000000000000000000000:" .. duty)  -- 0
			table.insert(send_sequence, "00000000000000000000000000000000:" .. (cycle-duty))
			table.insert(send_sequence, "00000000000000000000000000000000:" .. duty)  -- 0
			table.insert(send_sequence, "00000000000000000000000000000000:" .. (cycle-duty))
		end
		setmetatable(send_sequence, cjson.array_mt)
		local id = mongo_create_one_with_jsonstr(collection, "{}")
		local tab = rt.get_file_by_id(collection, id)
		tab.frequency = {} tab.frequency["$numberDouble"] = "" .. (frequency * cycle)
		tab.send_sequence = send_sequence
		tab.count_0 = COUNT_0
		tab.count_1 = COUNT_1
		tab.test_cnt = TEST_CNT
		tab.cycle = cycle
		tab.duty = duty
		rt.save_file(tab)

		-- send the sequence and record it
		local record_id = set_record(1)
		sleepms(100)
		count, real_frequency = tag_send(collection, id, "frequency", "send_sequence")
		logln(", real_frequency: " .. real_frequency)
		sleepms(100)
		set_record(0)

		-- rt.reader.plot_rx_data(record_id)  -- only enable this for debug

		-- save record_id
		local tab = rt.get_file_by_id(collection, id)
		tab.record_id = record_id
		tab.real_frequency = {} tab.real_frequency["$numberDouble"] = "" .. real_frequency
		rt.save_file(tab)

	end
	-- next frequency
	frequency = frequency * frequency_step
end
