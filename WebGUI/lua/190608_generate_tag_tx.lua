-- you can click buttons below to select a demo code
-- or click above to use library code

-- first create samples to send
collection = "test190608"
id = mongo_create_one_with_jsonstr(collection, "{}")
tab = rt.get_file_by_id(collection, id)
logln("record file is [" .. collection .. ":" .. id .. "]")
tab.NLCD = 1
tab.ct_fast = 0
tab.ct_slow = 0
tab.combine = 1
tab.cycle = 32
tab.duty = 4
tab.bit_per_symbol = 4
tab.data = "00"
tab.frequency = {} tab.frequency["$numberDouble"] = "8000.0"
rt.save_file(tab)
run("Tester/HandleData/HD_FastDSM_Modulate", collection, id)
tab = rt.get_file_by_id(collection, id)
logln("throughput is " .. tab.o_throughput["$numberDouble"] .. " bps")
