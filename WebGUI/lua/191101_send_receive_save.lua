-- you can click buttons below to select a demo code
-- or click above to use library code

tab, id = rt.create_tmp_file()

tag_set_en9(1)
tag_set_pwsel(0) --use 5V voltage

tab.frequency = {} tab.frequency["$numberDouble"] = "2000."
--fast edge: from 00 to FF
--slow edge: from FF to 00
--normal state: 0000
--warning: LCDs may have a 45 degree polarization misalignment
--    so "00FF00FF" might be proper
blink_array = {
	"00FF00FF:10",
  
  	"00FF00FF:1",
  	"00FF00FF:1",
  	"00FF00FF:1",  
	"00000000:1",
  	"00000000:1",
	"00FF00FF:1",
  	"00FF00FF:1",  
	"00000000:1",
  
  	"00FF00FF:10",
  
  	"00FF00FF:1",
    "00FF00FF:1",
	"00000000:1",
  	"00000000:1",
	"00FF00FF:1",
  	"00FF00FF:1",
  	"00FF00FF:1", 
  	"00000000:1",
  
  	"00FF00FF:10",
  
  	"00000000:1",
  	"00FF00FF:1",
  	"00FF00FF:1",
  	"00000000:1",
  	"00FF00FF:1",  
  	"00000000:1",
  
	"00FF00FF:10",
  
	"00000000:1",
  	"00FF00FF:1",
  	"00000000:1",
  	"00FF00FF:1",
  	"00FF00FF:1", 
  	"00000000:1", 
  
	"00FF00FF:10",
	  
	"00FF00FF:2",
  	"00FF00FF:2",
  	"00FF00FF:2",  
	"00000000:2",
  	"00000000:2",
	"00FF00FF:2",
  	"00FF00FF:2",  
	"00000000:2",
  
  	"00FF00FF:10",
  
  	"00FF00FF:2",
    "00FF00FF:2",
	"00000000:2",
  	"00000000:2",
	"00FF00FF:2",
  	"00FF00FF:2",
  	"00FF00FF:2", 
  	"00000000:2",
  
  	"00FF00FF:10",
  
  	"00000000:2",
  	"00FF00FF:2",
  	"00FF00FF:2",
  	"00000000:2",
  	"00FF00FF:2",  
  	"00000000:2",
  
	"00FF00FF:10",
  
	"00000000:2",
  	"00FF00FF:2",
  	"00000000:2",
  	"00FF00FF:2",
  	"00FF00FF:2", 
  	"00000000:2", 
  
	"00FF00FF:10",
	  
	"00FF00FF:4",
  	"00FF00FF:4",
  	"00FF00FF:4",  
	"00000000:4",
  	"00000000:4",
	"00FF00FF:4",
  	"00FF00FF:4",  
	"00000000:4",
  
  	"00FF00FF:10",
  
  	"00FF00FF:4",
    "00FF00FF:4",
	"00000000:4",
  	"00000000:4",
	"00FF00FF:4",
  	"00FF00FF:4",
  	"00FF00FF:4", 
  	"00000000:4",
  
  	"00FF00FF:10",
  
  	"00000000:4",
  	"00FF00FF:4",
  	"00FF00FF:4",
  	"00000000:4",
  	"00FF00FF:4",  
  	"00000000:4",
  
	"00FF00FF:10",
  
	"00000000:4",
  	"00FF00FF:4",
  	"00000000:4",
  	"00FF00FF:4",
  	"00FF00FF:4", 
  	"00000000:4", 
  
  	"00FF00FF:10",
  
} setmetatable(blink_array , cjson.array_mt)
tab.blink_array = blink_array
rt.save_file(tab)

logln(id)

record_id = set_record(1)
count, real_frequency = tag_send("tmp", id, "frequency", "blink_array")

sleepms(50)
set_record(0)
rt.reader.plot_rx_data(record_id)
logln("record_id: " .. record_id)
run("Tester/DebugTest/DT_GetDataFromMongo", "191101_"..record_id, record_id)
