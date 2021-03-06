// ar_1117.cpp: 콘솔 응용 프로그램의 진입점을 정의합니다.
//

#include <opencv2\opencv.hpp>
#include <opencv2\highgui\highgui.hpp>
#include <opencv2\features2d\features2d.hpp>
#include <opencv2\features2d.hpp>
#include <opencv2\nonfree\nonfree.hpp>

#include <GL\glut.h>

#include <iostream>
#include <string>

//#pragma comment(lib, "opencv_imgproc2413d.lib")
//#pragma comment(lib, "opencv_highgui2413d.lib")
//#pragma comment(lib, "opencv_nonfree2413d.lib")
//#pragma comment(lib, "opencv_core2413d.lib")
//#pragma comment(lib, "opencv_features2d2413d.lib")
//#pragma comment(lib, "opencv_flann2413d.lib")
//#pragma comment(lib, "opencv_calib3d2413d.lib")

// release mode
#pragma comment(lib, "opencv_imgproc2413.lib")
#pragma comment(lib, "opencv_highgui2413.lib")
#pragma comment(lib, "opencv_nonfree2413.lib")
#pragma comment(lib, "opencv_core2413.lib")
#pragma comment(lib, "opencv_features2d2413.lib")
#pragma comment(lib, "opencv_flann2413.lib")
#pragma comment(lib, "opencv_calib3d2413.lib")

using namespace cv;

#define FRAME_WIDTH 500
#define FRAME_HEIGHT 500
#define CUTLINE 20
#define MINDIST 3
#define MAX_COLOR_COUNT 60

struct xy
{
	int x;
	int y;
};

enum RSP
{
	N, R, S, P
};

RSP currentRSP = N;
GLfloat xRotated = 0, yRotated = 0, zRotated = 0, zRotatedReverse = 360;
int colorCount;

xy startXY = { 0, 0 };
xy endXY = { 200, 200 };

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
SurfFeatureDetector detector(minHessian);
SurfDescriptorExtractor extractor;
VideoCapture capture(0);
// 카메라 파라미터 정보
cv::Mat K = cv::Mat::eye(3, 3, CV_64FC1);

Mat E1 = Mat::eye(4, 4, CV_64FC1);
Mat E2 = Mat::eye(4, 4, CV_64FC1);

float degreeToRadian(float degree);
float radianToDegree(float radian);
void init();
void mydisplay();
void myidle();
void makeE(Mat &E, Mat R, Mat T);
void processVideoCapture();
bool calculatePoseFromH(const cv::Mat &H, cv::Mat &R, cv::Mat &T);
void reshape(int w, int h);
void convertFromCameraToOpenGLProjection(double *mGL);
void findHand(cv::Mat &mat);

int main(int argc, char** argv) {
	//< 1. 마커 영상을 읽어서, keypoint와 descriptor를 추출
	imgMarker = imread("marker3.png", 0); //< 0: 흑백, 1: 컬러
	imgMarker2 = imread("marker4.png", 0);

	std::cout << "Marker size : " << imgMarker.rows << " * " << imgMarker.cols << std::endl;

	// Marker1
	detector.detect(imgMarker, keypointsMarker);
	extractor.compute(imgMarker, keypointsMarker, descriptorsMarker);
	// Marker2
	detector.detect(imgMarker2, keypointsMarker2);
	extractor.compute(imgMarker2, keypointsMarker2, descriptorsMarker2);


	// glut
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(FRAME_WIDTH, FRAME_HEIGHT);
	glutInitWindowPosition(0, 0);
	glutCreateWindow("simple");
	glutDisplayFunc(mydisplay);
	glutIdleFunc(myidle);
	glutReshapeFunc(reshape);

	init();

	glutMainLoop();

	return 0;
}

float degreeToRadian(float degree)
{
	float PI = 3.14159265;

	return degree * PI / 180.0;
}

float radianToDegree(float radian)
{
	float PI = 3.14159265;

	return radian / PI * 180.0;
}

void init()
{
	glClearColor(0.0, 0.0, 0.0, 1.0); // 앞의 3개는 black clear color, 뒤의 1개는 opaque window
	glColor3f(1.0, 1.0, 1.0); // fill / draw with white
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	//glOrtho(-0.1, 1.0, -1.0, 1.0, -1.0, 1.0); // intrinsic parameter
	
	// Init K
	K.at<double>(0, 0) = 788.30328;
	K.at<double>(1, 1) = 783.09240;
	K.at<double>(0, 2) = 631.82488;
	K.at<double>(1, 2) = 361.44291;
}

