#include "sysutil.h"
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <opencv2/imgcodecs.hpp>
#include <string>
#include <sstream>
#include <iostream>
#include <vector>
#include <iostream>
#include <quirc.h>
using namespace cv;
using namespace std;

static inline void camera_undistort(InputArray src, OutputArray dst) {
	assert(src.size().width == 1920 && src.size().height == 1080 && "only support 1920x1080");
	Mat cameraMatrix(3, 3, CV_32FC1);
	for (int i=0; i<3; ++i) for (int j=0; j<3; ++j) cameraMatrix.at<float>(i,j) = 0;
	cameraMatrix.at<float>(0,0) = 106.20558984;
	cameraMatrix.at<float>(0,2) = 959.99855608;
	cameraMatrix.at<float>(1,1) = 106.02955628;
	cameraMatrix.at<float>(1,2) = 540.00011255;
	cameraMatrix.at<float>(2,2) = 1;
	Mat distCoeffs(1, 5, CV_32FC1);
	distCoeffs.at<float>(0,0) = -3.50233542e-03;
	distCoeffs.at<float>(0,1) = 2.58859079e-05;
	distCoeffs.at<float>(0,2) = 6.53387227e-04;
	distCoeffs.at<float>(0,3) = -9.63847780e-04;
	distCoeffs.at<float>(0,4) = -2.19384074e-07;
	undistort(src, dst, cameraMatrix, distCoeffs);
}

// the chessboard picture printed is 6.75cm/9 = 7.5mm
// undistorted picture is 1147/9 = 128 pixel
#define CAMERA_DISTANCE_PER_PIXEL (7.5/128)

// provide class to use quirc just like opencv
using corner_t = array<array<float, 2>, 4>;
struct QuircQRCodeDetector {
	quirc* q;
	int W;
	int H;
	QuircQRCodeDetector(int W_, int H_) {
		W = W_;
		H = H_;
		q = quirc_new();
		if (!q || quirc_resize(q, W, H) < 0) throw bad_alloc();
	}
	corner_t detect(const Mat& m, string& datastr) {
		assert(m.cols == W && m.rows == H && m.type() == CV_8UC1);
		unsigned char* qb = quirc_begin(q, nullptr, nullptr);
		for (int i = 0; i < H; ++i) copy(m.data + m.step * i, m.data + m.step * i + W, qb + i * W);
		quirc_end(q);
		assert(1 == quirc_count(q));
		quirc_code code;
		quirc_extract(q, 0, &code);
		corner_t ret;
		quirc_data data;
		quirc_decode_error_t err = quirc_decode(&code, &data);
		datastr = (char*)data.payload;
		assert(!err && "QRcode decode error");
		for (int i = 0; i < 4; ++i)  {
			ret[i] = {(float)code.corners[i].x, (float)code.corners[i].y };
		}
		return ret;
	}
	~QuircQRCodeDetector() { quirc_destroy(q); }
};

struct RefreshingCamera {
	VideoCapture cam;
	Mat frame;
	thread dumpd;
	mutex frm;
	atomic_bool run;
	atomic_bool first_run_finished;
	RefreshingCamera(): run(false) {}
	void open(const char* file) {
		assert(cam.open(file));
		run = true;
		first_run_finished = false;
		dumpd = thread([this]() {
			while (run) {
				Mat t1; cam >> t1;
				this->first_run_finished = true;
				{ lock_guard<mutex> lock(frm); frame = t1; };
			}
		});
		while ( ! first_run_finished) this_thread::sleep_for(10ms);  // to make sure first picture is get successfully
	}
	~RefreshingCamera() { run = false; if (dumpd.joinable()) dumpd.join(); }
	Mat get() {
		Mat gray, ret;
		{
			lock_guard<mutex> lock(frm);
			cvtColor(frame, gray, CV_BGR2GRAY);
		}
		camera_undistort(gray, ret);
		return ret;
	}
	Mat get_from_frame(Mat f) {
		Mat gray, ret;
		cvtColor(f, gray, CV_BGR2GRAY);
		camera_undistort(gray, ret);
		return ret;
	}
};

string get_move_parameter(QuircQRCodeDetector& qrd, Mat& frame, float& move_x_mm, float& move_y_mm, float& rotate_deg) {
	string datastr;
	auto con = qrd.detect(frame, datastr);
#define BX(i) (con[i][0])
#define BY(i) (con[i][1])
	// for (int i = 0; i < 4; ++i) {
	// 	printf("(%f,%f), ", BX(i), BY(i));
	// } printf("\n");
	float center_x = (BX(0) + BX(1) + BX(2) + BX(3)) / 4. - 960;
	float center_y = (BY(0) + BY(1) + BY(2) + BY(3)) / 4. - 540;
	cout << "center: " << center_x << "," << center_y << endl;
	float atan_y = (BX(3) + BX(2) - BX(0) - BX(1)) / 2.;
	float atan_x = (BY(0) + BY(1) - BY(3) - BX(2)) / 2.;
	float rotation = atan2(atan_x, atan_y);
	cout << "rotation: " << rotation << endl;
	// then indicate how should car move
	move_x_mm = - center_x * CAMERA_DISTANCE_PER_PIXEL;
	move_y_mm = center_y * CAMERA_DISTANCE_PER_PIXEL;
	rotate_deg = - (rotation * 180 / M_PI + 90) * 4/3 * 1.1;
	return datastr;
}
