#include "stdafx.h"
#include "opencv2\opencv.hpp"
#include "opencv2/highgui/highgui.hpp"

#include <iostream>
#include <string>

#ifdef _DEBUG 
#pragma comment (lib, "opencv_world331d.lib") 
#else 
#pragma comment (lib, "opencv_world331.lib")
#endif

#define PI 3.14159265359

struct xy
{
	int x;
	int y;
};

void mouseCallback(int event, int x, int y, int flags, void * userdata);
void reflectX(cv::Mat &mat);
void reflectY(cv::Mat &mat);
void rotateImage(cv::Mat &mat, double degree);
void scaleImage(cv::Mat &mat, double scale);
cv::Vec3b lerp(cv::Vec3b &p1, cv::Vec3b &p2, float d1);
void findHand(cv::Mat &mat);

bool clicked;
xy startXY;
xy endXY;

// opencv onmouse 이벤트 확인
int main()
{
	cv::VideoCapture cvVideoCapture(0);
	std::string strWndName = "Video Capture";
	cv::Mat matRGBImg;

	if (!cvVideoCapture.isOpened()) {
		std::cerr << "웹캠의 연결을 확인하세요" << std::endl;
		return -1;
	}

	// init show window
	cvVideoCapture >> matRGBImg;
	cv::imshow(strWndName.c_str(), matRGBImg);

	// set mouse callback function
	cv::setMouseCallback("Video Capture", mouseCallback, NULL);

	while (true) {
		cvVideoCapture >> matRGBImg;

		if (!clicked && startXY.x != endXY.x && startXY.y != endXY.y)
		{
			// functions
			//reflectX(matRGBImg);
			//reflectY(matRGBImg);
			//rotateImage(matRGBImg, 90);
			//scaleImage(matRGBImg, 3);
			findHand(matRGBImg);

			// draw rectangle
			cv::Rect rec(startXY.x, startXY.y, endXY.x - startXY.x, endXY.y - startXY.y);
			cv::Scalar scalar(255);
			cv::rectangle(matRGBImg, rec, scalar);
		}

		cv::imshow(strWndName.c_str(), matRGBImg);

		char ch = cv::waitKey(5);

		if (ch == 27)
			break;
	}

	cvVideoCapture.release();
	return 0;
}

void mouseCallback(int event, int x, int y, int flags, void * userdata)
{
	if (event == CV_EVENT_LBUTTONDOWN)
	{
		clicked = true;
		startXY = { x,y };
		std::cout << startXY.x << " " << startXY.y << std::endl;
		std::cout << endXY.x << " " << endXY.y << std::endl;
	}
	else if (event == CV_EVENT_LBUTTONUP)
	{
		clicked = false;
		endXY = { x,y };
	}
}

void reflectX(cv::Mat &mat)
{
	int baseY = (startXY.y + endXY.y) / 2;

	for (int i = startXY.x; i <= endXY.x; i++)
	{
		for (int j = startXY.y; j <= baseY; j++)
		{
			// two point in up and down
			cv::Point point1(i, j);
			cv::Point point2(i, endXY.y - (j - startXY.y));
			cv::Point t();

			//swap two point's color
			cv::Vec3b tempVec3b = mat.at<cv::Vec3b>(point1);
			mat.at<cv::Vec3b>(point1) = mat.at<cv::Vec3b>(point2);
			mat.at<cv::Vec3b>(point2) = tempVec3b;
		}
	}
}

void reflectY(cv::Mat &mat)
{
	int baseX = (startXY.x + endXY.x) / 2;

	for (int i = startXY.x; i <= baseX; i++)
	{
		for (int j = startXY.y; j <= endXY.y; j++)
		{
			// two point in up and down
			cv::Point point1(i, j);
			cv::Point point2(endXY.x - (i - startXY.x), j);
			cv::Point t();

			//swap two point's color
			cv::Vec3b tempVec3b = mat.at<cv::Vec3b>(point1);
			mat.at<cv::Vec3b>(point1) = mat.at<cv::Vec3b>(point2);
			mat.at<cv::Vec3b>(point2) = tempVec3b;
		}
	}
}

