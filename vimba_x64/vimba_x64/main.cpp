#include <iostream>
#include <string>
#include "VimbaC.h"
#include "VimbaCPP.h"
#include <opencv2/opencv.hpp>
#include <conio.h>
#include "utils.h"

using namespace std;
using namespace AVT::VmbAPI;
using namespace cv;

void test_opencv() {
	/* Canvas */
	Mat img(270, 720, CV_8UC3, Scalar(56, 50, 38));
	/* Straight line */
	line(img, Point(20, 40), Point(120, 140), Scalar(255, 0, 0), 3);
	/* Square */
	rectangle(img, Point(150, 40), Point(250, 140), Scalar(0, 0, 255), -1);
	/* Circle */
	circle(img, Point(330, 90), 50, Scalar(0, 255, 0), -1);
	/* Ellipse */
	ellipse(img, Point(460, 90), Size(60, 40), 45, 0, 360, Scalar(255, 255, 0), 2);
	/* Arbitrary polygon */
	Point points[1][5];
	int x = 40, y = 540;
	points[0][0] = Point(0 + y, 50 + x);
	points[0][1] = Point(40 + y, 0 + x);
	points[0][2] = Point(110 + y, 35 + x);
	points[0][3] = Point(74 + y, 76 + x);
	points[0][4] = Point(28 + y, 96 + x);
	const Point* ppt[1] = { points[0] };
	int npt[] = { 5 };
	polylines(img, ppt, npt, 1, 1, Scalar(0, 255, 255), 3);
	/* 繪出文字 */
	putText(img, "Test Passed !!", Point(10, 230), 0, 3, Scalar(255, 170, 130), 3);
	/* 開啟畫布 */
	namedWindow("OpenCV Test By:Charlotte.HonG", WINDOW_AUTOSIZE);
	imshow("OpenCV Test By:Charlotte.HonG", img);
	waitKey(0);
	destroyAllWindows();
	return;
}

// 1.define observer that reacts on new frames
class FrameObserver : public IFrameObserver
{
public:
	// In your contructor call the constructor of the base class
	// and pass a camera objectt
	string name;
	Mat cvMat;
	CameraPtr pCamera;
	FrameObserver(CameraPtr pCamera, string s) : IFrameObserver(pCamera)
	{
		// Put your initialization code here
		cout << "Init FrameObserver" << endl;
		name = s;
		this->pCamera = pCamera;
	}

	void FrameReceived(const FramePtr pFrame)
	{
		VmbFrameStatusType eReceiveStatus;
		if (VmbErrorSuccess == pFrame->GetReceiveStatus(eReceiveStatus))
		{
			//cout << "Frame received" << endl;
			if (VmbFrameStatusComplete == eReceiveStatus)
			{
				// Put your code here to react on a successfully received frame
				//cout << "Frame received successfully" << endl;
				VmbUchar_t *pImage = NULL;
				VmbUint32_t timeout = 500;
				VmbUint32_t nWidth = 10;
				VmbUint32_t nHeight = 10;
				if (VmbErrorSuccess != pFrame->GetWidth(nWidth))
					cout << "FAILED to aquire width of frame!" << endl;

				if (VmbErrorSuccess != pFrame->GetHeight(nHeight))
					cout << "FAILED to aquire height of frame!" << endl;

				if (VmbErrorSuccess != pFrame->GetImage(pImage))
					cout << "FAILED to acquire image data of frame!" << endl;
				cvMat = Mat(nHeight, nWidth, CV_8UC1, pImage);
				//cvtColor(cvMat, cvMat, CV_BayerBG2BGR);

				imshow("Our Great Window" + name, cvMat);
				//imwrite("C:\\Users\\NOL\\Desktop\\" + name + ".png", cvMat);
				waitKey(1);
			}
			else
			{
				// Put your code here to react on an unsuccessfully received frame
				cout << "Frame received failed" << endl;
			}
		}
		// When you are finished copying the frame , re-queue it
		m_pCamera->QueueFrame(pFrame);
	}
};

