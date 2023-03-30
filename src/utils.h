#ifndef _UTILS_H_
#define _UTILS_H_
#include "imconfig.h"
#include "imgui.h"
#include <imgui_internal.h>
#include <glad/gl.h>
#include <format>
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "facedetection_export.h"

#define PPI 300
#define CM2INCH 1 / 2.54
//define the buffer size. Do not change the size!
#define DETECT_BUFFER_SIZE 0x20000

float cm2pixel(float d)
{
	return d * PPI * CM2INCH;
}

cv::Scalar vec2scalar(ImVec4 vec)
{
	return cv::Scalar(vec.z * 255, vec.y * 255, vec.x * 255);
}

bool RoiRefine(cv::Rect &roi, cv::Size size)
{
	roi = roi & cv::Rect(cv::Point(0, 0), size);
	return roi.area() > 0;
}

ImVec2 GetScaleImageSize(ImVec2 img_size, ImVec2 window_size)
{
	ImVec2 outSize{};
	if (img_size.x != 0 && img_size.y != 0)
	{
		double h1 = window_size.x * (img_size.y / (double)img_size.x);
		double w2 = window_size.y * (img_size.x / (double)img_size.y);
		if (h1 <= window_size.y)
		{
			outSize.x = window_size.x;
			outSize.y = static_cast<float>(h1);
		}
		else
		{
			outSize.x = static_cast<float>(w2);
			outSize.y = window_size.y;
		}
	}
	else
	{
		outSize = ImVec2(0, 0);
	}
	return outSize;
}

cv::Mat resizeKeepAspectRatio(const cv::Mat &input, const cv::Size &dstSize, const cv::Scalar &bgcolor)
{
	cv::Mat output;

	double h1 = dstSize.width * (input.rows / (double)input.cols);
	double w2 = dstSize.height * (input.cols / (double)input.rows);
	if (h1 <= dstSize.height)
	{
		cv::resize(input, output, cv::Size(dstSize.width, h1), 0, 0, cv::INTER_CUBIC);
	}
	else
	{
		cv::resize(input, output, cv::Size(w2, dstSize.height), 0, 0, cv::INTER_CUBIC);
	}

	int top = (dstSize.height - output.rows) / 2;
	int down = (dstSize.height - output.rows + 1) / 2;
	int left = (dstSize.width - output.cols) / 2;
	int right = (dstSize.width - output.cols + 1) / 2;

	cv::copyMakeBorder(output, output, top, down, left, right, cv::BORDER_CONSTANT, bgcolor);

	return std::move(output);
}

bool FaceDetection(const cv::Mat &image, cv::Mat& out, float limitConfident, std::vector<FaceRect>& faces, std::vector<std::string>& logs)
{
	out.release();
	faces = objectdetect_cnn((unsigned char*)(image.ptr(0)), image.cols, image.rows, (int)image.step);
	int num_faces =(int)faces.size();
    num_faces = MIN(num_faces, 256);
	cv::Mat result_image = image.clone();
	for (int i = 0; i < num_faces; i++)
    {
		if(faces[i].score > limitConfident)
		{
			cv::Rect faceROI = {faces[i].x, faces[i].y, faces[i].w, faces[i].h};
			//show the score of the face. Its range is [0-100]
			std::string sScore = std::format("{:.2f}", faces[i].score);
			cv::putText(result_image, sScore, cv::Point(faceROI.x, faceROI.y-3), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
			//draw face rectangle
			cv::rectangle(result_image, faceROI, cv::Scalar(0, 255, 0), 2);

			//print the result
			std::string logStr = std::format("face {}: confidence={:.2f}, [{}, {}, {}, {}]", 
					i, faces[i].score, faceROI.x, faceROI.y, faceROI.width, faceROI.height);
			logs.push_back(logStr);
		}
	}

	out = result_image;
	return true;
}
#endif