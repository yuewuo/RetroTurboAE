#include "CameraLib.h"
#define OMNIWHEELCAR_IMPLEMENTATION
#include "OmniWheelCar.h"
#include <iostream>
#include <thread>
#include <mutex>
using namespace std;

RefreshingCamera cam;
QuircQRCodeDetector qrd = QuircQRCodeDetector(1920, 1080);
OmniWheelCar car;
#define DISTANCE_TOLERANCE_MM 2
#define DEGREE_TOLERANCE_DEG 2
#define DELAY_SMALL 2000
#define DELAY_LARGE 3000
#define CAM_URL "http://127.0.0.1:8080/?action=stream&dummy=param.mjpg"
int map2(int d1) {
    int r[] = {4, 3, 2, 1, 0};
    return d1 % 300 + r[d1 / 300] * 300;
}
int adjust_now(int target_angle) {  // return the location now
	string info;
	while (1) {
		Mat frame = cam.get();
		float move_x_mm, move_y_mm, rotate_deg;
		info = get_move_parameter(qrd, frame, move_x_mm, move_y_mm, rotate_deg);
		rotate_deg -= target_angle;
		// cout << "should move x: " << move_x_mm << ", y: " << move_y_mm << ", r: " << rotate_deg << endl;		
		if (fabs(move_x_mm) < DISTANCE_TOLERANCE_MM && fabs(move_y_mm) < DISTANCE_TOLERANCE_MM && fabs(rotate_deg) < DEGREE_TOLERANCE_DEG) break;
		if (fabs(rotate_deg) >= DEGREE_TOLERANCE_DEG) {
			cout << "move x: " << 0 << ", y: " << 0 << ", r: " << rotate_deg << endl;
			car.move_abs(0, 0, rotate_deg);
		} else {
			cout << "move x: " << move_x_mm << ", y: " << move_y_mm << ", r: " << 0 << endl;
			car.move_abs(move_x_mm, move_y_mm, 0);
		}
		this_thread::sleep_for(std::chrono::milliseconds(DELAY_SMALL));
	}
	return map2(atoi(info.c_str()));
}

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv);

	if (argc != 3 && argc != 4) {
		printf("usage: <portname> <target(cm)> [angle(deg)]\n");
		return -1;
	}

	car.open(argv[1]);
	car.setMaxSpeed(200);
	car.resume();

	int target = atoi(argv[2]);
	assert(target >= 0 && target % 10 == 0);
	printf("target is %d\n", target);
	int target_angle = argc == 4 ? atoi(argv[3]) : 0;
	printf("target_angle is %d\n", target_angle);

	cam.open(CAM_URL);

	int start = adjust_now(0);
	printf("now location is %d, will move %d\n", start, target - start);

	for (int now = start; now != target;) {
		printf("now: %d, target: %d\n", now, target);
		car.move_abs(0, now<target?100:-100, 0);
		this_thread::sleep_for(std::chrono::milliseconds(DELAY_LARGE));
		now = adjust_now(0);
	}
	adjust_now(target_angle);

	return 0;

}