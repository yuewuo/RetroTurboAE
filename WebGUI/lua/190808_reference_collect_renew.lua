
collection = "190808_reference_collect_renew"

NLCD = 16
base_frequencys = {125., 250.,}
cycle = 32
dutys = {2,4,6,8,12,16}
effect_length = 3
repeat_cnt = 100  -- noise is too large, have to do this

preamble_id = "5D4C8B5EA96A0000AF005832"
logln("loading preamble reference")
load_preamble_ref(preamble_id)

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
		frequency = base_frequency * cycle
		logln("testing duty: " .. duty .. "/" .. cycle .. " , base_frequency: " .. base_frequency)

		local id = mongo_create_one_with_jsonstr(collection, "{}")
        local tab = rt.get_file_by_id(collection, id)
        tab.NLCD = NLCD
		tab.frequency = {} tab.frequency["$numberDouble"] = "" .. frequency
		tab.cycle = cycle
        tab.duty = duty
        tab.effect_length = effect_length
        tab.repeat_cnt = repeat_cnt
        rt.save_file(tab)
        
        run("Tester/ExploreLCD/EL_AutoGenRef8421_BuildSequence", collection, id)

		for k=1,NLCD do
		-- for k=1,1 do
			-- send the sequence and record it

			for x=2*k-2,2*k-1 do
				run("Tester/ExploreLCD/EL_General_Repeat_Record_Gen", collection, id, "base_"..x)
				tab = rt.get_file_by_id(collection, id)
				o_final_sequence_length = tonumber(tab.o_final_sequence_length["$numberInt"])
				reader_start_preamble(math.ceil(o_final_sequence_length / frequency * 80000 + 0.05 * 80000), 20)
				sleepms(200)
				tag_send(collection, id, "frequency", "o_final_sequence")
				has = reader_wait_preamble(3)  -- wait for 1s
				reader_stop_preamble()
				if has then
					data_id = reader_save_preamble()
					-- rt.reader.plot_rx_data(data_id)
					logln("data_id: " .. data_id)
					tab = rt.get_file_by_id(collection, id)
					tab["data_id_"..x] = data_id
					rt.save_file(tab)
				else 
					logln("preamble failed")
				end

			end

		end
	end
end
