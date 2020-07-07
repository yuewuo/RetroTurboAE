collection = "tag_repeat"

port = "COM22"  -- this would keep same for tag
bit_per_symbol = 4  -- this should be 2 or 4, which is 4kbps or 8kbps
data = 
[[
<h2>Title</h2>
<p>
    This is a paragraph containing about 100 characters. This is a paragraph containing about 100 characters.
</p>
]]


-- first create document contain the parameters
id = mongo_create_one_with_jsonstr(collection, "{}")
local tab = rt.get_file_by_id(collection, id)
logln("record file is [" .. collection .. ":" .. id .. "]")

hex_data = ""
for i=1,#data do
    hex_data = hex_data .. string.format("%02X", string.byte(data,i))
end

frequency = 2000
tab.NLCD = 8
tab.channel_training_type = 0
tab.ct_fast = 0--duty
tab.ct_slow = 0--2 * cycle
tab.combine = 1
tab.duty = 2
tab.cycle = 8
tab.bit_per_symbol = bit_per_symbol
tab.data = hex_data
tab.frequency = {} tab.frequency["$numberDouble"] = "" .. frequency  -- 4ms / 8 = 0.5ms -> 2000Hz
tab.repeat_NLCD = 4
tab.repeat_count = 86400
tab.repeat_interval = frequency  -- 1s
tab.repeat_frequency = {} tab.repeat_frequency["$numberDouble"] = "" .. frequency
rt.save_file(tab)

-- then call program to modulate data and convert to mobicom'19 ones
run("Tester/HandleData/HD_FastDSM_Modulate", collection, id)
run("Tester/HandleData/HD_ReorderLCD_MobiCom19demo", collection, id, "o_out", "repeat_arr")
run("Tester/HandleData/HD_LoadRepeat_MobiCom19demo", collection, id, port)
