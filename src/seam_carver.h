#ifndef _SEAM_CARVER_H_
#define _SEAM_CARVER_H_
#include <vector>
#include "opencv2/core.hpp"

enum class SeamDirection {
  VERTICAL,
  HORIZONTAL
};

enum class SeamEnergyType {
  MIN_ENERGY,
  MAX_ENERGY
};

class SeamCarver {
public:
	explicit SeamCarver();
	void Inspection(const cv::Mat& input) noexcept;
	void SetSize(cv::Size size) noexcept { mSize = size; }
	void SetKernelSize(int kSize) noexcept { mKernelSize = kSize; }
	void SetProtectionMask(const cv::Mat &mask) noexcept { mProtectionMask = mask; }
	void SetRemovalMask(const cv::Mat &mask) noexcept { mRemovalMask = mask; }
	cv::Mat GetCarvedImage() noexcept {return std::move(mCarvedImage); }
private:
	void CalcEnergyMap() noexcept;
	cv::Mat CalcCarvedImage(const cv::Mat &image, cv::Size size) noexcept;
	void FindHorizontalSeam(std::vector<int> *seam) noexcept;
	void FindVerticalSeam(std::vector<int> *seam) noexcept;
	void CalcDynamicProgramming(const cv::Mat& energy_map, std::vector<int>* seam, SeamDirection direction, SeamEnergyType energy_type) noexcept;
	void RemoveSeam(cv::Mat& image, const std::vector<int> &seam, SeamDirection direction) noexcept;
	bool CheckFinishCarved() noexcept;
private:
	int mKernelSize = 3;
	cv::Size mSize{};
	cv::Mat mOriginImage{};
	cv::Mat mCarvedImage{};
	cv::Mat mEnergyMap{};
	cv::Mat mRemovalMask{};
	cv::Mat mProtectionMask{};
	SeamDirection mDirection;
	SeamEnergyType mEnergyType = SeamEnergyType::MIN_ENERGY;
};
#endif