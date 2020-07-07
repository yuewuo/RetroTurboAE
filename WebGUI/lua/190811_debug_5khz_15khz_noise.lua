-- send 0 sequence after preamble

collection = "tmp"
preamble_id = "5D4C8B5EA96A0000AF005832"
frequency = 250
time_s = 1

-- set tag tx to high-voltage mode
tag_set_en9(1)
tag_set_pwsel(1)

-- reader auto gain control
logln("gain is: " .. reader_gain_control(0.2))
sleepms(100)

local id = mongo_create_one_with_jsonstr(collection, "{}")
local tab = rt.get_file_by_id(collection, id)
logln("id: " .. id)
tab.NLCD = 16
tab.frequency = {} tab.frequency["$numberDouble"] = "" .. frequency  -- 250H is the minimum for preamble
tab.repeat_cnt = math.ceil(time_s * frequency)

base_sequence = {
	"00000000000000000000000000000000:1",
} setmetatable(base_sequence , cjson.array_mt)
tab.base_sequence = base_sequence

rt.save_file(tab)

run("Tester/ExploreLCD/EL_General_Repeat_Record_Gen", collection, id)
tab = rt.get_file_by_id(collection, id)

logln("loading preamble reference")
load_preamble_ref(preamble_id)

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
