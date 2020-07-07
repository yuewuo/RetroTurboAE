-- only collect duty=8,freq=8000,
-- copy from "190808_reference_collect_renew.lua"
-- "190809_handle_ref_renew.lua"

collection = "190815_xkn"
duty = 8
frequency = 8000


-- do not change this
NLCD = 16
cycle = 32
-- START "190808_reference_collect_renew.lua"

effect_length = 3
repeat_cnt = 100  -- noise is too large, have to do this

preamble_id = "5D54B44C0B4E00007B000C22"
logln("loading preamble reference")
load_preamble_ref(preamble_id)

-- set tag tx to high-voltage mode
tag_set_en9(1)
tag_set_pwsel(1)

-- reader auto gain control
logln("gain is: " .. reader_gain_control(0.2))
sleepms(100)

base_frequency = 250
frequency = base_frequency * cycle
logln("testing duty: " .. duty .. "/" .. cycle .. " , base_frequency: " .. base_frequency)

local id = mongo_create_one_with_jsonstr(collection, "{}")
local tab = rt.get_file_by_id(collection, id)
tab.NLCD = 16
tab.frequency = {} tab.frequency["$numberDouble"] = "" .. frequency
tab.cycle = 32
tab.duty = duty
tab.effect_length = effect_length
tab.repeat_cnt = repeat_cnt
rt.save_file(tab)

run("Tester/ExploreLCD/EL_AutoGenRef8421_BuildSequence", collection, id)

for k=1,NLCD do
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

-- END "190808_reference_collect_renew.lua"

-- START "190809_handle_ref_renew.lua"
data_id_keys = {}
for i=1,32 do data_id_keys[i] = "data_id_" .. (i-1) end
base_sequence_key = "base_0"
bias = -256  -- this is fine tuned for specific preamble, just change this by plotting
-- *** with positive bias, the curve is moving left ***
for j=1,#data_id_keys do
    run("Tester/ExploreLCD/EL_General_Repeat_Record_Get", collection, id, ""..bias, data_id_keys[j], base_sequence_key)
end
-- END "190809_handle_ref_renew.lua"

run("Tester/Emulation/EM_AutoGenRefFile", collection, id)
local tab = rt.get_file_by_id(collection, id)
local o_refs_id = tab.o_refs_id
local meta = rt.get_metadata(o_refs_id)
NLCD = meta.NLCD["$numberInt"]
duty = meta.duty["$numberInt"]
cycle = meta.cycle["$numberInt"]
frequency = math.floor(meta.frequency["$numberDouble"] + 0.5)
effect_length = meta.effect_length["$numberInt"]
filename = "turborefs_" .. NLCD .. "_" .. duty .. "_" .. cycle .. "_" .. frequency .. "_" .. effect_length .. ".bin"
logln("save reference to file: " .. filename)
run("Tester/DebugTest/DT_GetDataFromMongo", filename, o_refs_id)

logln("")
logln("refs_id: " .. o_refs_id)
logln("you can copy this to \"190814_debug_real_demod.lua\"")

rt.reader.plot_rx_data(o_refs_id)
