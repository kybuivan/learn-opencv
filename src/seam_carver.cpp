#include "seam_carver.h"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

SeamCarver::SeamCarver()
{
}

void SeamCarver::Inspection(const cv::Mat &input)
{
    mOriginImage = input.clone();
    CalcEnergyMap();
}

cv::Mat SeamCarver::GetEnergyMap()
{
    return mEnergyMap;
}

void SeamCarver::CalcEnergyMap()
{
    cv::cvtColor(mOriginImage, mGrayCarvedImage, cv::COLOR_BGR2GRAY);

    cv::Sobel(mGrayCarvedImage, mSobelMapX, CV_32F, 1, 0, mKernelSize);
    cv::convertScaleAbs(mSobelMapX, mSobelMapX);

    cv::Sobel(mGrayCarvedImage, mSobelMapY, CV_32F, 0, 1, mKernelSize);
    cv::convertScaleAbs(mSobelMapY, mSobelMapY);

    cv::addWeighted(mSobelMapX, 0.5, mSobelMapY, 0.5, 0, mEnergyMap);

    cv::Mat im_color;  
    cv::applyColorMap(mEnergyMap, im_color, cv::COLORMAP_JET);

    cv::namedWindow("debug eneryMap", cv::WINDOW_FREERATIO);
    cv::imshow("debug eneryMap",im_color);

    CalcDynamicProgramming(mEnergyMap);
}

void SeamCarver::FindHorizontalSeam(std::vector<int> &seam)
{
}

void SeamCarver::FindVerticalSeam(std::vector<int> &seam)
{
}

void SeamCarver::CalcDynamicProgramming(const cv::Mat &energyMap)
{
}

void SeamCarver::RemoveHorizontalSeam(const std::vector<int> &seam)
{
}

void SeamCarver::RemoveVerticalSeam(const std::vector<int> &seam)
{
}