void rotateImage(cv::Mat &mat, double degree)
{
	double radian = (PI / 180.0) * degree;
	xy centerXY = { (startXY.x + endXY.x) / 2, (startXY.y + endXY.y) / 2 };
	cv::Mat tempMat = mat.clone();
	cv::Vec3b blackVec(0, 0, 0);

	for (int i = startXY.x; i <= endXY.x; i++)
	{
		for (int j = startXY.y; j <= endXY.y; j++)
		{
			cv::Point pointFrom(i, j);
			mat.at<cv::Vec3b>(pointFrom) = blackVec; // fill rectangle with black color
		}
	}

	for (int i = startXY.x; i <= endXY.x; i++)
	{
		for (int j = startXY.y; j <= endXY.y; j++)
		{
			cv::Point pointFrom(i, j);
			int rotatedX = cos(radian) * (i - centerXY.x) - sin(radian) * (j - centerXY.y) + centerXY.x;
			int rotatedY = sin(radian) * (i - centerXY.x) + cos(radian) * (j - centerXY.y) + centerXY.y;
			cv::Point pointTo(rotatedX, rotatedY);

			if (0 <= rotatedX && rotatedX < mat.cols && 0 <= rotatedY && rotatedY < mat.rows)
			{
				mat.at<cv::Vec3b>(pointTo) = tempMat.at<cv::Vec3b>(pointFrom);
			}
		}
	}
}

void scaleImage(cv::Mat &mat, double scale)
{
	cv::Mat tempMat = mat.clone();
	cv::resize(tempMat, tempMat, cv::Size(mat.cols * scale, mat.rows * scale), 0, 0, CV_INTER_LINEAR);

	xy scaledStartXY = { startXY.x * scale, startXY.y * scale }; // 원본 이미지의 사각형 시작점이 스케일되었을 때 존재하는 장소
	xy scaledEndXY = { endXY.x * scale, endXY.y * scale };
	xy scaledCenterXY = { (scaledStartXY.x + scaledEndXY.x) / 2, (scaledStartXY.y + scaledEndXY.y) / 2 };

	// 확대된 이미지에서 원본 이미지의 사각형 크기에 맞춰서 시작점을 설정
	xy newScaledStartXY = { scaledCenterXY.x - (endXY.x - startXY.x) / 2, scaledCenterXY.y - (endXY.y - startXY.y) / 2 };
	xy newScaledEndXY = { scaledCenterXY.x + (endXY.x - startXY.x) / 2, scaledCenterXY.y + (endXY.y - startXY.y) / 2 };

	int matX = startXY.x;
	int matY = startXY.y;
	for (int x = newScaledStartXY.x; x <= newScaledEndXY.x; x++)
	{
		for (int y = newScaledStartXY.y; y <= newScaledEndXY.y; y++)
		{
			mat.at<cv::Vec3b>(matY, matX) = tempMat.at<cv::Vec3b>(y, x);
			matY++;
		}
		matY = startXY.y;
		matX++;
	}
}

void findHand(cv::Mat &mat)
{
	cv::Mat tempMat;
	cv::cvtColor(mat, tempMat, CV_BGR2YCrCb);

	cv::inRange(tempMat, cv::Scalar(0, 133, 77), cv::Scalar(255, 173, 127), tempMat);

	cv::Mat resultMat(tempMat.size(), CV_8UC3, cv::Scalar(0));

	cv::add(mat, cv::Scalar(0), resultMat, tempMat);

	int allPixcelCount = 0;
	int handColorCount = 0;

	for (int x = startXY.x; x < endXY.x; x++)
	{
		for (int y = startXY.y; y < endXY.y; y++)
		{
			allPixcelCount++;

			cv::Vec3b tempVec = resultMat.at<cv::Vec3b>(y, x);
			cv::Vec3b blueVec(255, 0, 0);
			if (tempVec[0] > 0 || tempVec[1] > 0 || tempVec[2] > 0) // skin color
			{
				handColorCount++;


			}
			else // not skin color
			{
			}
		}
	}
	double handRatio = handColorCount / (double)allPixcelCount;
	std::cout << "ratio : " << handRatio << std::endl;

	for (int x = startXY.x; x < endXY.x; x++)
	{
		for (int y = startXY.y; y < endXY.y; y++)
		{
			cv::Vec3b tempVec = resultMat.at<cv::Vec3b>(y, x);
			cv::Vec3b blueVec(255, 0, 0);
			cv::Vec3b greenVec(0, 255, 0);
			cv::Vec3b redVec(0, 0, 255);
			if (tempVec[0] > 0 || tempVec[1] > 0 || tempVec[2] > 0) // skin color
			{
				if (0.2 <= handRatio && handRatio <= 0.25)
					mat.at<cv::Vec3b>(y, x) = blueVec;
				else if (0.25 <= handRatio && handRatio <= 0.3)
					mat.at<cv::Vec3b>(y, x) = greenVec;
				else if (0.3 <= handRatio && handRatio <= 0.42)
					mat.at<cv::Vec3b>(y, x) = redVec;
			}
			else // not skin color
			{
				mat.at<cv::Vec3b>(y, x) = resultMat.at<cv::Vec3b>(y, x);
			}
		}
	}
}