int main() {
	cout << "hi" << endl;

	// Test OpenCV installation
	test_opencv();

	VmbErrorType err;                              // Every Vimba function returns an error code that
					                               // should always be checked for VmbErrorSuccess
	VimbaSystem &sys = VimbaSystem::GetInstance(); // A reference to the VimbaSystem singleton
	CameraPtrVector cameras;                       // A list of known cameras
	FramePtrVector frames0(3), frames1(3);         // A list of frames for streaming. We chose
	                                               // to queue 3 frames.

	err = sys.Startup();
	// Check version
	VmbVersionInfo_t m_VimbaVersion;
	err = sys.QueryVersion(m_VimbaVersion);
	if (err != VmbErrorSuccess) {
		cerr << "CamCtrlVmbAPI::Init : version Query failed: " << err << endl;
		sys.Shutdown();
		exit(-2);
	}
	cout << "Vimba " << m_VimbaVersion.major << "." << m_VimbaVersion.minor
		<< " initialized" << endl << endl;
	// Open cameras
	err = sys.GetCameras(cameras);
	FeaturePtr pFeature;       // Any camera feature
	VmbInt64_t nPLS0, nPLS1;   // The payload size of one frame

	err = cameras[0]->Open(VmbAccessModeFull);
	err = cameras[1]->Open(VmbAccessModeFull);

	// Set action trigger mode
	err = cameras[0]->GetFeatureByName("TriggerSelector", pFeature);
	err = pFeature->SetValue("FrameStart");
	err = cameras[0]->GetFeatureByName("TriggerSource", pFeature);
	err = pFeature->SetValue("Action0");
	err = cameras[0]->GetFeatureByName("TriggerMode", pFeature);
	err = pFeature->SetValue("On");
	err = cameras[1]->GetFeatureByName("TriggerSelector", pFeature);
	err = pFeature->SetValue("FrameStart");
	err = cameras[1]->GetFeatureByName("TriggerSource", pFeature);
	err = pFeature->SetValue("Action0");
	err = cameras[1]->GetFeatureByName("TriggerMode", pFeature);
	err = pFeature->SetValue("On");

	// Set Action Command to camera
	int deviceKey = 11, groupKey = 22, groupMask = 33;
	cameras[0]->GetFeatureByName("ActionDeviceKey", pFeature);
	pFeature->SetValue(deviceKey);
	cameras[0]->GetFeatureByName("ActionGroupKey", pFeature);
	pFeature->SetValue(groupKey);
	cameras[0]->GetFeatureByName("ActionGroupMask", pFeature);
	pFeature->SetValue(groupMask);
	cameras[1]->GetFeatureByName("ActionDeviceKey", pFeature);
	pFeature->SetValue(deviceKey);
	cameras[1]->GetFeatureByName("ActionGroupKey", pFeature);
	pFeature->SetValue(groupKey);
	cameras[1]->GetFeatureByName("ActionGroupMask", pFeature);
	pFeature->SetValue(groupMask);

	err = cameras[0]->GetFeatureByName("PayloadSize", pFeature);
	err = pFeature->GetValue(nPLS0);
	err = cameras[1]->GetFeatureByName("PayloadSize", pFeature);
	err = pFeature->GetValue(nPLS1);

	// Attach observer
	FrameObserver *pOb0, *pOb1;
	pOb0 = new FrameObserver(cameras[0], "0");
	pOb1 = new FrameObserver(cameras[1], "1");
	IFrameObserverPtr pObserver0(pOb0);
	IFrameObserverPtr pObserver1(pOb1);
	//IFrameObserverPtr pObserver0(new FrameObserver(cameras[0], "0"));
	//IFrameObserverPtr pObserver1(new FrameObserver(cameras[1], "1"));
	for (FramePtrVector::iterator iter0 = frames0.begin(), iter1 = frames1.begin();
		frames0.end() != iter0 || frames1.end() != iter1;
		++iter0, ++iter1)
	{
		(*iter0).reset(new Frame(nPLS0));
		err = (*iter0)->RegisterObserver(pObserver0); // 2. Register the observer before queuing the frame
		err = cameras[0]->AnnounceFrame(*iter0);
		(*iter1).reset(new Frame(nPLS1));
		err = (*iter1)->RegisterObserver(pObserver1); // 2. Register the observer before queuing the frame
		err = cameras[1]->AnnounceFrame(*iter1);
	}
	
	// StartCapture
	err = cameras[0]->StartCapture();
	err = cameras[1]->StartCapture();

	// QueueFrame
	for (FramePtrVector::iterator iter0 = frames0.begin(), iter1 = frames1.begin();
		frames0.end() != iter0 || frames1.end() != iter1;
		++iter0, ++iter1)
	{
		err = cameras[0]->QueueFrame(*iter0);
		err = cameras[1]->QueueFrame(*iter1);
	}

	// AcquisitionStart
	err = cameras[0]->GetFeatureByName("AcquisitionStart", pFeature);
	err = pFeature->RunCommand();
	err = cameras[1]->GetFeatureByName("AcquisitionStart", pFeature);
	err = pFeature->RunCommand();


	// Send Action Command
	char key = 0;
	while (key != 27) // press ESC to exit
	{
		if (key == 32) // press SPACE to pause and store image
		{
			cout << "press Enter tp continue" << endl;
			imwrite("C:\\Users\\NOL\\Desktop\\0.png", pOb0->cvMat);
			imwrite("C:\\Users\\NOL\\Desktop\\1.png", pOb0->cvMat);
			cin.ignore();
			key = 0;
		}
		// Set Action Command to Vimba API
		sys.GetFeatureByName("ActionDeviceKey", pFeature);
		pFeature->SetValue(deviceKey);
		sys.GetFeatureByName("ActionGroupKey", pFeature);
		pFeature->SetValue(groupKey);
		sys.GetFeatureByName("ActionGroupMask", pFeature);
		pFeature->SetValue(groupMask);
		sys.GetFeatureByName("ActionCommand", pFeature);
		pFeature->RunCommand();

		// Get keypress in non-blocking way
		if (_kbhit()) {
			key = _getch();
		}
	}

	// Program runtime ... e.g., Sleep(2000);
	//cin.ignore();

	// When finished , tear down the acquisition chain , close the camera and Vimba
	err = cameras[0]->GetFeatureByName("AcquisitionStop", pFeature);
	err = pFeature->RunCommand();
	err = cameras[1]->GetFeatureByName("AcquisitionStop", pFeature);
	err = pFeature->RunCommand();
	err = cameras[0]->EndCapture();
	err = cameras[0]->FlushQueue();
	err = cameras[0]->RevokeAllFrames();
	err = cameras[0]->Close();
	err = cameras[1]->EndCapture();
	err = cameras[1]->FlushQueue();
	err = cameras[1]->RevokeAllFrames();
	err = cameras[1]->Close();
	err = sys.Shutdown();
	cout << "end" << endl;
	return 0;
}



