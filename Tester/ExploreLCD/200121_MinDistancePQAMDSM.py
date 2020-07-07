import math

# ref_filename = "200115_model_500us_17mseq_5.5v.bin.resampled.reordered"
ref_filename = "200115_model_500us_17mseq_9v.bin.resampled.reordered"
min_time = 60
brute_force_symbols = 2

def main():
    # test_data_rate(4e3, [1,2,3,4])
    # test_data_rate(8e3, [1,2,3,4])
    # test_data_rate(12e3, [1,2,3,4])
    # test_data_rate(16e3, [1,2,3,4])
    # test_data_rate(1e3, [1,2,3,4], NLCDs=[1,2,3,4], bpscs=[1,2,3], PQAM=1)
    # test_data_rate(1e3, [1,2,3,4], NLCDs=[1,2,3], bpscs=[1,2], PQAM=0)
    # test_data_rate(32e3, [1,2,3], NLCDs=[16], bpscs=[1,2,3,4])
    test_data_rate(32e3, [4], NLCDs=[16], bpscs=[1,2,3,4])

def test_data_rate(data_rate, dutys, NLCDs=[1,2,4,8], bpscs=[1,2,3,4], PQAM=1):
    for duty in dutys:
        with open("200121_MinDistancePQAMDSM_%dkbps_duty%d_PQAM%d.bat" % (data_rate/1e3, duty, PQAM), "w", encoding='utf-8') as f:
            for NLCD in NLCDs:
                for bit_per_symbol_channel in bpscs:
                    fire_interval = math.ceil((bit_per_symbol_channel * (1 + PQAM)) * 40e3 / data_rate)
                    title, cmd = command(NLCD, bit_per_symbol_channel, fire_interval, duty, PQAM)
                    f.write(":: %s\n" % title)
                    f.write("%s\n" % cmd)

def command(NLCD, bit_per_symbol_channel, fire_interval, duty, PQAM):
    global ref_filename
    global min_time
    global brute_force_symbols
    data_rate = ((bit_per_symbol_channel * (1 + PQAM)) * 40e3 / fire_interval)
    title = "[ %d bps] NLCD=%d, bpsc=%d, fire=%d, duty=%d, PQAM=%d" % (data_rate, NLCD, bit_per_symbol_channel, fire_interval, duty, PQAM)
    parameters = (title, ref_filename, NLCD, bit_per_symbol_channel, fire_interval, duty, PQAM, brute_force_symbols, min_time, title)
    return (title, 'start "%s" /AFFINITY 0x7FF cmd.exe /k .\\Tester\\ExploreLCD\\EL_200121_MinDistancePQAMDSM.exe %s %d %d %d %d %d %d %d ^> "%s.txt"' % parameters)

if __name__ == "__main__":
    main()
