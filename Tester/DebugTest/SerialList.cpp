#include <stdio.h>
#include <vector>
#include <serial/serial.h>
using std::vector;
using serial::PortInfo;
using serial::list_ports;

int main() {
	vector<PortInfo> ports = list_ports();
	for (auto it = ports.begin(); it != ports.end(); ++it) {
		printf("port: \"%s\", fname: \"%s\", hardware_id: \"%s\"\n", it->port.c_str(), it->description.c_str(), it->hardware_id.c_str());
	}
}
