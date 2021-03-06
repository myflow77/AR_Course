// ar_1110.cpp: 콘솔 응용 프로그램의 진입점을 정의합니다.
//

#include <opencv2\opencv.hpp>
#include <opencv2\highgui\highgui.hpp>
#include <opencv2\features2d\features2d.hpp>
#include <opencv2\features2d.hpp>
#include <opencv2\nonfree\nonfree.hpp>

#include <iostream>
#include <string>

/*#pragma comment(lib, "opencv_imgproc2413d.lib")
#pragma comment(lib, "opencv_highgui2413d.lib")
#pragma comment(lib, "opencv_nonfree2413d.lib")
#pragma comment(lib, "opencv_core2413d.lib")
#pragma comment(lib, "opencv_features2d2413d.lib")
#pragma comment(lib, "opencv_flann2413d.lib")
#pragma comment(lib, "opencv_calib3d2413d.lib")*/

#pragma comment(lib, "opencv_imgproc2413.lib")
#pragma comment(lib, "opencv_highgui2413.lib")
#pragma comment(lib, "opencv_nonfree2413.lib")
#pragma comment(lib, "opencv_core2413.lib")
#pragma comment(lib, "opencv_features2d2413.lib")
#pragma comment(lib, "opencv_flann2413.lib")
#pragma comment(lib, "opencv_calib3d2413.lib")

using namespace cv;

Mat imgMarker;	//< 마커 영상
Mat imgMarker2;
Mat imgScene;	//< 웹 캠 영상

std::vector<KeyPoint> keypointsMarker;	//< 마커 keypoints
std::vector<KeyPoint> keypointsMarker2;
std::vector<KeyPoint> keypointsScene;	//< 웹 캠 keypoints

Mat descriptorsMarker;	//< 마커 descriptor
Mat descriptorsMarker2;
Mat descriptorsScene;	//< 웹 캠 descriptor

int minHessian = 400;
//SurfFeatureDetector detector(minHessian);
SurfDescriptorExtractor extractor;
//ORB detector;
OrbFeatureDetector detector;
VideoCapture capture(0);

bool calculatePoseFromH(const cv::Mat &H, cv::Mat &R, cv::Mat &T);

