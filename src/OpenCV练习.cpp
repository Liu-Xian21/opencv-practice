#include <opencv2/core.hpp>
#include <opencv2/core/hal/interface.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/matx.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp>
using namespace std;
using namespace cv;
void readImage(string fileName,Mat &imgName)
{
    string path = fileName + ".jpg";
    imgName = imread(path);
    if(imgName.empty()){cout<<"Add error"<<endl;
    return ;}

}
void readColor(Mat &imgName,int x,int y){
    Vec3b pixel = imgName.at<Vec3b>(y,x);
    uchar B = pixel[0];     uchar G = pixel[1];     uchar R = pixel[2];
    cout<<"BGR = [" <<(int)B<<","<<(int)G<<","<<(int)R<<"]"<<endl;
}
void changeGray(Mat &inputimgName,Mat &outimgName)
{
    
    if(inputimgName.empty()){cout<<"Add error"<<endl;
    return ;}
    cvtColor( inputimgName, outimgName,COLOR_BGR2GRAY); 

}
void dev_channels(Mat &inputimgName, vector<Mat>& channels){
    if(inputimgName.empty()){cout<<"Add error"<<endl;
    return ;}
    split(inputimgName,channels);
    // imshow("B",channels[0]);
    // imshow("G",channels[1]);
    // imshow("R",channels[2]);

}
void circleblack(int Oricertain,int X,int Y,int radius1,int radius2)
{
    int sideLength = Oricertain;
    Mat square(sideLength,sideLength,CV_8UC1,Scalar(0));
    int x = X,y = Y;
    vector<int> radiusList = {radius1-1,radius1,radius1+1,radius2-1,radius2,radius2+1};
    for(int radius:radiusList)
    {
        for(int angle = 0;angle<360;angle++)
        {
            double radian = (angle/360.0)*2*CV_PI;
            int pointX = x+radius*cos(radian);
            int pointY = y-radius*sin(radian);
            if(pointX >= 0 && pointX <= sideLength && pointY >= 0&& pointY<=sideLength)
            {
                square.at<uchar>(pointY,pointX) = 255;
            }
        }
    }
    imshow("circleBlack",square);
    waitKey(0);
    destroyAllWindows();
}
void find_minmaxLoc(Mat &img,Mat &imgGray)
{   double minVal,maxVal;
    Point minLoc,maxLoc;
    cvtColor(img,imgGray,COLOR_BGR2GRAY);
    minMaxLoc(imgGray,&minVal,&maxVal,&minLoc,&maxLoc);
    cout<<"Min value: "<<minVal<<" at "<<minLoc<<endl;
    cout<<"Max value: "<<maxVal<<" at "<<maxLoc<<endl;
    circle(img, maxLoc, 15, Scalar(0, 0, 255), 2);   // 红色 = 最大值
    circle(img, minLoc, 15, Scalar(255, 0, 0), 2);   // 蓝色 = 最小值
}
void find_meanStdDev(Mat &img,Mat &imgGray)
{
    Mat mean,stddev,img_x,img_y,img_xy;
    meanStdDev(imgGray, mean, stddev);
    double meanV = mean.at<double>(0, 0);
    double stdV  = stddev.at<double>(0, 0);
    cout << "Mean: " << meanV << endl;
     cout << "Standard Deviation: " << stdV << endl;      //求均值和方差
        flip(img,img_x,1);  //flipCode=1表示水平翻转，flipCode=0表示垂直翻转，
                                               //flipCode=-1表示水平和垂直翻转
    flip(img,img_y,0);
    flip(img,img_xy,-1);
    imshow("Original Image", img);
    imshow("Flipped Image (X-axis)", img_x);
    imshow("Flipped Image (Y-axis)", img_y);
    imshow("Flipped Image (XY-axis)", img_xy);
}

int main()
{
    Mat img= imread("1.jpg");
    Mat img1 = img.clone();
    Rect roi(100, 100, 200, 200); // 定义感兴趣区域
    Mat roiImg = img(roi); // 提取感兴趣区域
    imshow("Original Image", img);
    imshow("ROI Image", roiImg);

    waitKey(0);
    destroyAllWindows();
}