#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

struct Blob {
    Rect box;
    double area = 0;
    bool found = false;
};

// 在 mask 里找最大色块
Blob findLargestBlob(const Mat& mask, int minArea)
{
    Blob result;
    vector<vector<Point>> contours;
    findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    double maxArea = 0;
    int maxIdx = -1;
    for (size_t i = 0; i < contours.size(); ++i) {
        double a = contourArea(contours[i]);
        if (a > maxArea) {
            maxArea = a;
            maxIdx = (int)i;
        }
    }

    if (maxIdx >= 0 && maxArea > minArea) {
        result.box = boundingRect(contours[maxIdx]);
        result.area = maxArea;
        result.found = true;
    }
    return result;
}

int main()
{
    Mat img = imread("/home/liu_xian/sample/assets/images/green.png");
    if (img.empty()) {
        cerr << "无法读取图片: green.png" << endl;
        return -1;
    }

    Mat hsv;
    cvtColor(img, hsv, COLOR_BGR2HSV);

    Mat maskRed1, maskRed2, maskRed, maskGreen, maskYellow;

    // 红色：H 跨 0 度，分两段
    inRange(hsv, Scalar(0,   100, 70), Scalar(10,  255, 255), maskRed1);
    inRange(hsv, Scalar(170, 100, 70), Scalar(180, 255, 255), maskRed2);
    bitwise_or(maskRed1, maskRed2, maskRed);

    // 绿色
    inRange(hsv, Scalar(35, 43, 46), Scalar(77, 255, 255), maskGreen);

    // 黄色
    inRange(hsv, Scalar(20, 100, 100), Scalar(33, 255, 255), maskYellow);

    // 形态学：开运算去噪 + 闭运算填洞
    Mat kernel = getStructuringElement(MORPH_RECT, Size(5, 5));
    for (Mat* m : {&maskRed, &maskGreen, &maskYellow}) {
        morphologyEx(*m, *m, MORPH_OPEN,  kernel);
        morphologyEx(*m, *m, MORPH_CLOSE, kernel);
    }

    const int minArea = 100;

    Blob red    = findLargestBlob(maskRed,    minArea);
    Blob green  = findLargestBlob(maskGreen,  minArea);
    Blob yellow = findLargestBlob(maskYellow, minArea);

    // 比较面积，判定当前灯色
    string state = "UNKNOWN";
    if (red.found    && red.area    >= green.area && red.area    >= yellow.area) state = "RED";
    if (green.found  && green.area  >  red.area   && green.area  >= yellow.area) state = "GREEN";
    if (yellow.found && yellow.area >  red.area   && yellow.area >  green.area)  state = "YELLOW";

    // 画框
    if (red.found)    rectangle(img, red.box,    Scalar(0, 0, 255),   2);
    if (green.found)  rectangle(img, green.box,  Scalar(0, 255, 0),   2);
    if (yellow.found) rectangle(img, yellow.box, Scalar(0, 255, 255), 2);

    putText(img, state, Point(20, 40), FONT_HERSHEY_SIMPLEX, 1.2, Scalar(255, 255, 255), 2);

    imshow("img", img);
    imshow("red", maskRed);
    imshow("green", maskGreen);
    imshow("yellow", maskYellow);
    waitKey(0);
    return 0;
}