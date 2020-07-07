-- you can click buttons below to select a demo code
-- or click above to use library code

collection = "200423_record_comm"

tag_set_en9(1)
tag_set_pwsel(1) --use 9V voltage

cycle = 32
bias = -256
duty = 8

NLCD = 8
frequency = 8000
combines = {1, 2}
bit_per_symbols = {2,4,6,8}

for x1=1,#bit_per_symbols do
    bit_per_symbol = bit_per_symbols[x1]

    for x2=1,#combines do
        combine = combines[x2]
        -- first create document contain the parameters
        id = mongo_create_one_with_jsonstr(collection, "{}")
        local tab = rt.get_file_by_id(collection, id)
        -- logln("record file is [" .. collection .. ":" .. id .. "]")
        -- logln("" .. i .. ", " .. j .. ", " .. l)
        tab.NLCD = NLCD
        tab.combine = combine
        tab.cycle = cycle
        tab.duty = duty
        tab.bit_per_symbol = bit_per_symbol
        tab.data = "0123456789abcdef"
        tab.frequency = {} tab.frequency["$numberDouble"] = "" .. frequency
        tab.bias = bias
        tab.channel_training_type = 0
        tab.ct_fast = 0 --duty
        tab.ct_slow = 0 --2 * cycle
        rt.save_file(tab)

        -- then call program to modulate data
        run("Tester/HandleData/HD_FastDSM_Modulate", collection, id)
        -- we are using small tag
        run("Tester/HandleData/HD_ReorderLCD_MobiCom19demo", collection, id, "o_out", "o_reordered")
        tab = rt.get_file_by_id(collection, id)
        
        record_id = set_record(1)
        tag_send(collection, id, "frequency", "o_reordered")
        sleepms(50)
        set_record(0)
        tab.record_id = record_id
        rt.save_file(tab)

        rt.reader.plot_rx_data(record_id)
        logln("record_id: " .. record_id)
        run("Tester/DebugTest/DT_GetDataFromMongo", ".\\record_200423\\pd_no_tag_no\\combine_"..combine.."_bpsym_"..bit_per_symbol, record_id)

	end
end

