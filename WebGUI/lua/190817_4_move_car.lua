remote_host = "192.168.0.127"
timeout = 10.0

target = 300
taget_angle = 0
tty = "/dev/ttyUSB0"

ret = rt.proxy_run([[
    run("Tester/AutoExperiment/AE_QRCodeMoveTo190818", "]]..tty..[[", "]]..target..[[", "]]..taget_angle..[[")
    run("Tester/AutoExperiment/AE_CarPause", "]]..tty..[[")
]], "", remote_host, timeout)
assert(ret, "tag send failed")
