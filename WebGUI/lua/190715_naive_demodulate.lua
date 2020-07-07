
refs_id = "5D2D02CD7461000012003902"
effect_length = 4
collection = "ArchiveTest_Refined_BR_1000_LEN_100"


ids = list_all_documents_id(collection)
logln("there're " .. #ids .. " records")

record_storage = 0

for i = 1,#ids do
	if (i%10 == 0) then logln("[" .. i .. "/" .. #ids .. "]") end
	run("Tester/Archive/AR_Demodulate_WY_190715", refs_id, ""..effect_length, collection, ids[i])
	tab = rt.get_file_by_id(collection, ids[i])
	if tab["BER"] == nil then
		logln("demodulate failed: " .. ids[i])
	else
		logln("BER: " .. tab["BER"]["$numberDouble"])
	end
	may_terminate()  -- this allows user to terminate during execution
end
