#ifndef _SEAM_CARVER_H_
#define _SEAM_CARVER_H_
#include <vector>
#include "opencv2/core.hpp"

class SeamCarver {
public:
	SeamCarver();
	void SetKernelSize(int kSize) { mKernelSize = kSize; }
	void SetProtectionMask(const cv::Mat &mask) { mProtectionMask = mask; }
	void Inspection(const cv::Mat& input);
	cv::Mat GetEnergyMap();
private:
	void CalcEnergyMap();
	void FindHorizontalSeam(std::vector<int> &seam);
	void FindVerticalSeam(std::vector<int> &seam);
	void CalcDynamicProgramming(const cv::Mat &energyMap);
	void RemoveHorizontalSeam(const std::vector<int> &seam);
	void RemoveVerticalSeam(const std::vector<int> &seam);

private:
	cv::Mat mOriginImage{};
	cv::Mat mCarvedImage{};
	cv::Mat mGrayCarvedImage{};
	cv::Mat mEnergyMap{};
	cv::Mat mSobelMapX{};
	cv::Mat mSobelMapY{};
	cv::Mat mRemovalMask{};
	cv::Mat mProtectionMask{};
	int mKernelSize = 3;
};
#endif