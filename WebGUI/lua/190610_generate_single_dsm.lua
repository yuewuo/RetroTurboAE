-- you can click buttons below to select a demo code
-- or click above to use library code

-- first create samples to send
collection = "test190610"
id = mongo_create_one_with_jsonstr(collection, "{}")
tab = rt.get_file_by_id(collection, id)
logln("record file is [" .. collection .. ":" .. id .. "]")
tab.NLCD = 16
tab.ct_fast = 8
tab.ct_slow = 64
tab.combine = 1
tab.cycle = 16
tab.duty = 4
tab.data = "00"
tab.frequency = {} tab.frequency["$numberDouble"] = "8000.0"
rt.save_file(tab)
run("Tester/HandleData/HD_SingleDSM_Modulate", collection, id)
tab = rt.get_file_by_id(collection, id)
logln("throughput is " .. tab.o_throughput["$numberDouble"] .. " bps")
