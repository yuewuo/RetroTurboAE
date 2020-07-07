
tab, id = rt.create_tmp_file()

tab.frequency = {} tab.frequency["$numberDouble"] = "2."
blink_array = {
	"0000000000000000:1",
	"FFFFFFFFFFFFFFFF:1",
	"0000000000000000:1",
	"FFFFFFFFFFFFFFFF:1",
	"0000000000000000:1",
	"FFFFFFFFFFFFFFFF:1",
	"0000000000000000:1",
	"FFFFFFFFFFFFFFFF:1",
} setmetatable(blink_array , cjson.array_mt)
tab.blink_array = blink_array
rt.save_file(tab)

logln(id)

run_remote = true
remote_host = "192.168.0.127"
timeout = 10.0

if run_local then
	count, real_frequency = tag_send("tmp", id, "frequency", "blink_array")
else
	ret = rt.proxy_run([[
		tag_send("tmp", "]] .. id .. [[", "frequency", "blink_array")
	]], "", remote_host, timeout)
	assert(ret, "tag send failed")
end
