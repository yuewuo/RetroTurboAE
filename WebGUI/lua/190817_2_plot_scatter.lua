
collection = "190816_4_plot_scatter"
preamble_id = "5D5839036A1C8E7647550AE2"
refs_id = "5D5839BA182ACE64AC1ECCD2"  -- to generate this, refer to "190814_debug_collect_ref.lua"

remote_host = "192.168.0.168"
timeout = 10.0
rpc_port = 52228

NLCD = 16
packet_size = 128  -- bytes
packet_cnt = 1--0  -- 128*8*10 = 10kbit
bias = -256

frequency = 8000
combine = 2
duty = 8
bit_per_symbols = {2}--,4,6}
cycle = 32


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

-- experiment
for k=1,packet_cnt do
    logln("[" .. k .. "/" .. packet_cnt .. "]")

    for x1=1,#bit_per_symbols do
    -- for x1=1,1 do  -- for debug
        bit_per_symbol = bit_per_symbols[x1]

        -- first create document contain the parameters
        id = mongo_create_one_with_jsonstr(collection, "{}")
        local tab = rt.get_file_by_id(collection, id)
        -- logln("record file is [" .. collection .. ":" .. id .. "]")
        -- logln("" .. i .. ", " .. j .. ", " .. l)
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
--     	tab.channel_training_type = 1
        tab.refs_id = refs_id
        -- tab.distance = {} tab.distance["$numberDouble"] = "1.5"
        rt.save_file(tab)

        -- then call program to modulate data
        run("Tester/HandleData/HD_FastDSM_Modulate", collection, id)
        tab = rt.get_file_by_id(collection, id)

        o_out_length = tonumber(tab.o_out_length["$numberInt"])
        reader_start_preamble(math.ceil(o_out_length / frequency * 80000 + 0.05 * 80000), 20)
        sleepms(200)
        ret = rt.proxy_run([[
            tag_send("]]..collection..[[", "]]..id..[[", "frequency", "o_out")
        ]], "", remote_host, timeout)
        has = reader_wait_preamble(3)  -- wait for 1s
        reader_stop_preamble()

        if has then
            data_id = reader_save_preamble()
            rt.reader.plot_rx_data(data_id)
            tab = rt.get_file_by_id(collection, id)
            tab.data_id = data_id
            tab.scatter_data_id = data_id
            tab.demod_data_id = data_id
            tab.demod_buffer_length = 4
            tab.demod_nearest_count = 4
            rt.save_file(tab)
            run("Tester/Emulation/EM_ScatterPlot", collection, id, ""..rpc_port)
            run("Tester/Emulation/EM_Demodulate", collection, id, ""..rpc_port)
            tab = rt.get_file_by_id(collection, id)
            logln("BER: " .. tab.BER["$numberDouble"])
            rt.reader.plot_rx_data(tab.scatter_output_id)
        else 
            logln("preamble failed")
        end

        -- then try to demodulate real time

	end
end
