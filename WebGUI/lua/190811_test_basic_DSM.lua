--[[
refer to paper: basic DSM

16 LCDs, divide into 3 groups (only use 12 half LCDs)

case 1: (1000S/s) x100
1) pixel.0
0000000000000000000000000F0F0F0F:4
00000000000000000000000000000000:SLOW
2) pixel.1
00000000000000000000000000000000:1
00000000000000000F0F0F0F00000000:3
00000000000000000000000000000000:SLOW
3) pixel.2
00000000000000000000000000000000:2
000000000F0F0F0F0000000000000000:2
00000000000000000000000000000000:SLOW

case 2: (1000S/s) x100
1) 111
0000000000000000000000000F0F0F0F:1
00000000000000000F0F0F0F0F0F0F0F:1
000000000F0F0F0F0F0F0F0F0F0F0F0F:2
00000000000000000000000000000000:SLOW
2) 110
0000000000000000000000000F0F0F0F:1
00000000000000000F0F0F0F0F0F0F0F:3
00000000000000000000000000000000:SLOW
3) 101
0000000000000000000000000F0F0F0F:2
000000000F0F0F0F000000000F0F0F0F:2
00000000000000000000000000000000:SLOW
4) 011
00000000000000000000000000000000:1
00000000000000000F0F0F0F00000000:1
000000000F0F0F0F0F0F0F0F00000000:2
00000000000000000000000000000000:SLOW

case 3: (1000S/s) x
send 000
send 001
send 010
send 011
send 100
send 101
send 110
send 111

--]]

SLOW = 5

-- collection = "tmp"
collection = "190811_test_basic_DSM"
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

function get_single(bit)
    if bit == 1 then return "0F0F0F0F" end
    return "00000000"
end

function add_sequence(tab, bit0, bit1, bit2)
    local seq0 = get_single(bit0)
    local seq1 = get_single(bit1)
    local seq2 = get_single(bit2)
    local ret = {}
    tab[#tab+1] = "000000000000000000000000" .. seq0 .. ":1"
    tab[#tab+1] = "0000000000000000" .. seq1 .. seq0 .. ":1"
    tab[#tab+1] = "00000000" .. seq2 .. seq1 .. seq0 .. ":2"
    tab[#tab+1] = "00000000000000000000000000000000:" .. SLOW
end

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

logln("case 1")
local case1_repeat = 100
base_sequence = {} add_sequence(base_sequence, 0, 0, 1)
run_experiment("c1.001", case1_repeat, base_sequence)
base_sequence = {} add_sequence(base_sequence, 0, 1, 0)
run_experiment("c1.010", case1_repeat, base_sequence)
base_sequence = {} add_sequence(base_sequence, 1, 0, 0)
run_experiment("c1.100", case1_repeat, base_sequence)

logln("case 2")
local case2_repeat = 100
base_sequence = {} add_sequence(base_sequence, 1, 1, 1)
run_experiment("c2.111", case2_repeat, base_sequence)
base_sequence = {} add_sequence(base_sequence, 1, 1, 0)
run_experiment("c2.110", case2_repeat, base_sequence)
base_sequence = {} add_sequence(base_sequence, 1, 0, 1)
run_experiment("c2.101", case2_repeat, base_sequence)
base_sequence = {} add_sequence(base_sequence, 0, 1, 1)
run_experiment("c2.011", case2_repeat, base_sequence)

logln("case 3")
local case3_repeat = 20
base_sequence = {}
add_sequence(base_sequence, 0, 0, 0)
add_sequence(base_sequence, 0, 0, 1)
add_sequence(base_sequence, 0, 1, 0)
add_sequence(base_sequence, 0, 1, 1)
add_sequence(base_sequence, 1, 0, 0)
add_sequence(base_sequence, 1, 0, 1)
add_sequence(base_sequence, 1, 1, 0)
add_sequence(base_sequence, 1, 1, 1)
run_experiment("c3.all", case3_repeat, base_sequence)
