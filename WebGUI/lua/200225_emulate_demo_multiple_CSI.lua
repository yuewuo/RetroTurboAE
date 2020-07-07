--
-- This script demonstrates how to emulate the received signal given multiple tags and multiple readers
-- 
-- Here is some definitions:
--     CSI: channel state information
--          given `N` tags and `M` readers, the CSI is a N * M matrix (csi[N][M]) of complex<float>
--          since there is no complex<float> in mongo database, we use an array of two double value instead, [I, Q]
--     frequency: the frequency of tag tx sending. note that it must be a factor of 80000.
--     sequences: `N` sequences of tag tx. each squence is an array of compressed string
--     NLCDs: an array that contains `N` NLCD value for each tag
--     refs_file: the filename of 17th order m-sequence at 80kS/s and 0.5ms interval. the file should be exactly 41,943,040 bytes
--

collection = "200225_emulate_demo_multiple_CSI"
N_tag = 3  -- DO NOT CHANGE THIS
M_reader = 2  -- DO NOT CHANGE THIS
refs_file = "ref17mseq8kSs.9v.bin"

function complex(I, Q)
    local tab = {}
    tab[#tab + 1] = {}
    tab[#tab + 1] = {}
    tab[1]["$numberDouble"] = ""..I
    tab[2]["$numberDouble"] = ""..Q
    return tab
end

-- here I write a csi matrix just for testing (you should compute the csi based on physical model)
csi = {
    { complex(1.0, 2.0), complex(2.0, 1.0) },
    { complex(0.3, 0.3), complex(0.4, 0.2) },
    { complex(10.0, 0.0), complex(0.0, 10.0) }
}

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
    }, { "00:1" }, { "00:1" }
}

local id = mongo_create_one_with_jsonstr(collection, "{}")
logln("file id: " .. id)
local tab = rt.get_file_by_id(collection, id)
tab.refs_file = refs_file
tab.csi = csi
tab.sequences = sequences
tab.NLCDs = NLCDs
tab.frequency = {} tab.frequency["$numberDouble"] = ""..frequency
rt.save_file(tab)

-- run the program
run("Tester/Emulation/EM_MongoEmulate17", collection, id)
tab = rt.get_file_by_id(collection, id)  -- fetch the updated file

-- plot the emulated data
for i=1,M_reader do
    plot(tab.emulated_ids[i])
end