void mydisplay()
{
	float size = 200;
	float radious = size * 2;

	glClear(GL_COLOR_BUFFER_BIT);

	// 배경 영상 렌더링
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDrawPixels(imgScene.cols, imgScene.rows, GL_BGR_EXT, GL_UNSIGNED_BYTE, (void *)imgScene.data);


	// 마커로부터 카메라로의 변환행렬을 통해 마커좌표계로의 변환
	// 마커 좌표계의 중심에서 객체 렌더링
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	Mat E_1 = E1.t();
	glMultMatrixd((double *)E_1.data);
	
	if (currentRSP == R)
	{
		static int teapotAngle = 0;
		teapotAngle += 12;
		teapotAngle = teapotAngle % 361;

		float teapotMoveX = cos(degreeToRadian(teapotAngle)) * radious;
		float teapotMoveY = sin(degreeToRadian(teapotAngle)) * radious;
		glTranslatef(teapotMoveX, teapotMoveY, 0);

		// rotation about Z axis
		glRotatef(zRotated, 0.0, 0.0, 1.0);
	}

	if (colorCount <= MAX_COLOR_COUNT / 3)
		glColor3f(1, 0, 0);
	else if (colorCount <= MAX_COLOR_COUNT / 3 * 2)
		glColor3f(0, 1, 0);
	else
		glColor3f(0, 0, 1);
	
	if (currentRSP == P)
		glutWireTeapot(size);
	else
		glutSolidTeapot(size);

	// Cube
	// 마커로부터 카메라로의 변환행렬을 통해 마커좌표계로의 변환
	// 마커 좌표계의 중심에서 객체 렌더링
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	Mat E_2 = E2.t();
	glMultMatrixd((double *)E_2.data);

	if (currentRSP == R)
	{
		static int cubeAngle = 360;
		cubeAngle -= 12;
		if (cubeAngle < 0)
			cubeAngle = 360;

		float cubeMoveX = cos(degreeToRadian(cubeAngle)) * radious;
		float cubeMoveY = sin(degreeToRadian(cubeAngle)) * radious;
		glTranslatef(cubeMoveX, cubeMoveY, 0);
		
		// rotation about Z axis
		glRotatef(zRotatedReverse, 0.0, 0.0, 1.0);
	}

	if (colorCount <= MAX_COLOR_COUNT / 3)
		glColor3f(1, 0, 0);
	else if (colorCount <= MAX_COLOR_COUNT / 3 * 2)
		glColor3f(0, 1, 0);
	else
		glColor3f(0, 0, 1);

	if (currentRSP == P)
		glutWireCube(size);
	else
		glutSolidCube(size);


	// 티팟 축
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMultMatrixd((double *)E_1.data);
	//---------------------------- xyz축
	glBegin(GL_LINES);
	// draw line for x axis
	glColor3f(1.0, 0.0, 0.0);
	glVertex3f(0.0, 0.0, 0.0);
	glVertex3f(size, 0.0, 0.0);
	// draw line for y axis
	glColor3f(0.0, 1.0, 0.0);
	glVertex3f(0.0, 0.0, 0.0);
	glVertex3f(0.0, size, 0.0);
	// draw line for Z axis
	glColor3f(0.0, 0.0, 1.0);
	glVertex3f(0.0, 0.0, 0.0);
	glVertex3f(0.0, 0.0, size);
	glEnd();
	//---------------------------- /xyz축

	// Cube 축
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMultMatrixd((double *)E_2.data);
	//---------------------------- xyz축
	glBegin(GL_LINES);
	// draw line for x axis
	glColor3f(1.0, 0.0, 0.0);
	glVertex3f(0.0, 0.0, 0.0);
	glVertex3f(size, 0.0, 0.0);
	// draw line for y axis
	glColor3f(0.0, 1.0, 0.0);
	glVertex3f(0.0, 0.0, 0.0);
	glVertex3f(0.0, size, 0.0);
	// draw line for Z axis
	glColor3f(0.0, 0.0, 1.0);
	glVertex3f(0.0, 0.0, 0.0);
	glVertex3f(0.0, 0.0, size);
	glEnd();
	//---------------------------- /xyz축


	glutSwapBuffers();
}

