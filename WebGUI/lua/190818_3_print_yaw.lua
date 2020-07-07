-- you can click buttons below to select a demo code
-- or click above to use library code

collection = "190817_5_yaw_roll_test"
-- yaw experiment only use 4m
ids = list_all_documents_id(collection, [[{"position": 400}]])

yaws = {}

for i=1,#ids do
    tab = rt.get_file_by_id(collection, ids[i])
    yaw = tonumber(tab.yaw["$numberInt"])
    if yaws[yaw] then
        yaws[yaw] = yaws[yaw] + 1
    else 
        yaws[yaw] = 1
    end
end

yaw_sort = {}
for yaw,count in pairs(yaws) do
    yaw_sort[#yaw_sort+1] = yaw
end
table.sort(yaw_sort)
for i=1,#yaw_sort do
    yaw = yaw_sort[i]
    logln("" .. yaw .. ": " .. yaws[yaw])
end
