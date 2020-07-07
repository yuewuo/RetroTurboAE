#include "CameraLib.h"
#include <iostream>

// #define WITH_WINDOW_DEBUG

RefreshingCamera cam;
QuircQRCodeDetector qrd = QuircQRCodeDetector(1920, 1080);

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv);

	const char* url = "http://192.168.0.127:8080/?action=stream&dummy=param.mjpg";
	if (argc > 1) {
		url = argv[1];
	}
	printf("using url: \"%s\"\n", url);

	cam.open(url);
	Mat frame = cam.get();
	// Mat frame = cam.get_from_frame(imread("capture_1920x1080.png"));

#ifdef WITH_WINDOW_DEBUG
	namedWindow("Camera");
	imshow("Camera", frame);
	while (waitKey(30) < 0);  // for debug
#endif
	imwrite("save.png", frame);

	float move_x_mm, move_y_mm, rotate_deg;
	string info = get_move_parameter(qrd, frame, move_x_mm, move_y_mm, rotate_deg);
	printf("info: %s\n", info.c_str());

	// then indicate how should car move
	cout << "use MoveCar with x: " << move_x_mm << ", y: " << move_y_mm << ", r: " << rotate_deg << endl;

#ifdef WITH_WINDOW_DEBUG
	while (waitKey(30) < 0);  // for debug
#endif

	return 0;

}
