#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/core/mat.hpp>
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

Blob findBlobByBBox(const Mat& mask, int minArea)        
{
    Blob result; 
    vector<Point> pts;
    findNonZero(mask, pts);       
    if (pts.empty()) return result;

    Rect box = boundingRect(pts);     
    double area = countNonZero(mask);
    if (area > minArea)
    {
        result.found = true;
        result.area  = area;
        result.box = box;
    }
    return result;
}

enum class LightState
{
    Unknown,
    Red,
    Green,
    Yellow
};
const char* toString(LightState s)
{
    switch (s) {
        case LightState::Unknown: return "UNKNOWN";
        case LightState::Red:     return "RED";
        case LightState::Green:   return "GREEN";
        case LightState::Yellow:  return "YELLOW";
    }
    return "UNKNOWN";
}

// ostream& operator<<(std::ostream& os, LightState s)
// {
//     return os << toString(s);
// }
class TimeDebouncer
{
    private:
    double holdSec_;
    double candidateSince_ = 0.0;
    LightState confirmed_ =LightState::Unknown;
    LightState candidate_ = LightState::Unknown;

    public:
    explicit TimeDebouncer(double holdSec = 0.15) : holdSec_(holdSec) {}

    //string update(const string& raw, double nowSec)
    LightState update(LightState raw, double nowSec)     
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
    LightState confirmed() const { return confirmed_; }
    LightState candidate() const { return candidate_; }

};


void Morphology(Mat& mask)
{
    Mat kernel = getStructuringElement(MORPH_RECT, Size(5, 5));
    Mat temp;
    dilate(mask, temp, kernel);
    erode(temp, mask, kernel);
}

int main()
{ 

    //连接手机的
    // const string url = "http://10.163.62.219:8080/video";
    // vector<int> params = {
    //     cv::CAP_PROP_OPEN_TIMEOUT_MSEC, 5000,
    //     cv::CAP_PROP_READ_TIMEOUT_MSEC, 5000
    // };
    // VideoCapture cap(url, cv::CAP_FFMPEG, params);
    // if (!cap.isOpened()) {
    //     std::cerr << "Error1\n";
    //     return -1;
    // }
    // cout << "已连接: " << cap.get(cv::CAP_PROP_FRAME_WIDTH) << "x"
    //           << cap.get(cv::CAP_PROP_FRAME_HEIGHT) << "\n";

    VideoCapture cap("/home/liu_xian/sample/assets/videos/practice.mp4");
    if (!cap.isOpened())
    {
	    cout << "Error2" << endl;
	    return -1;
    }

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
        cv::resize(imgroi, img, cv::Size(), s, s,INTER_AREA);
        Rect roi(static_cast<int>(img.cols * 0.10),static_cast<int>(img.rows * 0.10),static_cast<int>(img.cols * 0.80),static_cast<int>(img.rows * 0.80));
        roi &= Rect(0,0,img.cols,img.rows);     //暂定，尝试使用动态roi
        Mat imgHSV;
        GaussianBlur(img, img, Size(5, 5), 0);
        cvtColor(img, imgHSV, COLOR_BGR2HSV);

        Mat maskRed,maskRed_1,maskRed_2,maskGreen,maskYellow;
        inRange(imgHSV, Scalar(0,   130, 120), Scalar(8,   255, 255), maskRed_1);
        inRange(imgHSV, Scalar(172, 130, 120), Scalar(180, 255, 255), maskRed_2);
        bitwise_or(maskRed_1, maskRed_2, maskRed);
        inRange(imgHSV, Scalar(15, 150, 120), Scalar(38, 255, 255), maskYellow);
        inRange(imgHSV, Scalar(40, 80, 80), Scalar(95, 255, 255), maskGreen);
        
        Morphology(maskRed);
        Morphology(maskGreen);
        Morphology(maskYellow);

        const int minArea = 100;
        Blob redBlob =findBlobByBBox(maskRed, minArea);           
        Blob greenBlob = findBlobByBBox(maskGreen, minArea); 
        Blob yellowBlob =findBlobByBBox(maskYellow, minArea); 

        LightState raw =LightState:: Unknown;
        if (redBlob.found && redBlob.area >= greenBlob.area && redBlob.area >= yellowBlob.area) raw = LightState::Red;
        if (greenBlob.found  && greenBlob.area > redBlob.area && greenBlob.area >= yellowBlob.area) raw = LightState::Green;
        if (yellowBlob.found && yellowBlob.area > redBlob.area && yellowBlob.area > greenBlob.area)  raw = LightState::Yellow;
        double now = getTickCount() / getTickFrequency();
        LightState state = debounce.update(raw, now);

        const Blob* best = nullptr;
        Scalar color;

        if (state == LightState::Red)         { best = &redBlob;    color = Scalar(0, 0, 255); }
        else if (state == LightState::Green)  { best = &greenBlob;  color = Scalar(0, 255, 0); }
        else if (state == LightState::Yellow) { best = &yellowBlob; color = Scalar(0, 255, 255); }

        if (best && best->found) 
         {// Rect drawBox = best->box + roi.tl();
        // //  rectangle(img, drawBox, color, 2);
        //     putText(img, toString(state),
        //     Point(drawBox.x, max(drawBox.y - 8, 15)),
        //     FONT_HERSHEY_SIMPLEX, 0.7, color, 2);
            rectangle(img, best->box, color, 2);
    putText(img, toString(state),
        Point(best->box.x, max(best->box.y - 8, 15)),
        FONT_HERSHEY_SIMPLEX, 0.7, color, 2);
        }

        putText(img, toString(state), Point(20, 40), FONT_HERSHEY_SIMPLEX, 1.2, Scalar(255, 255, 255), 2);
        putText(img, format("FPS %.1f", fps), Point(20, 80), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 255, 0), 2);

        int64 t = getTickCount();
        int64 dt = t - t0;
        if (dt > 0) 
        {
            double inst = getTickFrequency() / double(dt);
            fps = (fps == 0.0) ? inst : 0.9 * fps + 0.1 * inst;
        }
        t0 = t;
        imshow("img", img);
        // imshow("red", maskRed);
        // imshow("green", maskGreen);
        // imshow("yellow", maskYellow);
        // if ((waitKey(1) & 0xFF) == 27) break;
        if (waitKey(1000/30) == 27) break;
    }
        destroyAllWindows();
        return 0;
}