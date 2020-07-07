
collection = "eval_speed_attribute_LCD_new"

frequency_start = 125
frequency_end = 16000
-- frequency_end = 125  -- only for testing
EVERY_2_DIVIDE = 3
frequency_step = math.exp(math.log(2) * (1/ (EVERY_2_DIVIDE) ))

MIN_TIME_0 = 6e-3
MIN_TIME_1 = 2e-3

TEST_CNT = 30


-- set tag tx to high-voltage mode
tag_set_en9(1)
tag_set_pwsel(1)

-- reader auto gain control
logln("gain is: " .. reader_gain_control(0.2))
sleepms(100)

frequency = frequency_start
while frequency <= frequency_end + 1 do
	log("testing frequency: " .. frequency)
	-- construct the tag send sequence, first add a preamble which is long enough
	local COUNT_0 = math.ceil(MIN_TIME_0 * frequency)
	local COUNT_1 = math.ceil(MIN_TIME_1 * frequency)
	local send_sequence = {
		"FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF:" .. COUNT_1,
		"00000000000000000000000000000000:" .. COUNT_0,
	}
	for i=1,TEST_CNT do
		-- first test 010 then test 101
		table.insert(send_sequence, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF:1")
		table.insert(send_sequence, "00000000000000000000000000000000:" .. COUNT_0)
		table.insert(send_sequence, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF:" .. COUNT_1)
		table.insert(send_sequence, "00000000000000000000000000000000:1")
		table.insert(send_sequence, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF:" .. COUNT_1)
		table.insert(send_sequence, "00000000000000000000000000000000:" .. COUNT_0)
	end
	setmetatable(send_sequence, cjson.array_mt)
	local id = mongo_create_one_with_jsonstr(collection, "{}")
	local tab = rt.get_file_by_id(collection, id)
	tab.frequency = {} tab.frequency["$numberDouble"] = "" .. frequency
	tab.send_sequence = send_sequence
	tab.count_0 = COUNT_0
	tab.count_1 = COUNT_1
	tab.test_cnt = TEST_CNT
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

	-- next frequency
	frequency = frequency * frequency_step
end
