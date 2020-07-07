
collection = "tmp"

-- remote_host = "192.168.43.88"
remote_host = nil
timeout = 10.0
filename_suffix = "yaw0_reader0_2002041341"

NLCD = 8
base_frequencys = {250.}--{125., 250.,}
cycle = 32
dutys = {8}--{2,4,6,8,12,16}
effect_length = 3
repeat_cnt = 50  -- actual repeat count will not less than this value, but may be more

-- set tag tx to high-voltage mode
ret = rt.proxy_run([[
    tag_set_en9(1)
    tag_set_pwsel(1)
]], "", remote_host, timeout)
assert(ret, "tag send failed")

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
            run("Tester/HandleData/HD_ReorderLCD_MobiCom19demo", collection, id, "ref8421seq_"..(k-1), "ref8421seq_"..(k-1).."_re")
			local record_id = set_record(1)
            sleepms(100)
            ret = rt.proxy_run([[
                tag_send("]]..collection..[[", "]]..id..[[", "frequency", "ref8421seq_]] .. (k-1) .. [[_re")
            ]], "", remote_host, timeout)
			-- logln("[" .. k .. "]")
			sleepms(100)
			set_record(0)
			logln("record_id: " .. record_id)

			-- rt.reader.plot_rx_data(record_id)  -- only enable this for debug

			-- save record_id
			local tab = rt.get_file_by_id(collection, id)
			tab["record_id_" .. (k-1)] = record_id
            rt.save_file(tab)
            
            run("Tester/ExploreLCD/EL_AutoGenRef8421_HandleOne", collection, id, ""..(k-1))
            local tab = rt.get_file_by_id(collection, id)
            plot(tab["ref_id_"..(k-1)])
            may_terminate()
        end

        run("Tester/ExploreLCD/EL_AutoGenRef8421_Combine", collection, id, "refs_id", 
            "ref_id_0", "ref_id_1", "ref_id_2", "ref_id_3",
            "ref_id_4", "ref_id_5", "ref_id_6", "ref_id_7"
        )
        tab = rt.get_file_by_id(collection, id)
        frequency = math.modf(tonumber(tab.frequency["$numberDouble"]))
        filename = "refs8x2_"..frequency.."_"..duty.."_"..filename_suffix..".bin"
        run("Tester/DebugTest/DT_GetDataFromMongo", filename, tab["refs_id"])
        logln("refs id: " .. tab["refs_id"] .. " (cannot plot in WebGUI, no metadata available)")
        logln("file saved as: " .. filename)

	end
end
