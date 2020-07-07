import paho.mqtt.client as mqtt
import sys, time

host = "localhost"
port = 1883
topic = "retroturbo/"
timeout = 0.3  # s
clientID = "6666"

command = ""
client = mqtt.Client(clientID)
def on_connect(client, userdata, flags, rc):
    print("MQTT connected with result code " + str(rc))
client.on_connect = on_connect
def on_subscribe(client, userdata, mid, granted_qos):
	# print("subscribed")
	pass
client.on_subscribe = on_subscribe
def null_on_message(client, userdata, msg):
	pass
# def on_message(client, userdata, msg):
#     print(msg.topic + " " + str(msg.payload))
# client.on_message = on_message
client.connect(host, port, 10)
client.loop_start()

hostinfos = {}
def findhosts():
	client.subscribe(topic + "reply/+")
	client.publish(topic + "query")
	def on_message(client, userdata, msg):
		# print(msg.topic + " " + str(msg.payload))
		if msg.topic.startswith(topic + "reply/"):
			hid = msg.topic[len(topic + "reply/"):]
			hostinfos[hid] = str(msg.payload, encoding="utf-8")
	client.on_message = on_message
	time.sleep(timeout)
	client.on_message = null_on_message

def shutdownhost(hid):
	client.publish(topic + hid + "/shutdown")

def help_exit():
	print("usage: command [...]")
	print("  list: list service id")
	print("  shutdown [id]: shutdown one service, all if not provided")
	exit(0)

if __name__ == "__main__":
	argc = len(sys.argv)
	if argc < 2:
		help_exit()
	command = sys.argv[1]
	if command == "list":
		findhosts()
		print("found %d RetroHosts" % len(hostinfos))
		for h in hostinfos:
			print("-- %s: %s" % (h, hostinfos[h]))
	elif command == "shutdown":
		findhosts()
		if argc == 2:
			print("shutdown all service found, they are:", [h for h in hostinfos])
			for h in hostinfos:
				shutdownhost(h)
		else:
			shutdownhost(sys.argv[2])

client.loop_stop()
