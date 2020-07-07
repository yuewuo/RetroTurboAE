--[[
this script is to test how "Tester/Emulation/Demodulate.cpp" performance change with "demod_buffer_length"
larger "demod_buffer_length" would therotically be better, however, may leads to higher demodulation time
the time is linearly growing with "demod_buffer_length"
--]]

collection = "190813_emu_how_buffer_depth_effect"
packet_size = 128  -- bytes
refs_id = "5D5232DF286E0000030019A2"  -- this must match with the parameters below, otherwise will fail to emulate

demod_buffer_lengths = {1, 4, 16, 64, 256, 1024}
packet_cnt = 10
SNR_start_dB = 50  -- about 0.003
SNR_end_dB = 34  -- about 0.02, obviously not enough for 64PQAM yet
SNR_step_dB = -1
function db2value(db)
    return 10 ^ (-db/20.)
end

for i=1,#demod_buffer_lengths do
-- for i=1,1 do
    local demod_buffer_length = demod_buffer_lengths[i]
logln("demod_buffer_length: " .. demod_buffer_length)

    SNR_dB = SNR_start_dB
    while SNR_dB >= SNR_end_dB do
        noise = db2value(SNR_dB)

        local BER_sum = 0;
        local BER_divide = 0;
        for j=1,packet_cnt do
            -- first create document contain the parameters
            local id = mongo_create_one_with_jsonstr(collection, "{}")
            local tab = rt.get_file_by_id(collection, id)
            -- logln("record file is [" .. collection .. ":" .. id .. "]")
            tab.NLCD = 16
            tab.ct_fast = 8
            tab.ct_slow = 64
            tab.combine = 1
            tab.cycle = 32
            tab.duty = 4
            -- tab.bit_per_symbol = 4
            tab.bit_per_symbol = 6
            tab.data = generate_random_data(packet_size)
            tab.frequency = {} tab.frequency["$numberDouble"] = "4000"

            tab.bias = -256
            tab.effect_length = 3
            tab.refs_id = refs_id
            tab.noise = {} tab.noise["$numberDouble"] = "" .. noise
            rt.save_file(tab)
            run("Tester/Emulation/EM_MongoEmulate", collection, id)
            -- then demodualte
            tab = rt.get_file_by_id(collection, id)
            tab.demod_data_id = tab.emulated_id
            tab.demod_buffer_length = demod_buffer_length
            tab.demod_nearest_count = 4
            rt.save_file(tab)
            run("Tester/Emulation/EM_Demodulate", collection, id)
            tab = rt.get_file_by_id(collection, id)
            local BER = tonumber(tab.BER["$numberDouble"])
            -- logln("BER: " .. BER)
            BER_sum = BER_sum + BER
            BER_divide = BER_divide + 1
            may_terminate()  -- to allow terminate in process
        end
        local BER = BER_sum / BER_divide
        logln("" .. SNR_dB .. " " .. demod_buffer_length .. " " .. BER)
        
        SNR_dB = SNR_dB + SNR_step_dB
    end
end