void myidle()
{
	if (currentRSP == R)
	{
		zRotated += 12.0;
		if (zRotated > 360)
			zRotated = 0;
		zRotatedReverse -= 12.0;
		if (zRotatedReverse < 0)
			zRotatedReverse = 360;
	}
	if (currentRSP == S)
	{
		colorCount++;
		colorCount %= MAX_COLOR_COUNT;
	}

	//glut
	processVideoCapture();
	glutPostRedisplay();
}

void makeE(Mat &E, Mat R, Mat T)
{
	//// R 부분
	//for (int row = 0; row < 3; row++)
	//{
	//	for (int col = 0; col < 3; col++)
	//	{
	//		E.at<double>(row, col) = R.at<double>(row, col);
	//	}
	//}
	//// T 부분
	//for (int row = 0; row < 3; row++)
	//{
	//	E.at<double>(row, 3) = R.at<double>(row, 3);
	//}
	//// R 아래쪽
	//for (int col = 0; col < 3; col++)
	//{
	//	E.at<double>(3, col) = 0;
	//}
	//// T 아래쪽
	//E.at<double>(3, 3) = 1;

	R.copyTo(E.rowRange(0, 3).colRange(0, 3));
	T.copyTo(E.rowRange(0, 3).col(3));
	static double changeCoordArray[4][4] = { { 1, 0, 0, 0 },{ 0, -1, 0, 0 },{ 0, 0, -1, 0 },{ 0, 0, 0, 1 } };
	static Mat changeCoord(4, 4, CV_64FC1, changeCoordArray);

	E = changeCoord * E;

	/*// R 부분
	for (int row = 0; row < 3; row++)
	{
	for (int col = 0; col < 3; col++)
	{
	E.at<double>(col, row) = R.at<double>(col, row);
	}
	}
	// T 부분
	for (int row = 0; row < 3; row++)
	{
	E.at<double>(3, row) = R.at<double>(3, row);
	}
	// R 아래쪽
	for (int col = 0; col < 3; col++)
	{
	E.at<double>(col, 3) = 0;
	}
	// T 아래쪽
	E.at<double>(3, 3) = 1;*/
}

