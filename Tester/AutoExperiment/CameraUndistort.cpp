#include "CameraLib.h"

VideoCapture cam;
Mat frame;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv);

	// if (argc != 5) {
	// 	printf("usage: ./test\n");
	// 	return -1;
	// }

	// cam.open("http://192.168.0.127:8080/?action=stream?dummy=param.mjpg");
	// assert(cam.isOpened() && "cannot open video");
	// namedWindow("Camera");
	// cam >> frame;
	frame = imread("chessboard_1920x1080.jpg");
	// frame = imread("capture.png");
	Mat gray_image, black_image, first_image;
	cvtColor(frame, gray_image, CV_BGR2GRAY);
	threshold(gray_image, black_image, 127, 255, CV_THRESH_BINARY);

	first_image = black_image;
	imshow("Camera", first_image);

	Mat second_image;
	camera_undistort(first_image, second_image);
	imshow("second_image", second_image);
	imwrite("undistorted.jpg", second_image);

	while (waitKey(30) < 0);  // for debug

	return 0;

}
