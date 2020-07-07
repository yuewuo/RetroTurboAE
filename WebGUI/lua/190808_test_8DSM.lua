--[[
refer to paper: basic DSM

16 LCDs, divide into 6 groups, each has 2 LCD

frequency is 1kS/s

12ms + 6ms = 18ms total

--]]

-- collection = "tmp"
collection = "190808_test_8DSM"
preamble_id = "5D4C8B5EA96A0000AF005832"
frequency = 1000

logln("loading preamble reference")
load_preamble_ref(preamble_id)

-- set tag tx to high-voltage mode
tag_set_en9(1)
tag_set_pwsel(1)

-- reader auto gain control
logln("gain is: " .. reader_gain_control(0.2))
sleepms(100)

function run_experiment(name, repeat_cnt, base_sequence)
    local id = mongo_create_one_with_jsonstr(collection, "{}")
    local tab = rt.get_file_by_id(collection, id)
    logln("name: " .. name .. ", id: " .. id)
    tab.name = name
    tab.NLCD = 16
    tab.frequency = {} tab.frequency["$numberDouble"] = "" .. frequency  -- 250H is the minimum for preamble
    tab.repeat_cnt = repeat_cnt
    setmetatable(base_sequence , cjson.array_mt)
    tab.base_sequence = base_sequence
    rt.save_file(tab)
    run("Tester/ExploreLCD/EL_General_Repeat_Record_Gen", collection, id)
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
        tab.data_id = data_id
        rt.save_file(tab)
    else 
        logln("preamble failed")
    end
end

function add_bit(tab, bitidx, delay)
    tab[delay] = tab[delay]:sub(1, 4*(9-bitidx)-1) .. "F" .. tab[delay]:sub(4*(9-bitidx)+1)
    tab[delay] = tab[delay]:sub(1, 4*(9-bitidx)-3) .. "F" .. tab[delay]:sub(4*(9-bitidx)-1)
end

function build_seq()
    local tab = {}
    for i=1,18 do tab[i] = "00000000000000000000000000000000" end
    return tab
end

repeat_cnt = 100

base_sequence = build_seq()
add_bit(base_sequence, 1, 1)
add_bit(base_sequence, 1, 7)
run_experiment("1", repeat_cnt, base_sequence)

base_sequence = build_seq()
add_bit(base_sequence, 2, 8)
run_experiment("2", repeat_cnt, base_sequence)

base_sequence = build_seq()
add_bit(base_sequence, 3, 3)
run_experiment("3", repeat_cnt, base_sequence)

base_sequence = build_seq()
add_bit(base_sequence, 4, 4)
add_bit(base_sequence, 4, 10)
run_experiment("4", repeat_cnt, base_sequence)

base_sequence = build_seq()
add_bit(base_sequence, 5, 11)
run_experiment("5", repeat_cnt, base_sequence)

base_sequence = build_seq()
add_bit(base_sequence, 6, 6)
run_experiment("6", repeat_cnt, base_sequence)

base_sequence = build_seq()
add_bit(base_sequence, 1, 1)
add_bit(base_sequence, 1, 7)
add_bit(base_sequence, 2, 8)
add_bit(base_sequence, 3, 3)
add_bit(base_sequence, 4, 4)
add_bit(base_sequence, 4, 10)
add_bit(base_sequence, 5, 11)
add_bit(base_sequence, 6, 6)
run_experiment("all", repeat_cnt, base_sequence)
