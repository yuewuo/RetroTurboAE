collection = "eval_speed_attribute_LCD_new"

ids = list_all_documents_id(collection)
logln("there're " .. #ids .. " records")

for i = 1,#ids do
	-- run("Tester/ExploreLCD/EL_Get_OneBit_StdDev", collection, ids[i])
	tab = rt.get_file_by_id(collection, ids[i])
	-- logln(
	-- 	"frequency: " .. tab["real_frequency"]["$numberDouble"] .. "\n" ..
	-- 	"010: " .. tab["trans_010_stddev"]["$numberDouble"] .. "\n" ..
	-- 	"101: " .. tab["trans_101_stddev"]["$numberDouble"]
	-- ) sleepms(10)

	-- AWGN, 1% BER: AWGN_sigma^2 = (0.44)^2 * stddev = 0.194 * stddev
	sigma_010_sq = tonumber(tab["trans_010_stddev"]["$numberDouble"]) * 0.194
	SNR_010 = - 10 * math.log(sigma_010_sq) / math.log(10)
	sigma_101_sq = tonumber(tab["trans_101_stddev"]["$numberDouble"]) * 0.194
	SNR_101 = - 10 * math.log(sigma_101_sq) / math.log(10)
	logln("" .. tab["real_frequency"]["$numberDouble"] .. " " .. SNR_010 .. " " .. SNR_101)
	may_terminate()  -- this allows user to terminate during execution
end
