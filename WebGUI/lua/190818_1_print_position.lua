-- you can click buttons below to select a demo code
-- or click above to use library code

collection = "190817_3_distance_eval"
ids = list_all_documents_id(collection, [[{}]])

positions = {}

for i=1,#ids do
    tab = rt.get_file_by_id(collection, ids[i])
    position = tonumber(tab.position["$numberInt"])
    if positions[position] then
        positions[position] = positions[position] + 1
    else 
        positions[position] = 1
    end
end

position_sort = {}
for position,count in pairs(positions) do
    position_sort[#position_sort+1] = position
end
table.sort(position_sort)
for i=1,#position_sort do
    position = position_sort[i]
    logln("" .. position .. ": " .. positions[position])
end
