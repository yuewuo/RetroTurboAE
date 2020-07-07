"""
You can excute any lua script in TurboHost using this python scripts
run "pip install paho-mqtt" for the first time

This script is special that it will open a new process for TH_Host
you should execute this under /build/ folder, like "python3 ../WebGUI/lua/execlua_new_process.py"
"""
import os, random, time, sys, subprocess
from threading import Thread 
import paho.mqtt.client as mqtt

sid = None

def reader(f,filename):
    global sid
    local_sid = None
    with open(filename, "wb") as fo:
        while True:
            line=f.readline()
            if line:
                # print(line)
                if sid is None:  # try to analyze the line
                    s = str(line, encoding='ascii')
                    if local_sid is None:
                        idx = s.find("TurboID: ")
                        if idx != -1:
                            start = idx + len("TurboID: ")
                            local_sid = s[start:start+4]
                    else:  # find "hostmqtt connected: 0"
                        if s.find("hostmqtt connected: 0") != -1:
                            sid = local_sid
                fo.write(line)
                fo.flush()
            else:
                print("why not line?")
                break
    print("end of reader")

def main():
    global sid
    assert len(sys.argv) >= 2, "usage: <lua filename> [lua script inline] [MQTT sid] [sound file played when exit]"
    lua_inline = ""
    if len(sys.argv) > 2:
        lua_inline = sys.argv[2] + "\n"
    if len(sys.argv) > 3:
        sid = sys.argv[3]
    if len(sys.argv) > 4:
        import playsound  # first test if the sound library existed

    # first start a new process
    if sid is None:
        sid = "".join(random.sample("0123456789abcdefghijklmnopqrstuvwxyz", 4))
    sp = subprocess.Popen(["./Tester/TurboHost/TH_Host", "-S", "-C", sid], bufsize=1, stdout = subprocess.PIPE)
    sid = None
    filename = time.strftime('TH_Host_%Y_%m_%d_%H_%M_%S_record.txt')
    print("record TH_Host output as \"%s\"" % filename)
    t=Thread(target=reader,args=(sp.stdout,filename))
    t.daemon=True
    t.start()
    while sid is None:
        time.sleep(0.2)
        if sid is None:
            print("waiting for sid")
    print("found sid: %s" % sid)

    # print("main function called, here is demo of how to use this script:")
    luaExcute = LuaExcute()
    luaExcute.sid = sid
    luaExcute.load_library_code_from_html()  # use default path for ../index.html
    # luaExcute.run(luaExcute.get_lua_script("190609_simple_demo.lua"))
    luaExcute.run(lua_inline + luaExcute.get_lua_script(sys.argv[1]))
    # use "logln(xx)" in lua will print to stdout, just like you're running lua locally
    print(luaExcute.errcode, luaExcute.errstring)  # you can get the result of lua
    # then shutdown the client, just try
    luaExcute.client.publish(luaExcute.prefix + "/" + luaExcute.sid + "/shutdown").wait_for_publish()
    print("shudown request sent to turbohost, it should stop gracefully")

    if len(sys.argv) > 4:
        playsound.playsound(sys.argv[4])

class LuaExcute:
    def __init__(self, domain="localhost", port=1883, cid=None, prefix="retroturbo"):
        self.librarycode = ""
        if cid is None:
            cid = "".join(random.sample("0123456789abcdefghijklmnopqrstuvwxyz", 4))
        self.cid = cid
        self.sid = None
        self.prefix = prefix
        self.client = mqtt.Client(client_id=self.cid, userdata=self)
        self.client.on_connect = LuaExcute.on_connect
        self.client.on_message = LuaExcute.on_message
        self.client.connect(domain, port, 60)
        self.client.loop_start()
        self.running = False
        self.errcode = None
        self.errstring = ""  # the error of lua
    def run_start(self, usercode):
        assert(self.running == False)  # cannot start when already running one
        assert(self.sid is not None)  # not connected to server
        self.running = True
        self.client.publish(self.prefix + "/" + self.sid + "/do/" + self.cid, self.librarycode + usercode)
    def run_stop(self):
        print("[warning] this may cause strange behaviour, you may need to change cid then")
        self.running = False
    def run(self, usercode, timeout=None):
        self.run_start(usercode)
        start = time.time()
        self.errcode = None
        while self.errcode is None and (timeout is None or time.time() - start < timeout):
            try:
                time.sleep(0.01)
            except InterruptedError:
                pass
        if self.errcode is None and timeout is not None:
            print("[warning] this may cause strange behaviour, you may need to change cid then")
        self.running = False
    def __del__(self):
        print("del called, but may consume about 1.0 second to stop")
        self.client.loop_stop()  # seems useless
    def load_library_code_from_html(self, htmlpath=None):
        # find library code embedded in HTML file
        if htmlpath is None:
            htmlpath = os.path.join(os.path.dirname(os.path.dirname(os.path.realpath(__file__))), "index.html")
        assert(os.path.exists(htmlpath))  # "file not existed"
        html = ""
        with open(htmlpath, "r", encoding='utf-8') as f:
            html = f.read()
        html = html[html.find("-- RetroTurbo Lua Library"):]
        html = html[:html.find("*/")]
        self.librarycode += html
    @staticmethod
    def get_lua_script(filename):
        script = ""
        with open(os.path.join(os.path.dirname(os.path.realpath(__file__)), filename), "r", encoding="utf-8") as f:
            script = f.read()
        return script
    @staticmethod
    def on_connect(client, self, flags, rc):
        # print("Connected with result code " + str(rc))
        client.subscribe(self.prefix + "/#")
    @staticmethod
    def on_message(client, self, msg):
        # print(msg.topic + " " + str(msg.payload))
        reply_prefix = self.prefix + "/reply/"
        if msg.topic[:len(reply_prefix)] == reply_prefix and len(msg.topic) == len(reply_prefix) + 4:
            if self.sid is None:
                self.sid = msg.topic[len(reply_prefix):]
        elif self.sid is not None:
            server_prefix = self.prefix + "/" + self.sid + "/"
            if msg.topic == server_prefix + "ret/" + self.cid:
                errcode = int(str(msg.payload, encoding="utf-8"))
                self.errstring = ["OK", "YIELD", "ERRRUN", "ERRSYNTAX", "ERRMEM", "ERRGCMM", "ERRERR", "BUSY"][errcode]
                self.errcode = errcode  # report errcode
            elif msg.topic == server_prefix + "log/" + self.cid:
                print(str(msg.payload, encoding="utf-8"), end='')
            elif msg.topic == server_prefix + "err/" + self.cid:
                print(str(msg.payload, encoding="utf-8"))

if __name__ == "__main__":
    main()
    
