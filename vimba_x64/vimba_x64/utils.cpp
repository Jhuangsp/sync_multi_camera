#include "utils.h"

using namespace cv;

Mat PerfectReflectionAlgorithm(Mat src) {
	int row = src.rows;
	int col = src.cols;
	Mat dst(row, col, CV_8UC3);
	int HistRGB[767] = { 0 };
	int MaxVal = 0;
	for (int i = 0; i < row; i++) {
		for (int j = 0; j < col; j++) {
			MaxVal = max(MaxVal, (int)src.at<Vec3b>(i, j)[0]);
			MaxVal = max(MaxVal, (int)src.at<Vec3b>(i, j)[1]);
			MaxVal = max(MaxVal, (int)src.at<Vec3b>(i, j)[2]);
			int sum = src.at<Vec3b>(i, j)[0] + src.at<Vec3b>(i, j)[1] + src.at<Vec3b>(i, j)[2];
			HistRGB[sum]++;
		}
	}
	int Threshold = 0;
	int sum = 0;
	for (int i = 766; i >= 0; i--) {
		sum += HistRGB[i];
		if (sum > row * col * 0.1) {
			Threshold = i;
			break;
		}
	}
	int AvgB = 0;
	int AvgG = 0;
	int AvgR = 0;
	int cnt = 0;
	for (int i = 0; i < row; i++) {
		for (int j = 0; j < col; j++) {
			int sumP = src.at<Vec3b>(i, j)[0] + src.at<Vec3b>(i, j)[1] + src.at<Vec3b>(i, j)[2];
			if (sumP > Threshold) {
				AvgB += src.at<Vec3b>(i, j)[0];
				AvgG += src.at<Vec3b>(i, j)[1];
				AvgR += src.at<Vec3b>(i, j)[2];
				cnt++;
			}
		}
	}
	AvgB /= cnt;
	AvgG /= cnt;
	AvgR /= cnt;
	src.convertTo(src, CV_16UC3);
	src = src.mul(Scalar(MaxVal, MaxVal, MaxVal));
	src = src.mul(Scalar(1./AvgB, 1./AvgG, 1./AvgR));
	src.convertTo(dst, CV_8UC3);

	return dst;
}

void calibration(string path){
    Mat frame;
    string imgfolder = path; //"calib\\0\\"
    string outfile = imgfolder + "cmx_dis.xml";
    FileStorage fs(outfile, FileStorage::READ);
    
    // File found
    if (fs.isOpened()) {
        // Image buffer
        vector<Mat> images;
        int count = 0;
        
        // Calibration loop
        while (1) {
            // Get an image
            frame = imread(imgfolder + to_string(count) + ".png", -1)
            if(frame.empty())
                break;
            
            // Convert to grayscale
            cv::Mat gray;
            cv::cvtColor(frame, gray, CV_BayerBG2GRAY);
            
            // Detect a chessboard
            cv::Size size(PAT_COLS, PAT_ROWS);
            std::vector<cv::Point2f> corners;
            bool found = cv::findChessboardCorners(gray, size, corners, cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_FAST_CHECK);
            
            // Chessboard detected
            if (found) {
                // Draw it
                cv::drawChessboardCorners(frame, size, corners, found);
                
                // Space key was pressed
                if (key == ' ') {
                    // Add to buffer
                    images.push_back(gray);
                }
            }
            
            // Show the image
            std::ostringstream stream;
            stream << "Captured " << images.size() << " image(s).";
            cv::putText(frame, stream.str(), cv::Point(10, 20), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1, 1);
            cv::imshow("Camera Calibration", frame);
        }
        
        // We have enough samples
        if (images.size() > 4) {
            cv::Size size(PAT_COLS, PAT_ROWS);
            std::vector< std::vector<cv::Point2f> > corners2D;
            std::vector< std::vector<cv::Point3f> > corners3D;
            
            for (size_t i = 0; i < images.size(); i++) {
                // Detect a chessboard
                std::vector<cv::Point2f> tmp_corners2D;
                bool found = cv::findChessboardCorners(images[i], size, tmp_corners2D);
                
                // Chessboard detected
                if (found) {
                    // Convert the corners to sub-pixel
                    cv::cornerSubPix(images[i], tmp_corners2D, cvSize(11, 11), cvSize(-1, -1), cv::TermCriteria(cv::TermCriteria::EPS | cv::TermCriteria::COUNT, 30, 0.1));
                    corners2D.push_back(tmp_corners2D);
                    
                    // Set the 3D position of patterns
                    const float squareSize = CHESS_SIZE;
                    std::vector<cv::Point3f> tmp_corners3D;
                    for (int j = 0; j < size.height; j++) {
                        for (int k = 0; k < size.width; k++) {
                            tmp_corners3D.push_back(cv::Point3f((float)(k*squareSize), (float)(j*squareSize), 0.0));
                        }
                    }
                    corners3D.push_back(tmp_corners3D);
                }
            }
            
            // Estimate camera parameters
            cv::Mat cameraMatrix, distCoeffs;
            std::vector<cv::Mat> rvec, tvec;
            cv::calibrateCamera(corners3D, corners2D, images[0].size(), cameraMatrix, distCoeffs, rvec, tvec);
            std::cout << cameraMatrix << std::endl;
            std::cout << distCoeffs << std::endl;
            
            // Save them
            cv::FileStorage tmp(outfile, cv::FileStorage::WRITE);
            tmp << "intrinsic" << cameraMatrix;
            tmp << "distortion" << distCoeffs;
            tmp.release();
            
            // Reload
            fs.open(outfile, cv::FileStorage::READ);
        }
        
        // Destroy windows
        cv::destroyAllWindows();
    }
	else {
		cout << "Open file error...." << endl;
		getchar();
		return -1;
	}
    
    // Load camera parameters
    cv::Mat cameraMatrix, distCoeffs;
    fs["intrinsic"] >> cameraMatrix;
    fs["distortion"] >> distCoeffs;
    
    // Create undistort map
    cv::Mat mapx, mapy;
    cv::initUndistortRectifyMap(cameraMatrix, distCoeffs, cv::Mat(), cameraMatrix, frame.size(), CV_32FC1, mapx, mapy);
    
    // Main loop
    while (1) {
        // Key input
        int key = cv::waitKey(33);
        if (key == 0x1b) break;
        
        // Get an image
        //cv::Mat image_raw = GP.getImage();
        //Mat image_raw = cap.read();
        Mat image_raw;
        cap >> image_raw;
        
        // Undistort
        cv::Mat image;
        cv::remap(image_raw, image, mapx, mapy, cv::INTER_LINEAR);
        
        // Display the image
        cv::imshow("camera", image);
    }
	return 0
}