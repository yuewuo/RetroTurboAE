collection = "eval_speed_attribute_LCD_single_DSM"

cycle = 16
frequency_start = 125
frequency_end = 500
-- frequency_end = 125  -- only for testing
EVERY_2_DIVIDE = 2
frequency_step = math.exp(math.log(2) * (1/ (EVERY_2_DIVIDE) ))

duty_start = 1
duty_end = 8

frequency = frequency_start
stddevs = {}
frequencys = {}
while frequency <= frequency_end + 1 do
	local duty_max_stddevs = {}
	local middle_stddevs = {}
	local real_frequency = 0
	for duty = duty_start,duty_end do
		-- logln("testing duty: " .. duty .. "/" .. cycle .. " , frequency: " .. frequency)

		local ids = list_all_documents_id(collection,  [[{"frequency":]] .. (cycle * frequency) .. [[, "duty": ]] .. duty .. [[}]])
		-- logln("there're " .. #ids .. " records")
		if #ids ~= 1 then
			error("should find one document")
		end
		id = ids[1]

		-- run("Tester/ExploreLCD/EL_Get_DSM_StdDev", collection, id)
		tab = rt.get_file_by_id(collection, id)
		real_frequency = tab["real_frequency"]["$numberDouble"]

		-- output minimum stddev of 001, 011, 101, for each throughput
		local min_stddevs = {}
		for i = 1,5 do
			local trans_001_stddev = tab["trans_001_stddev"][i]["$numberDouble"]
			local trans_011_stddev = tab["trans_011_stddev"][i]["$numberDouble"]
			local trans_101_stddev = tab["trans_101_stddev"][i]["$numberDouble"]
			-- logln("" .. trans_001_stddev .. ", " .. trans_011_stddev .. ", " .. trans_101_stddev)
			local trans_min_stddev = math.min(trans_001_stddev, trans_011_stddev, trans_101_stddev)
			table.insert(min_stddevs, trans_min_stddev)
		end
		table.insert(middle_stddevs, min_stddevs)

		-- error("debug")


		-- 	-- AWGN, 1% BER: AWGN_sigma^2 = (0.44)^2 * stddev = 0.194 * stddev
		-- 	-- sigma_010_sq = tonumber(tab["trans_010_stddev"]["$numberDouble"]) * 0.194
		-- 	-- SNR_010 = - 10 * math.log(sigma_010_sq) / math.log(10)
		-- 	-- sigma_101_sq = tonumber(tab["trans_101_stddev"]["$numberDouble"]) * 0.194
		-- 	-- SNR_101 = - 10 * math.log(sigma_101_sq) / math.log(10)
		-- 	-- logln("" .. tab["real_frequency"]["$numberDouble"] .. " " .. SNR_010 .. " " .. SNR_101)
		may_terminate()  -- this allows user to terminate during execution
	end
	-- search all duty for the maximum stddev
	for i = 1,5 do
		local max_val = middle_stddevs[1][i]
		for j = 1,(duty_end-duty_start+1) do
			if middle_stddevs[j][i] > max_val then
				max_val = middle_stddevs[j][i]
			end
		end
		table.insert(duty_max_stddevs, max_val)
		-- log("" .. max_val .. ", ")
	end -- logln("")
	table.insert(stddevs, duty_max_stddevs)
	table.insert(frequencys, real_frequency)

	-- next frequency
	frequency = frequency * frequency_step
end

logln("[" .. #stddevs .. "][" .. #stddevs[1] .. "]")

-- finally, choose frequency [2,4] and points [2,5] to draw
for i = 1,5 do
	freq = frequencys[i]
	logln("freq: " .. frequencys[i])
	for j=1,5 do
		throughput = freq / 16 * ( 2 ^ (j-1) )
		SNR = - 10 * math.log(stddevs[i][j] * 0.194) / math.log(10)
		logln("" .. throughput .. " " .. SNR)
	end
end