int main(int argc, char** argv) {
	//< 1. 마커 영상을 읽어서, keypoint와 descriptor를 추출
	imgMarker = imread("marker1.png", 0); //< 0: 흑백, 1: 컬러
	imgMarker2 = imread("marker2.png", 0);

	std::cout << "Marker size : " << imgMarker.rows << " * " << imgMarker.cols << std::endl;

	// Marker1
	detector.detect(imgMarker, keypointsMarker);
	extractor.compute(imgMarker, keypointsMarker, descriptorsMarker);
	// Marker2
	detector.detect(imgMarker2, keypointsMarker2);
	extractor.compute(imgMarker2, keypointsMarker2, descriptorsMarker2);

	while (true) {
		//< 2. 웹 캠 영상을 읽어서, keypoint와 descriptor를 추출
		capture >> imgScene;

		detector.detect(imgScene, keypointsScene);
		extractor.compute(imgScene, keypointsScene, descriptorsScene);

		//< 3. 마커와 웹 캠 영상 매칭
		FlannBasedMatcher matcher;

		// Marker1
		std::vector<DMatch> matches;
		matcher.match(descriptorsMarker, descriptorsScene, matches);
		// Marker2
		std::vector<DMatch> matches2;
		matcher.match(descriptorsMarker2, descriptorsScene, matches2);

		//< 3-1. 애매모호한 매칭을 과감하게 제거한다.
		// Marker1
		double max_dist = 0; double min_dist = 1000;

		for (int i = 0; i < descriptorsMarker.rows; i++)
		{
			double dist = matches[i].distance;
			if (dist < min_dist)
				min_dist = dist;
			if (dist > max_dist)
				max_dist = dist;
		}

		printf("-- Max dist : %f \n", max_dist);
		printf("-- Min dist : %f \n", min_dist);

		std::vector<DMatch> good_matches;

		for (int i = 0; i < descriptorsMarker.rows; i++)
		{
			if (matches[i].distance < 3 * min_dist)
				good_matches.push_back(matches[i]);
		}
		// Marker2
		double max_dist2 = 0; double min_dist2 = 1000;

		for (int i = 0; i < descriptorsMarker2.rows; i++)
		{
			double dist = matches2[i].distance;
			if (dist < min_dist2)
				min_dist = dist;
			if (dist > max_dist2)
				max_dist = dist;
		}

		printf("-- Max dist : %f \n", max_dist2);
		printf("-- Min dist : %f \n", min_dist2);

		std::vector<DMatch> good_matches2;

		for (int i = 0; i < descriptorsMarker2.rows; i++)
		{
			if (matches2[i].distance < 5 * min_dist2)
				good_matches2.push_back(matches2[i]);
		}

		//<<<<<<<<<<<<<<<<<<<< 4. homography를 추정
		// Marker1
		// Localize the object
		std::vector<Point2f> obj;
		std::vector<Point2f> scene;

		for (int i = 0; i < good_matches.size(); i++)
		{
			//-- Get the keypoints from the good matches
			obj.push_back(keypointsMarker[good_matches[i].queryIdx].pt);
			scene.push_back(keypointsScene[good_matches[i].trainIdx].pt);
		}

		if (obj.size() != 0 && scene.size() != 0)
		{
			Mat H = findHomography(obj, scene, CV_RANSAC);
			Mat R;
			Mat T;
			if (calculatePoseFromH(H, R, T))
			{
				std::cout << "hello" << std::endl;
				imshow("Marker1 R", R);
				imshow("Marker1 T", T);
			}
			//-- Get the corners from the image_1 ( the object to be "detected" )
			std::vector<Point2f> obj_corners(4);
			obj_corners[0] = cvPoint(0, 0);
			obj_corners[1] = cvPoint(imgMarker.cols, 0);
			obj_corners[2] = cvPoint(imgMarker.cols, imgMarker.rows);
			obj_corners[3] = cvPoint(0, imgMarker.rows);
			std::vector<Point2f> scene_corners(4);

			perspectiveTransform(obj_corners, scene_corners, H);

			//< 5. 4각형을 그리면 된다.

			//-- Draw lines between the corners (the mapped object in the scene - image_2 )
			line(imgScene, scene_corners[0], scene_corners[1], Scalar(0, 255, 0), 4);
			line(imgScene, scene_corners[1], scene_corners[2], Scalar(0, 255, 0), 4);
			line(imgScene, scene_corners[2], scene_corners[3], Scalar(0, 255, 0), 4);
			line(imgScene, scene_corners[3], scene_corners[0], Scalar(0, 255, 0), 4);
		}
		// Marker2
		// Localize the object
		std::vector<Point2f> obj2;
		std::vector<Point2f> scene2;

		for (int i = 0; i < good_matches2.size(); i++)
		{
			//-- Get the keypoints from the good matches
			obj2.push_back(keypointsMarker2[good_matches2[i].queryIdx].pt);
			scene2.push_back(keypointsScene[good_matches2[i].trainIdx].pt);
		}

		if (obj2.size() != 0 && scene2.size() != 0)
		{
			Mat H = findHomography(obj2, scene2, CV_RANSAC);
			Mat R;
			Mat T;
			if (calculatePoseFromH(H, R, T))
			{
				imshow("Marker2 R", R);
				imshow("Marker2 T", T);
			}

			//-- Get the corners from the image_1 ( the object to be "detected" )
			std::vector<Point2f> obj_corners(4);
			obj_corners[0] = cvPoint(0, 0);
			obj_corners[1] = cvPoint(imgMarker2.cols, 0);
			obj_corners[2] = cvPoint(imgMarker2.cols, imgMarker2.rows);
			obj_corners[3] = cvPoint(0, imgMarker2.rows);
			std::vector<Point2f> scene_corners(4);

			perspectiveTransform(obj_corners, scene_corners, H);

			//< 5. 4각형을 그리면 된다.

			//-- Draw lines between the corners (the mapped object in the scene - image_2 )
			line(imgScene, scene_corners[0], scene_corners[1], Scalar(255, 0, 0), 4);
			line(imgScene, scene_corners[1], scene_corners[2], Scalar(255, 0, 0), 4);
			line(imgScene, scene_corners[2], scene_corners[3], Scalar(255, 0, 0), 4);
			line(imgScene, scene_corners[3], scene_corners[0], Scalar(255, 0, 0), 4);
		}

		//--------------------------------- Show detected matches
		imshow("Good Matches & Object detection", imgScene);

		char ch = waitKey(100);

		if (ch == 27)
			break;
	}
	return 0;
}

bool calculatePoseFromH(const cv::Mat &H, cv::Mat &R, cv::Mat &T)
{
	// 카메라 파라미터 정보
	cv::Mat K = cv::Mat::eye(3, 3, CV_64FC1);
	K.at<double>(0, 0) = 788.30328;
	K.at<double>(1, 1) = 783.09240;
	K.at<double>(0, 2) = 631.82488;
	K.at<double>(1, 2) = 361.44291;
	//---
	cv::Mat InvK = K.inv();
	cv::Mat InvH = InvK * H;
	cv::Mat h1 = H.col(0);
	cv::Mat h2 = H.col(1);
	cv::Mat h3 = H.col(2);

	double dbNormV1 = cv::norm(InvH.col(0));

	if (dbNormV1 != 0)
	{
		InvK /= dbNormV1;

		cv::Mat r1 = InvK * h1;
		cv::Mat r2 = InvK * h2;
		cv::Mat r3 = r1.cross(r2);

		T = InvK * h3;

		cv::Mat R1 = cv::Mat::zeros(3, 3, CV_64FC1);

		r1.copyTo(R1.rowRange(cv::Range::all()).col(0));
		r2.copyTo(R1.rowRange(cv::Range::all()).col(1));
		r3.copyTo(R1.rowRange(cv::Range::all()).col(2));

		cv::SVD svd(R1);

		R = svd.u * svd.vt;

		return true;
	}
	else
	{
		return false;
	}
}