#include <iostream>
#include <opencv2/videoio.hpp>
#include <vector>
#include <opencv2/opencv.hpp>
#include <string>
using namespace std;
using namespace cv;
struct Blob 
{   
    Rect box;
    double area = 0;
    bool found = false;
};

Blob findBlobByBBox(const Mat& mask, int minArea)           //找出面积最大的色块
{
    Blob result;


    // vector<vector<Point>> contours;
    // findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    // double maxArea = 0;
    // int maxIdx = -1;
    // for (size_t i = 0; i < contours.size(); i++)
    // {
    //     double area = contourArea(contours[i]);
    //     if (area > maxArea)
    //     {
    //         maxArea = area;
    //         maxIdx = (int)i;
    //     }
    // }
    // if(maxArea > minArea && maxIdx >= 0)
    //     {
    //         result.found = true;
    //         result.area = maxArea;
    //         result.Box = boundingRect(contours[maxIdx]);
    //     }
    // return result;  
    vector<Point> pts;
    findNonZero(mask, pts);          // 所有白色像素的坐标
    if (pts.empty()) return result;

    Rect box = boundingRect(pts);    // 一次性框住全部
    double area = countNonZero(mask);// 白色像素总数

    if (area > minArea)
    {
        result.found = true;
        result.area  = area;
        result.box = box;
    }
    return result;
}

class TimeDebouncer
{
    private:
    double holdSec_;
    double candidateSince_ = 0.0;
    string confirmed_ = "UNKNOWN";
    string candidate_ = "UNKNOWN";

    public:
    explicit TimeDebouncer(double holdSec = 0.15) : holdSec_(holdSec) {}

    string update(const string& raw, double nowSec)     
    {
        if (raw != candidate_) 
        {
            candidate_ = raw;
            candidateSince_ = nowSec;
        }
        if (nowSec - candidateSince_ >= holdSec_)
        {
            confirmed_ = candidate_;
        }
         return confirmed_;
    }
    string confirmed()const { return confirmed_; }
    string candidate() const { return candidate_; }

};


void Morphology(const Mat& mask, Mat& dst)
{
    Mat kernel = getStructuringElement(MORPH_RECT, Size(5, 5));
    dilate(mask, dst, kernel);
    erode(dst, dst, kernel);
}
int main()
{ 

    //视频链接
    const string url = "http://10.163.62.219:8080/video";
    vector<int> params = {
        cv::CAP_PROP_OPEN_TIMEOUT_MSEC, 5000,
        cv::CAP_PROP_READ_TIMEOUT_MSEC, 5000
    };
    VideoCapture cap(url, cv::CAP_FFMPEG, params);
    if (!cap.isOpened()) {
        std::cerr << "Error1\n";
        return -1;
    }
    cout << "已连接: " << cap.get(cv::CAP_PROP_FRAME_WIDTH) << "x"
              << cap.get(cv::CAP_PROP_FRAME_HEIGHT) << "\n";




    Mat imgroi, img;
    const int TARGET_W = 640; 
    double fps = 0.0;
    int64 t0 = cv::getTickCount();
    TimeDebouncer debounce(0.15); 
    while (true) 
    {
        if (!cap.read(imgroi) || imgroi.empty()) 
        {
            std::cerr << "读帧失败\n";
            break;
        }

        double s = static_cast<double>(TARGET_W) / imgroi.cols;
        cv::resize(imgroi, img, cv::Size(), s, s);
    Mat imgHSV;
    GaussianBlur(img, img, Size(5, 5), 0);
    cvtColor(img, imgHSV, COLOR_BGR2HSV);

    Mat maskRed,maskRed_1,maskRed_2,maskGreen,maskYellow;
    // inRange(imgHSV,Scalar(0,100,70),Scalar(10,255,255),maskRed_1);
    // inRange(imgHSV,Scalar(170,100,70),Scalar(180,255,255),maskRed_2);
    // bitwise_or(maskRed_1,maskRed_2,maskRed);
    // inRange(imgHSV,Scalar(35,43,46),Scalar(77,255,255),maskGreen);
    // inRange(imgHSV, Scalar(20, 100, 100), Scalar(33, 255, 255), maskYellow);
    inRange(imgHSV, Scalar(0,   130, 120), Scalar(8,   255, 255), maskRed_1);
    inRange(imgHSV, Scalar(172, 130, 120), Scalar(180, 255, 255), maskRed_2);
    bitwise_or(maskRed_1, maskRed_2, maskRed);
    inRange(imgHSV, Scalar(15, 150, 120), Scalar(38, 255, 255), maskYellow);
    inRange(imgHSV, Scalar(40, 80, 80), Scalar(95, 255, 255), maskGreen);
    
    Morphology(maskRed, maskRed);
    Morphology(maskGreen, maskGreen);
    Morphology(maskYellow, maskYellow);

    const int minArea = 100;
    Blob redBlob =findBlobByBBox(maskRed, minArea);           
    Blob greenBlob = findBlobByBBox(maskGreen, minArea); 
    Blob yellowBlob =findBlobByBBox(maskYellow, minArea); 

    string raw = "UNKNOWN";
    if (redBlob.found && redBlob.area >= greenBlob.area && redBlob.area >= yellowBlob.area) raw = "RED";
    if (greenBlob.found  && greenBlob.area > redBlob.area && greenBlob.area >= yellowBlob.area) raw = "GREEN";
    if (yellowBlob.found && yellowBlob.area > redBlob.area && yellowBlob.area > greenBlob.area)  raw = "YELLOW";
    double now = cv::getTickCount() / cv::getTickFrequency();
    const string& state = debounce.update(raw, now);
    if (redBlob.found)    
        rectangle(img, redBlob.box,    Scalar(0, 0, 255),   2);
    if (greenBlob.found)  
        rectangle(img, greenBlob.box,  Scalar(0, 255, 0),   2);
    if (yellowBlob.found) 
        rectangle(img, yellowBlob.box, Scalar(0, 255, 255), 2);

    putText(img, state, Point(20, 40), FONT_HERSHEY_SIMPLEX, 1.2, Scalar(255, 255, 255), 2);
    putText(img, format("FPS %.1f", fps), Point(20, 80), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 255, 0), 2);

    int64 t = getTickCount();
    fps = getTickFrequency() / double(t - t0);
    t0 = t;
    imshow("img", img);
    // imshow("red", maskRed);
    // imshow("green", maskGreen);
    // imshow("yellow", maskYellow);
    if ((waitKey(1) & 0xFF) == 27) break;
    
}
    destroyAllWindows();
}