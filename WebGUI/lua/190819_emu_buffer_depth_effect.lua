--[[
This program is used to do all emulation results

outter loop is the how many packets needed

take these parameters as variables:

--]]

collection = "190819_emu_buffer_depth_effect"
packet_size = 128  -- bytes
packet_cnt = 1

-- program will find reference in this collection
-- basically generated by
-- "190808_reference_collect_renew.lua"
-- "190809_handle_ref_renew.lua"
-- "190811_auto_gen_refs.lua"
refs_collection = "190808_reference_collect_renew"  

frequencys = {8000}
dutys = {8}
SNR_start_dB = 20  -- 0.1
SNR_end_dB = 35  -- 0.001
SNR_step_dB = 1
combines = {2}
bit_per_symbols = {4}
demod_buffer_lengths = {1,4,16,64,256,1024,4096}
function db2value(db)
    return 10 ^ (-db/20.)
end

-- first find the reference id
refs_ids = {}
for i=1,#frequencys do
    refs_ids[i] = {}
    for j=1, #dutys do
        refs_id = cjson.decode(mongo_get_one_as_jsonstr(refs_collection
            , '{"frequency":{"$numberDouble":"'..frequencys[i]..'"},"duty":'..dutys[j]..'}'
            , '{"limit": 1}'
        )).o_refs_id
        refs_ids[i][j] = refs_id
    end
end
logln("all refs found successfully")

for i=1,packet_cnt do
    -- for all different settings, use the same data the emulate
    local data = generate_random_data(packet_size)
    
    local SNR_dB = SNR_start_dB
    while SNR_dB <= SNR_end_dB do  -- SNR loop
        noise = db2value(SNR_dB)
        SNR_dB = SNR_dB + SNR_step_dB
        logln("["..i.."], SNR: " .. SNR_dB .. "dB")

        for x1=1,#frequencys do  -- frequency loop
            frequency = frequencys[x1]

            for x2=1,#dutys do -- duty loop
                duty = dutys[x2]

                for x3=1,#combines do -- combine loop
                    combine = combines[x3]

                    for x4=1,#bit_per_symbols do -- bit_per_symbol loop
                        bit_per_symbol = bit_per_symbols[x4]

                        -- first create document contain the parameters
                        local id = mongo_create_one_with_jsonstr(collection, "{}")
                        local tab = rt.get_file_by_id(collection, id)
                        -- logln("record file is [" .. collection .. ":" .. id .. "]")
                        tab.SNR_dB = SNR_dB
                        tab.NLCD = 16
                        tab.ct_fast = 0
                        tab.ct_slow = 0
                        tab.channel_training_type = 1  -- use default no channel training
                        tab.combine = combine
                        tab.cycle = 32
                        tab.duty = duty
                        tab.bit_per_symbol = bit_per_symbol
                        tab.data = data
                        tab.frequency = {} tab.frequency["$numberDouble"] = ""..frequency
                        tab.bias = -256
                        tab.effect_length = 3
                        tab.refs_id = refs_ids[x1][x2]
                        tab.noise = {} tab.noise["$numberDouble"] = "" .. noise
                        rt.save_file(tab)
                        run("Tester/Emulation/EM_MongoEmulate", collection, id)
                        -- then demodualte
                        tab = rt.get_file_by_id(collection, id)
                        tab.demod_data_id = tab.emulated_id
                        tab.demod_buffer_length = 4
                        tab.demod_nearest_count = 4
                        rt.save_file(tab)

                        for x5=1,#demod_buffer_lengths do
                            demod_buffer_length = demod_buffer_lengths[x5]
                            tab.demod_buffer_length = demod_buffer_length
                            tab.BER_output_key = "BER_"..demod_buffer_length
                            rt.save_file(tab)

                            run("Tester/Emulation/EM_Demodulate", collection, id)
                            tab = rt.get_file_by_id(collection, id)
                            local BER = tonumber(tab["BER_"..demod_buffer_length]["$numberDouble"])
                            logln("demod_buffer_length("..demod_buffer_length.."), BER: " .. BER)
                        end
                        
                    -- to save space, the emulated data is of no use at all, so just delete them
                    -- run("Tester/DebugTest/DT_DeleteGridfsFile", tab.emulated_id)
                    end
                end
            end
        end
    end

    may_terminate()  -- to allow terminate in process
end
