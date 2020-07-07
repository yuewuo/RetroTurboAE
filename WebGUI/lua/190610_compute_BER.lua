lst = list_all_documents_id("test190610_sdsm")

logln("has " .. #lst .. " documents")

-- for i = 1, #lst do
-- 	logln(i)
-- 	run("Tester/HandleData/HD_SingleDSM_Demodulate", "test190610_sdsm", lst[i])
-- end

BERs = {}
for i = 1, #lst do
	doc = rt.get_file_by_id("test190610_sdsm", lst[i])
	BER = tonumber(doc["BER"]["$numberDouble"])
	logln(i .. ":" .. BER)
	BERs[i] = BER
end
sum = 0
for i = 1, #lst do
	sum = sum + BERs[i]
end
avr = sum / #lst
logln('average BER: ' .. (avr * 100) .. ' %')
stddev = 0
for i = 1, #lst do
	stddev = stddev + (BERs[i] - avr) ^ 2
end
stddev = (stddev / #lst) ^ 0.5
logln('deviation: ' ..  (stddev * 100) .. ' %')

