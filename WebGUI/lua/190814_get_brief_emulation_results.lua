
-- copy this from "190813_emulation_long_test.lua"
frequencys = {4000, 8000}
dutys = {2,4,6,8,12,16}
combines = {1,2,4,8}
bit_per_symbols = {2,4,6,8}

SNR_start_dB = 20  -- 0.1
SNR_end_dB = 60  -- 0.001
SNR_step_dB = 1



for x1=1,#frequencys do  -- frequency loop
    frequency = frequencys[x1]

    for x2=1,#dutys do -- duty loop
        duty = dutys[x2]

        for x3=1,#combines do -- combine loop
            combine = combines[x3]

            for x4=1,#bit_per_symbols do -- bit_per_symbol loop
                bit_per_symbol = bit_per_symbols[x4]

                filename = "emuret_" .. frequency .. "_" .. duty .. "_" .. combine .. "_" .. bit_per_symbol .. ".txt"
                logln(filename)
            end
        end
    end
end
