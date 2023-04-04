#include "utils.h"
#include "seam_carver.h"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

SeamCarver::SeamCarver()
{
}

void SeamCarver::Inspection(const cv::Mat &input) noexcept
{
    if (!input.empty())
    {
        //mOriginImage = input.clone();
        mCarvedImage= CalcCarvedImage(input, mSize);
        while(CheckFinishCarved())
        {
            std::vector<int>* seam = new std::vector<int>();
            if (mDirection == SeamDirection::VERTICAL)
            {
                FindVerticalSeam(seam);
            }
            else
            {
                FindHorizontalSeam(seam);
            }
        }
        mCarvedImage.convertTo(mCarvedImage, CV_8UC3);
        cv::namedWindow("debug mCarvedImage", cv::WINDOW_FREERATIO); cv::imshow("debug mCarvedImage", mCarvedImage);
    }
}

void SeamCarver::CalcEnergyMap() noexcept
{
    cv::Mat grayImage, sobelMapX, sobelMapY;
    cv::cvtColor(mCarvedImage, grayImage, cv::COLOR_RGB2GRAY);

    cv::Sobel(grayImage, sobelMapX, CV_32F, 1, 0, mKernelSize);
    cv::convertScaleAbs(sobelMapX, sobelMapX);

    cv::Sobel(grayImage, sobelMapY, CV_32F, 0, 1, mKernelSize);
    cv::convertScaleAbs(sobelMapY, sobelMapY);
    cv::Mat energyImage;
    cv::addWeighted(sobelMapX, 0.5, sobelMapY, 0.5, 0, energyImage);
    cv::Mat im_color;  
    cv::applyColorMap(energyImage, im_color, cv::COLORMAP_JET);

    cv::namedWindow("debug eneryMap", cv::WINDOW_FREERATIO);
    cv::imshow("debug eneryMap",im_color);
    if (!mProtectionMask.empty())
    {
        cv::add(energyImage, mProtectionMask, mEnergyMap);
    }
    mEnergyMap.convertTo(mEnergyMap, CV_32F);
    grayImage.release();
    sobelMapX.release();
    sobelMapY.release();
}

cv::Mat SeamCarver::CalcCarvedImage(const cv::Mat &image, cv::Size size) noexcept
{
    cv::Mat output;
    
    if(!image.empty() && !size.empty())
    {
        double h1 = size.width * (image.rows / (double)image.cols);
        double w2 = size.height * (image.cols / (double)image.rows);
        if (h1 > size.height)
        {
            cv::resize(image, output, cv::Size(size.width, h1), 0, 0, cv::INTER_CUBIC);
            if(!mProtectionMask.empty()) cv::resize(mProtectionMask, mProtectionMask, cv::Size(size.width, h1), 0, 0, cv::INTER_CUBIC);
        }
        else
        {
            cv::resize(image, output, cv::Size(w2, size.height), 0, 0, cv::INTER_CUBIC);
            if(!mProtectionMask.empty()) cv::resize(mProtectionMask, mProtectionMask, cv::Size(w2, size.height), 0, 0, cv::INTER_CUBIC);
        }
    }
    else
    {
        output = {};
    }

    return output;
}

void SeamCarver::FindHorizontalSeam(std::vector<int> *seam) noexcept
{
    CalcDynamicProgramming(mEnergyMap, seam, SeamDirection::HORIZONTAL, SeamEnergyType::MIN_ENERGY);
    RemoveSeam(mCarvedImage, *seam, SeamDirection::HORIZONTAL);
    RemoveSeam(mProtectionMask, *seam, SeamDirection::HORIZONTAL);
}

void SeamCarver::FindVerticalSeam(std::vector<int> *seam) noexcept
{
    CalcDynamicProgramming(mEnergyMap, seam, SeamDirection::VERTICAL, SeamEnergyType::MIN_ENERGY);
    RemoveSeam(mCarvedImage, *seam, SeamDirection::VERTICAL);
    RemoveSeam(mProtectionMask, *seam, SeamDirection::VERTICAL);
}

template <class T>
cv::Mat getConvertMatFromVector(std::vector<std::vector<T>> input_vec)
{
    int rows = input_vec.size();
    int cols = input_vec[0].size();

    // Create an empty cv::Mat with the same dimensions as the input vector
    cv::Mat output_mat(rows, cols, CV_32FC1);

    // Copy the values from the input vector to the output matrix
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            output_mat.at<float>(i, j) = input_vec[i][j];
        }
    }
    return output_mat;
}

