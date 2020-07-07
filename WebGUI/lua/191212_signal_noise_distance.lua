-- tag需要配置成20Hz恒定闪烁（AlwaysBlink），本脚本不操作tag
-- 每个距离接受2秒钟波形，输出信号强度和噪声水平

distance = 1
gain_dac_min = 0.5
gain_dac_max = 2.0
gain_dac_step = 0.1
record_ms = 5000  -- 5s

-- collection = "191212_signal_noise_distance"
collection = "tmp"

for gain_dac=gain_dac_min,gain_dac_max+gain_dac_step/2,gain_dac_step do
    reader_set_gain(gain_dac)
    sleepms(100)

    local record_id, filename = set_record(1)
    sleepms(record_ms)
    set_record(0)

    id = mongo_create_one_with_jsonstr(collection, "{}")
    local tab = rt.get_file_by_id(collection, id)
    tab.distance = distance
    tab.gain_dac = {} tab.gain_dac["$numberDouble"] = "" .. gain_dac
    tab.record_id = record_id
    -- plot(record_id)
    rt.save_file(tab)

    run("Tester/Archive/AR_ComputeSignalStrengthAndNoiseV2", collection, id)
    tab = rt.get_file_by_id(collection, id)
    plot(tab.aligned_period)  -- plot to early terminate
    amp = tab.amp["$numberDouble"]

    -- 根据VCA821的手册计算增益
    AVV = 1000 / 50 * 1 / ( 1 + math.exp( (0.85 - gain_dac) / 0.09 ) )
    -- 不考虑后面的固定增益和sensor的固定增益，计算信号强度
    sensor_signal = amp / AVV
    tab.sensor_signal = {} tab.sensor_signal["$numberDouble"] = "" .. sensor_signal

    if gain_dac == gain_dac_min or gain_dac == gain_dac_max then
        logln("gain_dac: " .. gain_dac)
        logln("id: " .. id)
        logln("distance: " .. distance)
        logln("amp: " .. amp)
        logln("AVV: " .. AVV)
        logln("sensor_signal: " .. sensor_signal)
    end

    rt.save_file(tab)
    may_terminate()  -- to allow terminate in process
end
