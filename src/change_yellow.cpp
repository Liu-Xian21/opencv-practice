#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
using namespace cv;
using namespace std;
// ===================== 滑动条阈值 =====================
static int hmin = 0,   hmax = 180;
static int smin = 0,   smax = 255;
static int vmin = 0,   vmax = 255;
// 鼠标点击取 HSV 用
static Mat gHsv;
static void onTrackbar(int, void*) {}
static void onMouse(int event, int x, int y, int, void*)
{
    if (event != EVENT_LBUTTONDOWN) return;
    if (gHsv.empty()) return;
    if (x < 0 || y < 0 || x >= gHsv.cols || y >= gHsv.rows) return;
    Vec3b p = gHsv.at<Vec3b>(y, x);
    cout << "(" << x << "," << y << ")  H=" << (int)p[0]
         << "  S=" << (int)p[1]
         << "  V=" << (int)p[2] << "\n";
}
int main(int argc, char** argv)
{
    // 用法：./hsv_image_tuner frame.png
    Mat img = imread("/home/liu_xian/sample/build/98b9773eb947747cbbc9f2372d12fb54.png");
    if (img.empty()) {
        cerr << "无法读取图片: " << "\n";
        return -1;
    }
    Mat hsv;
    cvtColor(img, hsv, COLOR_BGR2HSV);
    gHsv = hsv;
    namedWindow("original", WINDOW_AUTOSIZE);
    namedWindow("mask", WINDOW_AUTOSIZE);
    setMouseCallback("original", onMouse);
    createTrackbar("H min", "mask", &hmin, 180, onTrackbar);
    createTrackbar("H max", "mask", &hmax, 180, onTrackbar);
    createTrackbar("S min", "mask", &smin, 255, onTrackbar);
    createTrackbar("S max", "mask", &smax, 255, onTrackbar);
    createTrackbar("V min", "mask", &vmin, 255, onTrackbar);
    createTrackbar("V max", "mask", &vmax, 255, onTrackbar);
    Mat mask, maskBgr;
    while (true)
    {
        Scalar lower(hmin, smin, vmin);
        Scalar upper(hmax, smax, vmax);
        inRange(hsv, lower, upper, mask);
        int count = countNonZero(mask);
        double ratio = 100.0 * count / (mask.rows * mask.cols);
        Scalar meanHsv = (count > 0) ? mean(hsv, mask) : Scalar(0, 0, 0);
        cvtColor(mask, maskBgr, COLOR_GRAY2BGR);
        Mat view = img.clone();
        putText(view,
                format("H[%d,%d] S[%d,%d] V[%d,%d]",
                       hmin, hmax, smin, smax, vmin, vmax),
                Point(10, 25), FONT_HERSHEY_SIMPLEX, 0.6,
                Scalar(0, 255, 0), 2);
        putText(view,
                format("pixels=%d (%.2f%%)", count, ratio),
                Point(10, 50), FONT_HERSHEY_SIMPLEX, 0.6,
                Scalar(0, 255, 0), 2);
        putText(view,
                format("mean H=%.1f S=%.1f V=%.1f",
                       meanHsv[0], meanHsv[1], meanHsv[2]),
                Point(10, 75), FONT_HERSHEY_SIMPLEX, 0.6,
                Scalar(0, 255, 0), 2);
        imshow("original", view);
        imshow("mask", maskBgr);
        int key = waitKey(1);
        if (key == 27) break;          // ESC 退出
        if (key == 's') {              // 打印当前阈值
            cout << "lower = Scalar(" << hmin << ", " << smin << ", " << vmin << ")\n";
            cout << "upper = Scalar(" << hmax << ", " << smax << ", " << vmax << ")\n";
        }
        if (key == 'p') {              // 保存当前 mask
            imwrite("tuner_mask.png", mask);
            cout << "saved tuner_mask.png\n";
        }
    }
    return 0;
}