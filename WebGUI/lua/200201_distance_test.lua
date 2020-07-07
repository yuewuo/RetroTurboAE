-- you can click buttons below to select a demo code
-- or click above to use library code
this_is_try = true
if this_is_try then
    collection = "tmp"
else
    collection = "200201_distance_test"
end

plot_data = this_is_try
enable_scatter_plot = this_is_try
position = "2.0"  -- this is only used to record, but not any automatic move
big_tag = true
-- big_tag = false
preamble_id = big_tag and "5E35314CA11A000004005F52" or "5E3902EF1508000082002352"

-- remote_host = "192.168.43.88"
remote_host = nil  -- using current turbo host
timeout = 10.0
rpc_port = 52220

NLCD = big_tag and 16 or 8
packet_size = 128  -- bytes
packet_cnt = this_is_try and 1 or 10  -- 128*8*10 = 10kbit
bias = -256

-- frequency = 8000
-- duty = 8
-- cycle = 32
frequency = 2000
duty = 2
cycle = 8

combine = big_tag and 2 or 1
bit_per_symbols = big_tag and {2,4,6} or {2,4}

logln("loading preamble reference")
load_preamble_ref(preamble_id)

-- set tag tx to high-voltage mode
ret = rt.proxy_run([[
    tag_set_en9(1)
    tag_set_pwsel(1)
]], "", remote_host, timeout)
assert(ret, "tag send failed")

-- reader auto gain control
logln("gain is: " .. reader_gain_control(0.2))
sleepms(100)

done_records = {}
function recover_program()
    local done_ids = ""
    for i=1,#done_records do done_ids = done_ids .. "\"" .. done_records[i] .. "\", " end
    return "-- if you think this result is error, you can tag those records with \"terminated: 1\" by running:\n"
    .. "ids = {" .. done_ids .. "}\n"
    .. "for i=1,#ids do\n"
    .. "    local tab = rt.get_file_by_id(\"" .. collection .. "\", ids[i])\n"
    .. "    tab.terminated = 1\n"
    .. "    rt.save_file(tab)\n"
    .. "end\n"
    .. "-- end recover program"
end

-- experiment
for x1=1,#bit_per_symbols do
    -- for x1=1,1 do  -- for debug
    bit_per_symbol = bit_per_symbols[x1]

    for k=1,packet_cnt do

        -- first create document contain the parameters
        id = mongo_create_one_with_jsonstr(collection, "{}")
        local tab = rt.get_file_by_id(collection, id)
        -- logln("record file is [" .. collection .. ":" .. id .. "]")
        -- logln("" .. i .. ", " .. j .. ", " .. l)
        tab.position = position
        tab.NLCD = NLCD
        tab.ct_fast = 0--duty
        tab.ct_slow = 0--2 * cycle
        tab.combine = combine
        tab.cycle = cycle
        tab.duty = duty
        tab.bit_per_symbol = bit_per_symbol
        tab.data = generate_random_data(packet_size)
        tab.frequency = {} tab.frequency["$numberDouble"] = "" .. frequency
        tab.bias = bias
        tab.effect_length = 3
    	tab.channel_training_type = 0
        rt.save_file(tab)

        -- then call program to modulate data
        run("Tester/HandleData/HD_FastDSM_Modulate", collection, id)
        if not big_tag then
            run("Tester/HandleData/HD_ReorderLCD_MobiCom19demo", collection, id, "o_out", "o_reordered")
        end
        tab = rt.get_file_by_id(collection, id)

        o_out_length = tonumber(tab.o_out_length["$numberInt"])
        reader_start_preamble(math.ceil(o_out_length / frequency * 80000 + 0.05 * 80000), 20)
        sleepms(200)
        seq_name = big_tag and "o_out" or "o_reordered"
        ret = rt.proxy_run([[
            tag_send("]]..collection..[[", "]]..id..[[", "frequency", "]]..seq_name..[[")
        ]], "", remote_host, timeout)
        has = reader_wait_preamble(3)  -- wait for 1s
        reader_stop_preamble()

        local BER = 1
        if has then
            data_id = reader_save_preamble()
            if plot_data then
                rt.reader.plot_rx_data(data_id)
            end
            tab = rt.get_file_by_id(collection, id)
            tab.data_id = data_id
            tab.scatter_data_id = data_id
            tab.demod_data_id = data_id
            tab.demod_buffer_length = 64
            tab.demod_nearest_count = 4
            rt.save_file(tab)

            if big_tag then
                if enable_scatter_plot then
                    run("Tester/Emulation/EM_ScatterPlot", collection, id, ""..rpc_port)
                    tab = rt.get_file_by_id(collection, id)
                    rt.reader.plot_rx_data(tab.scatter_output_id)
                end
                run("Tester/Emulation/EM_Demodulate", collection, id, ""..rpc_port)
                tab = rt.get_file_by_id(collection, id)
                BER = tonumber(tab.BER["$numberDouble"])
            else
                -- need to modify
                tab.NLCD = 16
                tab.combine = 2
                rt.save_file(tab)
                run("Tester/Emulation/EM_MobiCom19_Do", collection, id, ""..rpc_port)
                tab = rt.get_file_by_id(collection, id)
                BER = tonumber(tab.BER["$numberDouble"])
                if enable_scatter_plot then
                    plot(tab.scatter_output_id)
                end
            end
        else 
            logln("preamble failed")
        end

        done_records[#done_records+1] = id
        logln("bps("..bit_per_symbol..") [" .. k .. "/" .. packet_cnt .. "]: BER = " .. (BER*100) .. "%")

        may_terminate(recover_program())

	end
end

logln(recover_program())
