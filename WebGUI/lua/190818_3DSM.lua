--[[
refer to paper: basic DSM

16 LCDs, divide into 3 groups, each has 4 LCD

frequency is 1kS/s

24ms + 6ms = 30ms total

--]]

-- collection = "tmp"
collection = "190818_3DSM"
preamble_id = "5D5839036A1C8E7647550AE2"
frequency = 1000
bias = -256  -- this is fine tuned for specific preamble, just change this by plotting

remote_host = "192.168.0.127"
timeout = 10.0

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
    ret = rt.proxy_run([[
        tag_send("]]..collection..[[", "]]..id..[[", "frequency", "o_final_sequence")
    ]], "", remote_host, timeout)
    has = reader_wait_preamble(3)  -- wait for 1s
    reader_stop_preamble()
    if has then
        data_id = reader_save_preamble()
        -- rt.reader.plot_rx_data(data_id)
        logln("data_id: " .. data_id)
        tab = rt.get_file_by_id(collection, id)
        tab.data_id = data_id
        rt.save_file(tab)
        run("Tester/ExploreLCD/EL_General_Repeat_Record_Get", collection, id, ""..bias, "data_id", "base_sequence")
        tab = rt.get_file_by_id(collection, id)
        rt.reader.plot_rx_data(tab.o_data_id)
    else 
        logln("preamble failed")
    end
end

function add_bit(tab, bitidx, delay)
    tab[delay] = tab[delay]:sub(1, 8*(5-bitidx)-1) .. "F" .. tab[delay]:sub(8*(5-bitidx)+1)
    tab[delay] = tab[delay]:sub(1, 8*(5-bitidx)-3) .. "F" .. tab[delay]:sub(8*(5-bitidx)-1)
    tab[delay] = tab[delay]:sub(1, 8*(5-bitidx)-5) .. "F" .. tab[delay]:sub(8*(5-bitidx)-3)
    tab[delay] = tab[delay]:sub(1, 8*(5-bitidx)-7) .. "F" .. tab[delay]:sub(8*(5-bitidx)-5)
end

function build_seq()
    local tab = {}
    for i=1,60 do tab[i] = "00000000000000000000000000000000" end
    return tab
end

repeat_cnt = 100

-- case 1: send 000, 001, 010, 011, 100, 101, 110, 111

base_sequence = build_seq()
now_delay = -1
now_bitidx = 2
-- 000
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; --add_bit(base_sequence, now_bitidx, now_delay)
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; --add_bit(base_sequence, now_bitidx, now_delay)
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; --add_bit(base_sequence, now_bitidx, now_delay)
-- 001
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; --add_bit(base_sequence, now_bitidx, now_delay)
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; --add_bit(base_sequence, now_bitidx, now_delay)
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; add_bit(base_sequence, now_bitidx, now_delay)
-- 010
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; --add_bit(base_sequence, now_bitidx, now_delay)
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; add_bit(base_sequence, now_bitidx, now_delay)
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; --add_bit(base_sequence, now_bitidx, now_delay)
-- 011
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; --add_bit(base_sequence, now_bitidx, now_delay)
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; add_bit(base_sequence, now_bitidx, now_delay)
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; add_bit(base_sequence, now_bitidx, now_delay)
-- 100
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; add_bit(base_sequence, now_bitidx, now_delay)
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; --add_bit(base_sequence, now_bitidx, now_delay)
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; --add_bit(base_sequence, now_bitidx, now_delay)
-- 101
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; add_bit(base_sequence, now_bitidx, now_delay)
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; --add_bit(base_sequence, now_bitidx, now_delay)
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; add_bit(base_sequence, now_bitidx, now_delay)
-- 110
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; add_bit(base_sequence, now_bitidx, now_delay)
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; add_bit(base_sequence, now_bitidx, now_delay)
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; --add_bit(base_sequence, now_bitidx, now_delay)
-- 111
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; add_bit(base_sequence, now_bitidx, now_delay)
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; add_bit(base_sequence, now_bitidx, now_delay)
now_delay = now_delay + 2; now_bitidx = ((now_bitidx+1)%3)+1; add_bit(base_sequence, now_bitidx, now_delay)
run_experiment("all", repeat_cnt, base_sequence)
