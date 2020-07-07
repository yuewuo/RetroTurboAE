
collection = "ArchiveTest_Refined_BR_1000_LEN_100"

ids = list_all_documents_id(collection)
logln("there're " .. #ids .. " records")

record_storage = 0

for i = 1,#ids do
	if (i%10 == 0) then logln("[" .. i .. "/" .. #ids .. "]") end
	run("Tester/Archive/AR_Recover_Original_data_WY_190715", collection, ids[i])
	tab = rt.get_file_by_id(collection, ids[i])
	logln("recovered data: " .. tab["data"])
	may_terminate()  -- this allows user to terminate during execution
end
