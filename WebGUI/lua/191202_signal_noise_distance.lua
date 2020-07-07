-- tag需要配置成20Hz恒定闪烁（AlwaysBlink），本脚本不操作tag
-- 每个距离接受2秒钟波形，输出信号强度和噪声水平

distance = 1

-- collection = "191202_signal_noise_distance"
collection = "tmp"

record_ms = 2000  -- 2s
gain = 0.2
dac_volt = reader_gain_control(gain)

local record_id, filename = set_record(1)
sleepms(record_ms)
set_record(0)

id = mongo_create_one_with_jsonstr(collection, "{}")
local tab = rt.get_file_by_id(collection, id)
tab.distance = distance
tab.gain = {} tab.gain["$numberDouble"] = "" .. gain
tab.dac_volt = {} tab.dac_volt["$numberDouble"] = "" .. dac_volt
tab.record_id = record_id
plot(record_id)
rt.save_file(tab)

run("Tester/Archive/AR_ComputeSignalStrengthAndNoise", collection, id)
tab = rt.get_file_by_id(collection, id)
amp = tab.amp["$numberDouble"]
rt.reader.plot_rx_data(tab.aligned_period)

logln("id: " .. id)
logln("distance: " .. distance)
logln("amp: " .. amp)

-- 根据VCA821的手册计算增益
AVV = 1000 / 50 * 1 / ( 1 + math.exp( (0.85 - dac_volt) / 0.09 ) )
logln("AVV: " .. AVV)
-- 不考虑后面的固定增益和sensor的固定增益，计算信号强度
sensor_signal = amp / AVV
tab.sensor_signal = {} tab.sensor_signal["$numberDouble"] = "" .. sensor_signal
logln("sensor_signal: " .. sensor_signal)
rt.save_file(tab)