// int main() {

// 	Mat img0, img1;
// 	img0 = imread("C:\\Users\\NOL\\Desktop\\0.png", -1);
// 	img1 = imread("C:\\Users\\NOL\\Desktop\\1.png", -1);
// 	cvtColor(img0, img0, CV_BayerBG2BGR);
// 	cvtColor(img1, img1, CV_BayerBG2BGR);

// 	img0 = PerfectReflectionAlgorithm(img0);
// 	img1 = PerfectReflectionAlgorithm(img1);
// 	imwrite("C:\\Users\\NOL\\Desktop\\0b.png", img0);
// 	imwrite("C:\\Users\\NOL\\Desktop\\1b.png", img1);

// 	imshow("0", img0);
// 	imshow("1", img1);
// 	waitKey(0);

// 	return 0;
// }

//int main() {
//
//	Mat img;
//
//	img = Mat(2, 2, CV_8UC3, Scalar(70,60,50));
//	cout << img.at<Vec3b>(0, 0) << endl;
//	cout << img.at<Vec3b>(0, 1) << endl;
//	cout << img.at<Vec3b>(1, 0) << endl;
//	cout << img.at<Vec3b>(1, 1) << endl;
//	cout << img.mul(Scalar(1, 2, 3)) << endl;
//
//	img.convertTo(img, CV_16UC3);
//	Mat tt = img.mul(Scalar(10000, 20, 30));
//	cout << tt << endl;
//	tt.convertTo(tt, CV_8UC3);
//	cout << tt << endl;
//
//	return 0;
//}