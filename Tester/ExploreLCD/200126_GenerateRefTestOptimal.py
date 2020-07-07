import sys, os, subprocess, math, time

charging_edge = None
discharging_edge = None
ref_filename = None
ref1kSs_id = None
edge_prefix = None

def main():
    if len(sys.argv) != 3:
        print("usage: . <charging_edge/ms> <discharging_edge/ms>")
        exit(-1)
    global data_rate
    global charging_edge
    global discharging_edge
    charging_edge = float(sys.argv[1])
    discharging_edge = float(sys.argv[2])
    print("data_rate: %dbps, charging_edge: %f, discharging_edge: %f\n" % (1e3, charging_edge, discharging_edge))
    generate_prerequisites()
    os.system(OOK_command()[1])
    os.system(Miller_command()[1])
    for PQAM in [0,1]:
        for duty in [1,2,3]:
            for NLCD in [1,2,4]:
                for bpsc in [1,2,3]:
                    fire_interval = math.ceil((bpsc * (1 + PQAM)) * 40e3 / 1e3)
                    title, cmd = PQAM_DSM_command(NLCD, bpsc, fire_interval, duty, PQAM)
                    time.sleep(0.2)
                    os.system(cmd)

def PQAM_DSM_command(NLCD, bit_per_symbol_channel, fire_interval, duty, PQAM):
    global ref_filename
    data_rate = ((bit_per_symbol_channel * (1 + PQAM)) * 40e3 / fire_interval)
    title = "%s NLCD=%d, bpsc=%d, fire=%d, duty=%d, PQAM=%d" % (edge_prefix, NLCD, bit_per_symbol_channel, fire_interval, duty, PQAM)
    parameters = (title, ref_filename, NLCD, bit_per_symbol_channel, fire_interval, duty, PQAM, 2, 60, title)
    return (title, 'start "%s" /AFFINITY 0x7FF cmd.exe /k .\\Tester\\ExploreLCD\\EL_200121_MinDistancePQAMDSM.exe %s %d %d %d %d %d %d %d ^> "%s.txt"' % parameters)

def OOK_command():
    global ref1kSs_id
    title = "%s OOK" % (edge_prefix)
    return (title, 'start "%s" /AFFINITY 0x7FF cmd.exe /k .\\Tester\\ExploreLCD\\EL_200121_MinDistance1kSsOOK.exe %s ^> "%s.txt"' % (title, ref1kSs_id, title))

def Miller_command():
    global ref_filename
    title = "%s Miller" % (edge_prefix)
    return (title, 'start "%s" /AFFINITY 0x7FF cmd.exe /k .\\Tester\\ExploreLCD\\EL_200121_MinDistance2kSsMiller.exe %s ^> "%s.txt"' % (title, ref_filename, title))

def generate_prerequisites():
    global charging_edge
    global discharging_edge
    global ref_filename
    global ref1kSs_id
    global edge_prefix
    assert 0 == os.system(".\\Tester\\ExploreLCD\\EL_200126_GenerateExpReference.exe %f %f" % (charging_edge, discharging_edge)), "run failed"
    ref_filename = "simu_%dus_%dus.reordered" % (int(1e3*charging_edge), int(1e3*discharging_edge))
    edge_prefix = "[%d,%d]" % (int(1e3*charging_edge), int(1e3*discharging_edge))
    ret = os.popen(".\\Tester\\ExploreLCD\\EL_200121_Produce1kSsRef.exe %s" % ref_filename).readlines()[0]
    assert ret.startswith("upload ref1kSs with ID: "), "generate 1kS/s reference failed"
    ref1kSs_id = ret.split(' ')[-1].strip()

if __name__ == "__main__":
    main()
