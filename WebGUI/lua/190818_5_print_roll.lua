-- you can click buttons below to select a demo code
-- or click above to use library code

collection = "190817_5_yaw_roll_test"
-- roll experiment use 7m, 8m, 9m
ids = list_all_documents_id(collection, [[{"position": {"$gt":500}}]])

rolls = {}

for i=1,#ids do
    tab = rt.get_file_by_id(collection, ids[i])
    roll = tonumber(tab.roll["$numberInt"])
    if rolls[roll] then
        rolls[roll] = rolls[roll] + 1
    else 
        rolls[roll] = 1
    end
end

roll_sort = {}
for roll,count in pairs(rolls) do
    roll_sort[#roll_sort+1] = roll
end
table.sort(roll_sort)
for i=1,#roll_sort do
    roll = roll_sort[i]
    logln("" .. roll .. ": " .. rolls[roll])
end
