# since with 8.5ms effect length, we can only test those edges faster than 3.2ms
# to avoid modifying and debugging new programs that support longer sequences,
#       it is equivalent to test 3.2ms edges with larger data rate, e.g. 2kbps, 4kbps, 8kbps...

# 200129_LongerRefTestOptimal

import sys, os, subprocess, math, time

data_rate_exp = 1
charging_edge = None
discharging_edge = None
ref_filename = None
ref1kSs_id = None
edge_prefix = None

def main():
    if len(sys.argv) != 3:
        print("usage: . <charging_edge/ms> <discharging_edge/ms>")
        print("    the actual data_rate is 1kbps * data_rate_exp, where (dis)charging_edge / data_rate_exp <= 3.2")
        exit(-1)
    global data_rate_exp
    global charging_edge
    global discharging_edge
    global data_rate_exp
    charging_edge = float(sys.argv[1])
    discharging_edge = float(sys.argv[2])
    while charging_edge / data_rate_exp > 3.2:
        data_rate_exp = data_rate_exp * 2
    while discharging_edge / data_rate_exp > 3.2:
        data_rate_exp = data_rate_exp * 2
    print("data_rate: %dbps, charging_edge: %f, discharging_edge: %f\n" % (1e3, charging_edge, discharging_edge))
    charging_edge = charging_edge / data_rate_exp
    discharging_edge = discharging_edge / data_rate_exp
    print("data_rate: %dbps, charging_edge: %f, discharging_edge: %f\n" % (1e3 * data_rate_exp, charging_edge, discharging_edge))
    generate_prerequisites()
    for PQAM in [0,1]:
        for duty in [1,2,3]:
            for NLCD in [1,2,4]:
                for bpsc in [1,2,3]:
                    fire_interval = math.floor((bpsc * (1 + PQAM)) * 40e3 / (1e3 * data_rate_exp))
                    _, cmd = PQAM_DSM_command(NLCD, bpsc, fire_interval, duty, PQAM)
                    time.sleep(0.1)
                    os.system(cmd)

def PQAM_DSM_command(NLCD, bit_per_symbol_channel, fire_interval, duty, PQAM):
    global ref_filename
    data_rate = ((bit_per_symbol_channel * (1 + PQAM)) * 40e3 / fire_interval)
    print((NLCD, bit_per_symbol_channel, fire_interval, duty, PQAM), "actual data_rate = %s" % data_rate)
    title = "%s NLCD=%d, bpsc=%d, fire=%d, duty=%d, PQAM=%d" % (edge_prefix, NLCD, bit_per_symbol_channel, fire_interval, duty, PQAM)
    parameters = (title, ref_filename, NLCD, bit_per_symbol_channel, fire_interval, duty, PQAM, 128, 8, 2400, title)
    return (title, 'start "%s" /AFFINITY 0x7FF cmd.exe /k .\\Tester\\ExploreLCD\\EL_200203_MinDistancePQAMDSM_demod.exe %s %d %d %d %d %d %d %d %d ^> "%s.txt"' % parameters)

def generate_prerequisites():
    global charging_edge
    global discharging_edge
    global ref_filename
    global ref1kSs_id
    global edge_prefix
    global data_rate_exp
    assert 0 == os.system(".\\Tester\\ExploreLCD\\EL_200126_GenerateExpReference.exe %f %f" % (charging_edge, discharging_edge)), "run failed"
    ref_filename = "simu_%dus_%dus.reordered" % (int(1e3*charging_edge), int(1e3*discharging_edge))
    edge_prefix = "[%d,%d,%d]" % (int(1e3*charging_edge), int(1e3*discharging_edge), data_rate_exp)
    ret = os.popen(".\\Tester\\ExploreLCD\\EL_200121_Produce1kSsRef.exe %s" % ref_filename).readlines()[0]
    assert ret.startswith("upload ref1kSs with ID: "), "generate 1kS/s reference failed"
    ref1kSs_id = ret.split(' ')[-1].strip()

if __name__ == "__main__":
    main()
