#include "CameraLib.h"
#include <iostream>

VideoCapture cam;
Mat frame;
QRCodeDetector qrd = QRCodeDetector();

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv);

	if (argc != 2) {
		printf("usage: <filename>\n");
		return -1;
	}

	frame = imread(argv[1]);

	Mat bbox, rectifiedImage;
	string info = qrd.detectAndDecode(frame, bbox, rectifiedImage);
	printf("info: %s\n", info.c_str());
	printf("length: %d\n", (int)info.size());
	cout << bbox << endl;

	while (waitKey(30) < 0);  // for debug

	return 0;

}