void SeamCarver::CalcDynamicProgramming(const cv::Mat& energy_map, std::vector<int>* seam, SeamDirection seam_direction, SeamEnergyType energy_type) noexcept
{
    if (!energy_map.empty() && !seam->empty())
    {
        assert(seam);
        const int rows = energy_map.rows;
        const int cols = energy_map.cols;
        std::vector<std::vector<float>> dp;
        std::vector<std::vector<int>> dp_path;

        if (seam_direction == SeamDirection::HORIZONTAL) {
            dp = std::vector<std::vector<float>>(rows, std::vector<float>(cols, 0.f));
            dp_path = std::vector<std::vector<int>>(rows, std::vector<int>(cols, 0));
        }
        else
        {
            dp = std::vector<std::vector<float>>(cols, std::vector<float>(rows, 0.f));
            dp_path = std::vector<std::vector<int>>(cols, std::vector<int>(rows, 0));
        }

        // Initialize first row or column of dp matrix
        for (int i = 0; i < dp.size(); ++i) {
            int x = seam_direction == SeamDirection::HORIZONTAL ? 0 : i;
            int y = seam_direction == SeamDirection::VERTICAL ? 0 : i;
            dp[i][0] = energy_map.at<float>(cv::Point(x, y));
            dp_path[i][0] = i;
        }

        // Compute dynamic programming matrix
        int dpSize = dp.size();
        int dp0Size = dp[0].size();
        for (int j = 1; j < dp0Size; ++j) {
            for (int i = 0; i < dpSize; ++i) {
                int x = seam_direction == SeamDirection::HORIZONTAL ? j : i;
                int y = seam_direction == SeamDirection::VERTICAL ? j : i;

                float energy_min = energy_type == SeamEnergyType::MIN_ENERGY ? std::numeric_limits<float>::max() : -std::numeric_limits<float>::max();
                int parent_idx = i;
                for (int k = -1; k <= 1; ++k) {
                    if ((i + k >= 0) && (i + k < dpSize)) {
                        float energy_prev = dp[i+k][j-1];
                        if (energy_type == SeamEnergyType::MAX_ENERGY) energy_prev *= -1;
                        if (energy_prev < energy_min) {
                            energy_min = energy_prev;
                            parent_idx = i + k;
                        }
                    }
                }
                dp[i][j] = energy_map.at<float>(cv::Point(x, y)) + energy_min;
                dp_path[i][j] = parent_idx;
            }
        }

        cv::Mat dpMat = getConvertMatFromVector(dp);
        cv::Mat dp_pathMat = getConvertMatFromVector(dp_path);
        // Find seam with minimum or maximum energy
        seam->resize(dp0Size);
        float energy_seam = energy_type == SeamEnergyType::MIN_ENERGY ? std::numeric_limits<float>::max() : -std::numeric_limits<float>::max();
        int seam_idx = 0;
        for (int i = 0; i < dpSize; ++i) {

            float energy_i = dp[i][dp0Size - 1];
            if (energy_type == SeamEnergyType::MAX_ENERGY) energy_i *= -1;
            if (energy_i < energy_seam) {
                energy_seam = energy_i;
                seam_idx = i;
            }
        }

        // Backtrack to find seam indices
        for (int j = dp0Size - 1; j >= 0; --j) {
            (*seam)[j] = seam_direction == SeamDirection::VERTICAL ? seam_idx : dpSize - 1 - seam_idx;
            seam_idx = dp_path[(*seam)[j]][j];
        }

        // cv::Mat debug;
        // cv::cvtColor(energy_map, debug, cv::COLOR_GRAY2RGB);
        // if (seam_direction == SeamDirection::VERTICAL)
        // {
        //     for (int i = 0; i < debug.rows; i++)
        //     {
        //         debug.at<cv::Vec3f>(i, (*seam)[i]) = cv::Vec3f(0, 0, 255);
        //     }
        // }
        // else
        // {
        //     for (int i = 0; i < debug.cols; i++)
        //     {
        //         debug.at<cv::Vec3f>((*seam)[i], i) = cv::Vec3f(0, 0, 255);
        //     }
        // }
        // cv::imshow("debug", debug);
        // cv::waitKey(0);
    }
}

void SeamCarver::RemoveSeam(cv::Mat& image, const std::vector<int> &seam, SeamDirection direction) noexcept
{
    if (!image.empty() && !seam.empty())
    {
        int rows = image.rows;
        int cols = image.cols;
    
        if (direction == SeamDirection::VERTICAL) {
            // Remove pixels from left to right
            for (int i = 0; i < rows; i++) {
                int seam_idx = seam[i];
                cv::Mat row = image.row(i);
                cv::Rect roi(seam_idx, 0, cols - seam_idx - 1, 1);
                if (RoiRefine(roi, row.size()) && seam_idx != 0)
                {
                    cv::Mat submat = row(roi);
                    submat.copyTo(row(cv::Rect(seam_idx - 1, 0, cols - seam_idx - 1, 1)));
                }
            }
            // Reduce image width by 1
            image = image(cv::Rect(0, 0, cols-1, rows));
        }
        else {
            // Remove pixels from top to bottom
            for (int j = 0; j < cols; j++) {
                int seam_idx = seam[j];
                cv::Mat col = image.col(j);
                cv::Rect roi(0, seam_idx, 1, rows - seam_idx - 1);
                if (RoiRefine(roi, col.size()) && seam_idx != 0)
                {
                    cv::Mat submat = col(roi);
                    submat.copyTo(col(cv::Rect(0, seam_idx - 1, 1, rows - seam_idx - 1)));
                }
            }
            // Reduce image height by 1
            image = image(cv::Rect(0, 0, cols, rows-1));
        }
    }
}

bool SeamCarver::CheckFinishCarved() noexcept
{
    bool finish = false;
    if(!mCarvedImage.empty())
    {
        CalcEnergyMap();

        int width = mSize.width - mCarvedImage.cols;
        int height = mSize.height - mCarvedImage.rows;

        if(width == height && width == 0)
        {
            finish = true;
        }
        else
        {
            mDirection = (width < height) ? SeamDirection::VERTICAL : SeamDirection::HORIZONTAL;
        }
    }
    return !finish;
}
