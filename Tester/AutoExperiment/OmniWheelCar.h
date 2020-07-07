/*
 * A library to control OmniWheelCar
 * car must be in POSITION mode (there's a switch on the car, switch it to 'P' rather than 'V')
 * 
 * MCU project is in https://github.com/wuyuepku/ceilgo
 * 
 */

#include <mutex>
#include <vector>
#include <assert.h>
#include <string.h>
#include "serial/serial.h"
#include <cmath>
using namespace std;

struct OmniWheelCar {
	OmniWheelCar();
	serial::Serial *com;
	string port;
	bool verbose;
	mutex lock;
	int open(const char* portname);
	int close();
	unsigned char ctrl_buf[10];
	int __send_ctrl_8byte(const unsigned char* data);  // will add 0xff and 0xfe at head
// immediate motion control
	void stop();
	void move(int A, int B, int C);
	void setMaxSpeed(unsigned int speed);  // default is 100
	void move_abs(float x, float y, float r);
	void pause();  // pause PID engine and PWM output
	void resume();  // resume PID engine and PWM output
};

#ifdef OMNIWHEELCAR_IMPLEMENTATION
#undef OMNIWHEELCAR_IMPLEMENTATION

OmniWheelCar::OmniWheelCar(): com(NULL) {
	verbose = false;
}

int OmniWheelCar::open(const char* _port) {
	assert(com == NULL && "device has been opened");
	// open com port
	lock.lock();
	port = _port;
	if (verbose) printf("opening device \"%s\"\n", port.c_str());
	com = new serial::Serial(port.c_str(), 115200, serial::Timeout::simpleTimeout(1000));
	assert(com->isOpen() && "port is not opened");
	lock.unlock();
	return 0;
}

int OmniWheelCar::close() {
	lock.lock();
	if (com) {
		com->close();
		delete com;
		com = NULL;
	}
	lock.unlock();
	return 0;
}

int OmniWheelCar::__send_ctrl_8byte(const unsigned char* data) {
	assert(com && "device not opened");
	ctrl_buf[0] = 0xff;
	ctrl_buf[1] = 0xfe;
	memcpy(ctrl_buf+2, data, 8);  // copy 8 byte control
	lock.lock();
	assert(com->write(ctrl_buf, 10) == 10 && "write failed");
	com->flush();
	lock.unlock();
	return 0;
}

#define a2u8(x) ((unsigned char)((x)&0xFF))

void OmniWheelCar::stop() {
	const unsigned char msg[8] = {0x77, 0, 0, 0, 0, 0, 0, 0};
	__send_ctrl_8byte(msg);
}

void OmniWheelCar::move_abs(float x, float y, float r) {
	// rough measurement: 55000 -> pi*10cm
	float k = 55000 / 100 / 3.14159, h = 1.73205 / 2;
	float a=0, b=0, c=0;
	a += -k * x;
	b += 0.5 * k * x;
	c += 0.5 * k * x;
	b += -h * k * y;
	c += h * k * y;
	// R = 165mm, delta = k * R * (r * pi / 180) = k * (R*pi/180) * r
	float g = k * 165 * 3.14159 / 180;
	a += g * r;
	b += g * r;
	c += g * r;
	// overflow
	float fa = fabs(a / 30000);
	float fb = fabs(b / 30000);
	float fc = fabs(c / 30000);
	if (fa <= 1 && fb <= 1 && fc <= 1) {
		move(a, b, c);
	} else {
		int num = max(max(fa, fb), fc) + 1;
		for (int i=0; i<num; ++i) {
			move(a/num, b/num, c/num);
		}
	}
}

void OmniWheelCar::move(int A, int B, int C) {
	const unsigned char msg[8] = {0x66, a2u8(A>>8), a2u8(A), a2u8(B>>8), a2u8(B), a2u8(C>>8), a2u8(C), 0};
	__send_ctrl_8byte(msg);
}

void OmniWheelCar::setMaxSpeed(unsigned int speed) {
	assert(speed <= 1000 && "speed too large, may slipper");
	const unsigned char msg[8] = {0x88, a2u8(speed>>8), a2u8(speed), 0, 0, 0, 0, 0};
	__send_ctrl_8byte(msg);
}

void OmniWheelCar::pause() {
	const unsigned char msg[8] = {0xA0, 0, 0, 0, 0, 0, 0, 0};
	__send_ctrl_8byte(msg);
}

void OmniWheelCar::resume() {
	const unsigned char msg[8] = {0xA1, 0, 0, 0, 0, 0, 0, 0};
	__send_ctrl_8byte(msg);
}

#undef a2u8

#endif
