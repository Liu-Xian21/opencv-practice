#include <opencv2/opencv.hpp>
#include <iostream>
using namespace cv;
using namespace std;
int main() {
	string path = "1.jpg";
	Mat img = imread(path);
	if(img.empty()){
    cout << "图片读取失败！路径错误或者文件不存在！" << endl;
    return -1;
	}
	Mat imgresize;
	cout << img.size() << endl;
	resize(img, imgresize, Size(), 0.5, 0.5);
	Rect roi(200, 100, 250, 400);
	Mat imgecrop = img(roi);
	imshow("Image", img);
	imshow("Image resize", imgresize);
	imshow("ImageCrop", imgecrop);
	waitKey(0);
}