void processVideoCapture()
{
	capture >> imgScene;

	findHand(imgScene);

	cv::Rect rec(startXY.x, startXY.y, endXY.x - startXY.x, endXY.y - startXY.y);
	//cv::Scalar scalar(255);
	cv::rectangle(imgScene, rec, Scalar(0));

	detector.detect(imgScene, keypointsScene);
	extractor.compute(imgScene, keypointsScene, descriptorsScene);

	//< 3. 마커와 웹 캠 영상 매칭
	FlannBasedMatcher matcher;

	//---
	// Marker1
	//---
	std::vector<DMatch> matches;
	matcher.match(descriptorsMarker, descriptorsScene, matches);

	//< 3-1. 애매모호한 매칭을 과감하게 제거한다.
	double max_dist = 0; double min_dist = 100;

	for (int i = 0; i < descriptorsMarker.rows; i++)
	{
		double dist = matches[i].distance;
		if (dist < min_dist)
			min_dist = dist;
		if (dist > max_dist)
			max_dist = dist;
	}

	//printf("-- Max dist : %f \n", max_dist);
	//printf("-- Min dist : %f \n", min_dist);

	std::vector<DMatch> good_matches;

	for (int i = 0; i < descriptorsMarker.rows; i++)
	{
		if (matches[i].distance < MINDIST * min_dist)
			good_matches.push_back(matches[i]);
	}

	//<<<<<<<<<<<<<<<<<<<< 4. homography를 추정
	// Localize the object
	std::vector<Point2f> obj;
	std::vector<Point2f> scene;
	Mat R1;
	Mat T1;

	for (int i = 0; i < good_matches.size(); i++)
	{
		//-- Get the keypoints from the good matches
		obj.push_back(keypointsMarker[good_matches[i].queryIdx].pt);
		scene.push_back(keypointsScene[good_matches[i].trainIdx].pt);
	}

	if (good_matches.size() >= CUTLINE)
	{
		Mat H = findHomography(obj, scene, CV_RANSAC);

		if (calculatePoseFromH(H, R1, T1))
		{
			printf("marker1 ==> R : %d * %d, T : %d * %d \n", R1.rows, R1.cols, T1.rows, T1.cols);
			makeE(E1, R1, T1);
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

	//---
	// Marker2
	//---
	std::vector<DMatch> matches2;
	matcher.match(descriptorsMarker2, descriptorsScene, matches2);

	double max_dist2 = 0; double min_dist2 = 1000;

	for (int i = 0; i < descriptorsMarker2.rows; i++)
	{
		double dist = matches2[i].distance;
		if (dist < min_dist2)
			min_dist = dist;
		if (dist > max_dist2)
			max_dist = dist;
	}

	//printf("-- Max dist : %f \n", max_dist2);
	//printf("-- Min dist : %f \n", min_dist2);

	std::vector<DMatch> good_matches2;

	for (int i = 0; i < descriptorsMarker2.rows; i++)
	{
		if (matches2[i].distance < MINDIST * min_dist2)
			good_matches2.push_back(matches2[i]);
	}


	// Marker2
	// Localize the object
	std::vector<Point2f> obj2;
	std::vector<Point2f> scene2;
	Mat R2;
	Mat T2;

	for (int i = 0; i < good_matches2.size(); i++)
	{
		//-- Get the keypoints from the good matches
		obj2.push_back(keypointsMarker2[good_matches2[i].queryIdx].pt);
		scene2.push_back(keypointsScene[good_matches2[i].trainIdx].pt);
	}

	if (good_matches2.size() >= CUTLINE)
	{
		Mat H = findHomography(obj2, scene2, CV_RANSAC);

		if (calculatePoseFromH(H, R2, T2))
		{
			printf("marker2 ==> R : %d * %d, T : %d * %d \n", R2.rows, R2.cols, T2.rows, T2.cols);
			makeE(E2, R2, T2);
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
	// end

	flip(imgScene, imgScene, 0);
}

bool calculatePoseFromH(const cv::Mat &H, cv::Mat &R, cv::Mat &T)
{
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

void reshape(int w, int h)
{
	double P[16] = { 0 };

	convertFromCameraToOpenGLProjection(P);

	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixd(P);
	glMatrixMode(GL_MODELVIEW);
}

void convertFromCameraToOpenGLProjection(double *mGL)
{
	Mat P = Mat::zeros(4, 4, CV_64FC1);

	const int zNear = 1;
	const int zFar = 1000000;

	P.at<double>(0, 0) = 2 * K.at<double>(0, 0) / FRAME_WIDTH;
	P.at<double>(1, 0) = 0;
	P.at<double>(2, 0) = 0;
	P.at<double>(3, 0) = 0;

	P.at<double>(0, 1) = 0;
	P.at<double>(1, 1) = 2 * K.at<double>(1, 1) / FRAME_HEIGHT;
	P.at<double>(2, 1) = 0;
	P.at<double>(3, 1) = 0;

	P.at<double>(0, 2) = 1 - 2 * K.at<double>(0, 2) / FRAME_WIDTH;
	P.at<double>(1, 2) = -1 + (2 * K.at<double>(1, 2) + 2) / FRAME_HEIGHT;
	P.at<double>(2, 2) = (zNear + zFar) / (zNear - zFar);
	P.at<double>(3, 2) = -1;

	P.at<double>(0, 3) = 0;
	P.at<double>(1, 3) = 0;
	P.at<double>(2, 3) = 2 * zNear * zFar / (zNear - zFar);
	P.at<double>(3, 3) = 0;

	for (int ix = 0; ix < 4; ix++)
	{
		for (int iy = 0; iy < 4; iy++)
		{
			mGL[ix * 4 + iy] = P.at<double>(iy, ix);
		}
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
				;
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
				if (0.3 <= handRatio && handRatio <= 0.4) // 묵
				{
					currentRSP = R;
					mat.at<cv::Vec3b>(y, x) = blueVec;
				}
				else if (0.4 < handRatio && handRatio <= 0.51) // 찌
				{
					currentRSP = S;
					mat.at<cv::Vec3b>(y, x) = greenVec;
				}
				else if (0.51 < handRatio && handRatio <= 0.65) // 빠
				{
					currentRSP = P;
					mat.at<cv::Vec3b>(y, x) = redVec;
				}
				else
				{
					currentRSP = N;
				}
			}
			else // not skin color
			{
				mat.at<cv::Vec3b>(y, x) = resultMat.at<cv::Vec3b>(y, x);
			}
		}
	}
}
