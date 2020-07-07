--
-- This script demonstrates how to emulate the received signal given multiple tags and multiple readers
-- 
-- Here is some definitions:
--     frequency: the frequency of tag tx sending. note that it must be a factor of 80000.
--     sequences: `N` sequences of tag tx. each squence is an array of compressed string
--     NLCDs: an array that contains `N` NLCD value for each tag
--     refs_file: the filename of 17th order m-sequence at 80kS/s and 0.5ms interval. the file should be exactly 41,943,040 bytes
--

collection = "200307_emulate_MIMO"
N_tag = 3  -- DO NOT CHANGE THIS
M_reader = 2  -- DO NOT CHANGE THIS
refs_file = "ref17mseq8kSs.9v.bin"

function coordinate(x, y, z)
    local tab = {}
    tab[#tab + 1] = {}
    tab[#tab + 1] = {}
    tab[#tab + 1] = {}
    tab[1]["$numberDouble"] = ""..x
    tab[2]["$numberDouble"] = ""..y
    tab[3]["$numberDouble"] = ""..z
    return tab
end

tag_location = {
    { coordinate(1.0, 2.0, 3.0), coordinate(29.0, 10.0, 10.0) },
    { coordinate(2.0, 3.0, 3.6), coordinate(24.0, 3.0, 2.0) },
    { coordinate(3.0, 4.0, 1.0), coordinate(26.0, 4.0, 1.0) }
}
tag_change_time = {0, 40}
S_location = 2

-- each tag has only one PQAM8:4:2:1 pixel group
NLCDs = { 1, 1, 1 }

-- only the first tag is sending, others send nothing
frequency = 8000
sequences = {
    {
        "00:32", 
        "FF:32", 
        "00:16", 
        "FF:16", 
        "00:64"
    }, 
  	{ 
    	"00:64", 
    	"FF:32" 
  	}, 
  	{ 
    	"00:64", 
    	"FF:16",
    	"0F:16"
  	}
}

fov = 160.0
const_dist = 500.0

local id = mongo_create_one_with_jsonstr(collection, "{}")
logln("file id: " .. id)
local tab = rt.get_file_by_id(collection, id)
tab.refs_file = refs_file
tab.sequences = sequences
tab.NLCDs = NLCDs
tab.frequency = {} tab.frequency["$numberDouble"] = ""..frequency
tab.N_tag = N_tag
tab.M_reader = M_reader
tab.S_location = S_location
tab.tag_location = tag_location
tab.tag_change_time = tag_change_time
tab.fov = {} tab.fov["$numberDouble"] = ""..fov
tab.const_dist = {} tab.const_dist["$numberDouble"] = ""..const_dist
rt.save_file(tab)

-- run the program
run("Tester/Emulation/EM_MongoEmulateMIMO", collection, id)
tab = rt.get_file_by_id(collection, id)  -- fetch the updated file

-- plot the emulated data
for i=1,M_reader*2 do
    plot(tab.emulated_ids[i])
end
