"""
You can excute any lua script in TurboHost using this python scripts
run "pip install paho-mqtt" for the first time
"""
import os, random, time, sys
import paho.mqtt.client as mqtt

def main():
    assert(len(sys.argv) >= 2 and "usage: <lua filename> [sound file played when exit]")
    if len(sys.argv) >= 3:
        import playsound  # first test if the sound library existed

    # print("main function called, here is demo of how to use this script:")
    luaExcute = LuaExcute()
    luaExcute.find_turbohost_return_id_or_None()
    assert(luaExcute.sid is not None)  # turbohost not found
    luaExcute.load_library_code_from_html()  # use default path for ../index.html
    # luaExcute.run(luaExcute.get_lua_script("190609_simple_demo.lua"))
    luaExcute.run(luaExcute.get_lua_script(sys.argv[1]))
    # use "logln(xx)" in lua will print to stdout, just like you're running lua locally
    print(luaExcute.errcode, luaExcute.errstring)  # you can get the result of lua

    if len(sys.argv) >= 3:
        playsound.playsound(sys.argv[2])

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
            time.sleep(0.01)
        if self.errcode is None and timeout is not None:
            print("[warning] this may cause strange behaviour, you may need to change cid then")
        self.running = False
    def __del__(self):
        print("del called, but may consume about 1.0 second to stop")
        self.client.loop_stop()  # seems useless
    def find_turbohost_return_id_or_None(self, timeout=1.0):
        self.sid = None
        self.client.publish(self.prefix + "/query", "")
        start = time.time()
        while self.sid is None and time.time() - start < timeout:
            time.sleep(0.01)
        return self.sid
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
    
