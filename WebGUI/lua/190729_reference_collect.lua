
collection = "eval_LCD_DSM_base"

NLCD = 16
base_frequencys = {125., 250.,}
cycle = 32
dutys = {2,4,6,8,12,16}
effect_length = 3
repeat_cnt = 20  -- actual repeat count will not less than this value, but may be more

-- set tag tx to high-voltage mode
tag_set_en9(1)
tag_set_pwsel(1)

-- reader auto gain control
logln("gain is: " .. reader_gain_control(0.2))
sleepms(100)

for i=1,#base_frequencys do
-- for i=1,1 do
    for j=1,#dutys do
    -- for j=2,2 do
        duty = dutys[j]
        base_frequency = base_frequencys[i]
		logln("testing duty: " .. duty .. "/" .. cycle .. " , base_frequency: " .. base_frequency)

		local id = mongo_create_one_with_jsonstr(collection, "{}")
        local tab = rt.get_file_by_id(collection, id)
        tab.NLCD = NLCD
		tab.frequency = {} tab.frequency["$numberDouble"] = "" .. (base_frequency * cycle)
		tab.cycle = cycle
        tab.duty = duty
        tab.effect_length = effect_length
        tab.repeat_cnt = repeat_cnt
        rt.save_file(tab)
        
        run("Tester/ExploreLCD/EL_AutoGenRef8421_BuildSequence", collection, id)

		for k=1,NLCD do
		-- for k=1,1 do
			-- send the sequence and record it
			local record_id = set_record(1)
			sleepms(100)
			count, real_frequency = tag_send(collection, id, "frequency", "ref8421seq_" .. (k-1))
			logln("[" .. k .. "] real_frequency: " .. real_frequency)
			sleepms(100)
			set_record(0)
			logln("record_id: " .. record_id)

			-- rt.reader.plot_rx_data(record_id)  -- only enable this for debug

			-- save record_id
			local tab = rt.get_file_by_id(collection, id)
			tab["record_id_" .. (k-1)] = record_id
			tab.real_frequency = {} tab.real_frequency["$numberDouble"] = "" .. real_frequency
			rt.save_file(tab)
		end
	end
end
