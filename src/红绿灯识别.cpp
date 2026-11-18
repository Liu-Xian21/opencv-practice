#include <iostream>
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include "SerialPort.h"
using namespace std;
using namespace cv;
struct Blob {
    Rect box;
    double area = 0;
    bool found = false;
};

Blob findBlobByBBox(const Mat& mask, int minArea)       //找像素然后拟框，不需要修改了吧。。
{
    Blob result;
    vector<Point> pts;
    findNonZero(mask, pts);     //pts就是接收白色像素的坐标的，所以得知这是二值化图像
    if (pts.empty()) return result;

    Rect box = boundingRect(pts);
    double area = countNonZero(mask);
    if (area > minArea) {
        result.found = true;
        result.area  = area;
        result.box   = box;
    }
    return result;
}


// ===================== LED 点阵圆点检测 =====================
struct DotGrid {
    bool found = false;
    int count = 0;
    Rect box;
};

// 在 ROI 里找"圆形亮斑"，用形状而不是颜色确认 LED 点阵
DotGrid detectDotGrid(const Mat& roiBgr)
{
    DotGrid grid;

    Mat gray;
    cvtColor(roiBgr, gray, COLOR_BGR2GRAY);

    Mat bin;
    threshold(gray, bin, 160, 255, THRESH_BINARY);

    vector<vector<Point>> contours;
    findContours(bin, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    vector<Point> centers;
    float maxR = 0.0f;

    for (auto& c : contours) {
        double area = contourArea(c);
        double peri = arcLength(c, true);
        if (peri <= 0 || area < 5) continue;

        // 圆度：4πA/P²，越接近 1 越圆
        double circularity = 4.0 * CV_PI * area / (peri * peri);
        if (circularity < 0.25) continue;

        Point2f center;
        float radius;
        minEnclosingCircle(c, center, radius);
        if (radius < 1.0f || radius > 25.0f) continue;

        centers.emplace_back(cvRound(center.x), cvRound(center.y));
        maxR = std::max(maxR, radius);
    }

    if ((int)centers.size() < 8) return grid;

    Rect box = boundingRect(centers);
    int m = std::max(2, (int)std::ceil(maxR));
    box.x = std::max(0, box.x - m);
    box.y = std::max(0, box.y - m);
    box.width = std::min(roiBgr.cols - box.x, box.width + 2 * m);
    box.height = std::min(roiBgr.rows - box.y, box.height + 2 * m);

    grid.found = true;
    grid.count = (int)centers.size();
    grid.box = box;
    return grid;
}

// 用点阵区域内的"主色相"判色，不依赖面积大小
LightState classifyGridColor(const Mat& hsv, const DotGrid& grid)
{
    if (!grid.found) return LightState::Unknown;

    Mat region = Mat::zeros(hsv.size(), CV_8UC1);
    rectangle(region, grid.box, Scalar(255), FILLED);

    // 只保留有颜色的亮像素，排除白色中心
    Mat bright;
    inRange(hsv, Scalar(0, 15, 100), Scalar(180, 255, 255), bright);
    Mat m = region & bright;

    if (countNonZero(m) < 15) return LightState::Unknown;

    int histSize = 180;
    float range[] = {0, 180};
    const float* ranges[] = {&range[0]};
    int channels[] = {0};
    Mat hist;
    calcHist(&hsv, 1, channels, m, hist, 1, &histSize, ranges);

    Mat histS;
    GaussianBlur(hist, histS, Size(5, 1), 0, 0, BORDER_REPLICATE);

    Point maxLoc;
    minMaxLoc(histS, nullptr, nullptr, nullptr, &maxLoc);
    int domH = maxLoc.y;

    if (domH <= 10 || domH >= 170) return LightState::Red;
    if (domH <= 40) return LightState::Yellow;
    if (domH <= 90) return LightState::Green;
    return LightState::Unknown;
}

struct Detection 
{
    LightState raw = LightState::Unknown;       //原始状态是unknown。
    double confidence = 0.0;
    Rect box;
    const char* reason = "unknown";
};

Detection decide(const Blob& red, const Blob& green, const Blob& yellow, int minArea)       //这个我怎么看不懂。这个应该是辨别颜色的然后计入候选的
{
    Detection d;

    struct Cand { bool ok; LightState s; double area; const Blob* b; };
    Cand cands[3] = 
    {
        {red.found    && red.area    > minArea, LightState::Red,    red.area,    &red},
        {green.found  && green.area  > minArea, LightState::Green,  green.area,  &green},
        {yellow.found && yellow.area > minArea, LightState::Yellow, yellow.area, &yellow}
    };

    Cand* best = nullptr;       //定义最佳颜色
    Cand* second = nullptr;     //然后这个定义次佳颜色的
    for (auto& c : cands) 
    {
        if (!c.ok) continue;
        if (!best || c.area > best->area) { second = best; best = &c; }
        else if (!second || c.area > second->area) { second = &c; }
    }

    if (!best) { d.reason = "no blob"; return d; }

    // 主色优势：最大 / 次大 必须 >= 1.5
    if (second && best->area / second->area < 1.5) 
    {
        d.reason = "color ambiguous";
        return d;
    }

    d.raw = best->s;
    d.box = best->b->box;
    d.confidence = second ? 1.0 - second->area / best->area : 1.0;
    d.reason = "ok";
    return d;
}

// ===================== SequenceValidator =====================
class SequenceValidator 
{
    public:
        explicit SequenceValidator(double holdSec = 0.15,       //正常的切换的防抖时间，看看可不可以修改
                                   double graceSec = 0.50,      //短暂遮挡宽限时间，修改？
                                   double unknownTimeoutSec = 1.0,      //长时间没有结果，过了这个时间就显示unknown
                                   double longHoldSec = 0.60)           //非法跳变的容忍时间
                                    : holdSec_(holdSec), graceSec_(graceSec),
                                    unknownTimeoutSec_(unknownTimeoutSec), longHoldSec_(longHoldSec) {}

        LightState update(LightState detected, double nowSec)
        {
            // 1. 没检测到：短暂宽限，长期超时
            if (detected == LightState::Unknown) 
            {
                if (nowSec - lastValidTime_ < graceSec_)
                    return confirmed_;
                if (nowSec - lastValidTime_ > unknownTimeoutSec_)
                    setConfirmed(LightState::Unknown, nowSec);
                return confirmed_;
            }

            lastValidTime_ = nowSec;

            // 2. 启动：第一个稳定检测直接接受
            if (confirmed_ == LightState::Unknown)
                return tryConfirm(detected, nowSec, holdSec_);

            // 3. 和当前一致：保持
            if (detected == confirmed_) 
            {
                candidate_ = detected;
                candidateSince_ = nowSec;
                return confirmed_;
            }

            // 4. 符合序列：正常切换
            if (detected == nextState(confirmed_))
                return tryConfirm(detected, nowSec, holdSec_);

            // 5. 非法跳变：先忽略；持续 longHoldSec 才容忍
            return tryConfirm(detected, nowSec, longHoldSec_);
        }

        double stateAge(double nowSec) const { return nowSec - stateSince_; }

        // 距离"红灯开始"的最短/最长剩余时间（规则：绿 3~5s，黄 2s）
        double timeUntilRedMin(double nowSec) const
        {
            double age = stateAge(nowSec);
            switch (confirmed_) {
                case LightState::Green:  return std::max(0.0, 3.0 - age) + 2.0;
                case LightState::Yellow: return std::max(0.0, 2.0 - age);
                case LightState::Red:    return 0.0;
                default:                 return 0.0;
            }
        }

        double timeUntilRedMax(double nowSec) const
        {
            double age = stateAge(nowSec);
            switch (confirmed_) {
                case LightState::Green:  return std::max(0.0, 5.0 - age) + 2.0;
                case LightState::Yellow: return std::max(0.0, 2.0 - age);
                case LightState::Red:    return 0.0;
                default:                 return 0.0;
            }
        }

        // 只有"亲眼看到绿灯开始"且剩余时间足够，才允许起步
        bool safeToGo(double nowSec, double requiredSec) const
        {
            if (confirmed_ != LightState::Green) return false;
            if (!stateKnownFromStart_) return false;
            return timeUntilRedMin(nowSec) >= requiredSec;
        }

        // 红灯时：距离下一个绿灯的最短/最长剩余时间
        double timeUntilGreenMin(double nowSec) const
        {
            if (confirmed_ != LightState::Red) return 0.0;
            return std::max(0.0, 3.0 - stateAge(nowSec));
        }

        double timeUntilGreenMax(double nowSec) const
        {
            if (confirmed_ != LightState::Red) return 0.0;
            return std::max(0.0, 5.0 - stateAge(nowSec));
        }

// 是否"亲眼看到这个状态的开始"
bool stateKnownFromStart() const { return stateKnownFromStart_; }
private:
    void setConfirmed(LightState s, double nowSec)
    {
        if (confirmed_ == s) return;

        bool fromTransition = (confirmed_ != LightState::Unknown);
        confirmed_ = s;
        stateSince_ = nowSec;
        stateKnownFromStart_ = fromTransition;
    }

    LightState tryConfirm(LightState detected, double nowSec, double hold)
    {
        if (detected != candidate_) {
            candidate_ = detected;
            candidateSince_ = nowSec;
        }
        if (nowSec - candidateSince_ >= hold)
            setConfirmed(detected, nowSec);
        return confirmed_;
    }

    static LightState nextState(LightState s)
    {
        switch (s) {
            case LightState::Red:    return LightState::Green;
            case LightState::Green:  return LightState::Yellow;
            case LightState::Yellow: return LightState::Red;
            default:                 return LightState::Unknown;
        }
    }

    double holdSec_, graceSec_, unknownTimeoutSec_, longHoldSec_;
    LightState confirmed_ = LightState::Unknown;
    LightState candidate_ = LightState::Unknown;
    double candidateSince_ = 0.0;
    double lastValidTime_ = 0.0;
    double stateSince_ = 0.0;
    bool stateKnownFromStart_ = false;
};

// ===================== toString =====================
const char* toString(LightState s)
{
    switch (s) {
        case LightState::Red:    return "RED";
        case LightState::Green:  return "GREEN";
        case LightState::Yellow: return "YELLOW";
        default:                 return "UNKNOWN";
    }
}

// ===================== Morphology =====================
void Morphology(Mat& mask)
{
    Mat k9 = getStructuringElement(MORPH_RECT, Size(9, 9));
    morphologyEx(mask, mask, MORPH_CLOSE, k9);
}
// void Morphology(Mat& mask)
// {
//     Mat kernel = getStructuringElement(MORPH_RECT, Size(5, 5));
//     Mat temp;
//     dilate(mask, temp, kernel);
//     erode(temp, mask, kernel);
//}
// void Morphology(Mat& mask)
// {
//     Mat k3 = getStructuringElement(MORPH_RECT, Size(3, 3));
//     Mat k9 = getStructuringElement(MORPH_RECT, Size(9, 9));

//     morphologyEx(mask, mask, MORPH_OPEN,  k3);   // 先去小噪点
//     morphologyEx(mask, mask, MORPH_CLOSE, k9);   // 再连 LED 点
// }
// ===================== main =====================
int main()
{
    // 1. 串口（必须放 main 里）
    SerialPort serial;
    if (!serial.openPort("/dev/ttyUSB0", 115200)) {
        cerr << "串口打开失败，继续只显示\n";
    }

    // 2. 视频
    VideoCapture cap("/home/liu_xian/sample/build/实践4.mp4");
    // VideoCapture cap("/home/liu_xian/sample/build/0c290a9c41216ebcc64ad99976399cd5.mp4");
    if (!cap.isOpened()) {
        cerr << "视频打开失败\n";
        return -1;
    }

    // const string url = "http://10.163.3.59:8080/video";
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
    Mat imgroi, img;
    const int TARGET_W = 640;
    double fps = 0.0;
    int64 t0 = getTickCount();
    int readFail = 0;

    
    const double t_cross = 2.0;     // 车从停车点到"完全进入隧道"的实测时间先填，之后按实车改

    SequenceValidator validator;   // 0.15 / 0.50 / 1.0 / 0.60

    while (true)
    {
        if (!cap.read(imgroi) || imgroi.empty()) {
            sendTrafficLight(serial, LightState::Unknown, false);
            if (++readFail > 30) break;   // 视频结束/相机断开
            continue;
        }
        readFail = 0;

        double s = static_cast<double>(TARGET_W) / imgroi.cols;
        resize(imgroi, img, Size(), s, s, INTER_AREA);

        // 3. ROI
        // ROI：左上到右下 3/4 区域
        Rect roi(0, 0,
                 static_cast<int>(img.cols * 3 / 4),
                 static_cast<int>(img.rows * 3 / 4));
        roi &= Rect(0, 0, img.cols, img.rows);
        Mat imgROI = img(roi).clone();
        rectangle(img, roi, Scalar(255, 0, 255), 1);   // 调试：画出 ROI

        Mat hsv;
        medianBlur(imgROI, imgROI, 3); 
        GaussianBlur(imgROI, imgROI, Size(5, 5), 0);
        cvtColor(imgROI, hsv, COLOR_BGR2HSV);

        Mat maskRed1, maskRed2, maskRed, maskGreen, maskYellow;
        inRange(hsv, Scalar(0,   130, 120), Scalar(8,   255, 255), maskRed1);
        inRange(hsv, Scalar(172, 130, 120), Scalar(180, 255, 255), maskRed2);
        bitwise_or(maskRed1, maskRed2, maskRed);
        inRange(hsv, Scalar(10, 40, 150), Scalar(40, 255, 255), maskYellow);
                                                                                                                           
        inRange(hsv, Scalar(40, 80, 80),   Scalar(95, 255, 255), maskGreen);

        Morphology(maskRed);
        Morphology(maskGreen);
        Morphology(maskYellow);

        const int minArea = 50;
        Blob redBlob    = findBlobByBBox(maskRed, minArea);
        Blob greenBlob  = findBlobByBBox(maskGreen, minArea);
        Blob yellowBlob = findBlobByBBox(maskYellow, minArea);

        Detection det = decide(redBlob, greenBlob, yellowBlob, minArea);

        // 点阵检测只在颜色路径 UNKNOWN 时跑，省算力
        DotGrid grid;
        if (det.raw == LightState::Unknown) {
            grid = detectDotGrid(imgROI);
            if (grid.found) {
                LightState gridState = classifyGridColor(hsv, grid);
                if (gridState != LightState::Unknown) {
                    det.raw = gridState;
                    det.box = grid.box;
                    det.reason = "dot-grid";
                }
            }
        }

        static int dbg = 0;
        if (++dbg % 15 == 0) {
            cout << "yellowArea=" << yellowBlob.area
                 << " grid=" << grid.found << "/" << grid.count
                 << " raw=" << static_cast<int>(det.raw)
                 << " reason=" << det.reason << endl;
        }

        double now = static_cast<double>(getTickCount()) / getTickFrequency();
        LightState state = validator.update(det.raw, now);

        bool healthy = (state != LightState::Unknown);
        sendTrafficLight(serial, state, healthy);

        // 4. 画框（ROI 坐标 + 偏移）
        const Blob* best = nullptr;
        Scalar color;
        if (state == LightState::Red)         { best = &redBlob;    color = Scalar(0, 0, 255); }
        else if (state == LightState::Green)  { best = &greenBlob;  color = Scalar(0, 255, 0); }
        else if (state == LightState::Yellow) { best = &yellowBlob; color = Scalar(0, 255, 255); }

        if (best && best->found) {
            Rect drawBox = best->box + roi.tl();
            rectangle(img, drawBox, color, 2);
            putText(img, toString(state),
                    Point(drawBox.x, max(drawBox.y - 8, 15)),
                    FONT_HERSHEY_SIMPLEX, 0.7, color, 2);
        }

        if (grid.found) {
            Rect gb = grid.box + roi.tl();
            rectangle(img, gb, Scalar(255, 255, 0), 2);
            putText(img, "dots=" + to_string(grid.count),
                    Point(gb.x, std::max(gb.y - 6, 12)),
                    FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255, 255, 0), 1);
        }

        // 5. 状态 + 时间 + FPS
        putText(img, toString(state), Point(20, 40),
                FONT_HERSHEY_SIMPLEX, 1.2, Scalar(255, 255, 255), 2);

        double age = validator.stateAge(now);
        bool known = validator.stateKnownFromStart();

        String info;
        if (state == LightState::Green) {
            info = format("GREEN  age=%.1fs  remain=%.1f~%.1fs  %s",
                        age,
                        validator.timeUntilRedMin(now),
                        validator.timeUntilRedMax(now),
                        known ? "seen-start" : "mid-state");
        }
        else if (state == LightState::Yellow) {
            info = format("YELLOW age=%.1fs  remain=%.1fs  %s",
                        age,
                        validator.timeUntilRedMin(now),
                        known ? "seen-start" : "mid-state");
        }
        else if (state == LightState::Red) {
            info = format("RED    age=%.1fs  nextGreen=%.1f~%.1fs  %s",
                        age,
                        validator.timeUntilGreenMin(now),
                        validator.timeUntilGreenMax(now),
                        known ? "seen-start" : "mid-state");
        }
        else {
            info = "UNKNOWN";
        }
        putText(img, info, Point(20, 80),
                FONT_HERSHEY_SIMPLEX, 0.55, Scalar(0, 255, 0), 2);
                int64 t = getTickCount();
                int64 dt = t - t0;
                if (dt > 0) 
                {
                    double inst = getTickFrequency() / double(dt);
                    fps = (fps == 0.0) ? inst : 0.9 * fps + 0.1 * inst;
                }
        t0 = t;
        putText(img, format("FPS %.1f", fps), Point(20, 115),
                FONT_HERSHEY_SIMPLEX, 0.6, Scalar(0, 255, 0), 2);

         imshow("img", img);
        // imshow("imgyellow",maskYellow);
        // imshow("imgGreen",maskGreen);
        //  imshow("imgRed",maskRed);

        // if (waitKey(1000 / 30) == 27) break;
        if (waitKey(1000 / cap.get(CAP_PROP_FPS)) == 27) break;
    }

    destroyAllWindows();
    return 0;
}
