#include <iostream>
#include <string>
#include <vector>
#include "window.h"
#include "nfd.h"
#include "utils.h"
#include "ref.h"
#include "opencv2/highgui.hpp"
#include <iostream>
#include <filesystem>
#include "seam_carver.h"
using namespace std::filesystem;
const char* algorithmItems[] = { "resize", "seam", "crop" };
const int algorithmSize = 3;

struct Texture2D
{
	int width = 0;
	int height = 0;
	uint32_t id = 0;
};

class ImageInfo
{
public:
	ImageInfo(std::string _path)
		: mPath(_path)
	{
		cv::Mat img = cv::imread(mPath);
		mWidth = img.cols;
		mHeight = img.rows;
		//cv::resize(img, img, cv::Size(100, 150));
		cv::cvtColor(img, img, cv::COLOR_RGB2BGR);
		mTexture = CreateTexture(img);
		img.release();
	}

	void Release()
	{
		if (mTexture.id)
		{
			glDeleteTextures(1, &mTexture.id);
			mTexture.width = 0;
			mTexture.height = 0;
		}
		
		if(!mFaces.empty()) mFaces.clear();
	}

	static Texture2D CreateTexture(cv::Mat img, int format = GL_RGB)
	{
		Texture2D text;
		// Create a OpenGL texture identifier
		glGenTextures(1, &text.id);
		glBindTexture(GL_TEXTURE_2D, text.id);
		text.width = img.cols;
		text.height = img.rows;

		// Setup filtering parameters for display
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same
		glTexImage2D(GL_TEXTURE_2D, 0, format, text.width, text.height, 0, format, GL_UNSIGNED_BYTE, img.data);
		return text;
	}

public:
	std::string GetPath() { return mPath; }
	const Texture2D &GetTexture() { return mTexture; }
	int GetWidth() { return mWidth; }
	int GetHeight() { return mHeight; }
	std::string GetName()
	{
		std::string name = mPath;
		return name.substr(name.find_last_of("\\/") + 1);
	}
	const std::vector<FaceRect>& GetFaces() { return mFaces; }

	cv::Mat GetMat() {return cv::imread(mPath);}
	bool CheckEnableFindFaceAuto() { return mEnableFindFaceAuto; }
	void FaceDetection(cv::Size limitSize, float limitConfident, std::vector<std::string>& debugLogs)
	{
		cv::Mat faceDetectImg;
		ImVec2 newSize = GetScaleImageSize(ImVec2(mWidth, mHeight), ImVec2(limitSize.width, limitSize.height));
		cv::resize(GetMat(), faceDetectImg, cv::Size(newSize.x, newSize.y), cv::INTER_CUBIC);

		std::vector<FaceRect> faces = objectdetect_cnn((unsigned char*)(faceDetectImg.ptr(0)), faceDetectImg.cols, faceDetectImg.rows, (int)faceDetectImg.step);
		int num_faces =(int)faces.size();
		num_faces = MIN(num_faces, 256);
		cv::Mat faceDetectImgResult = faceDetectImg.clone();
		for (int i = 0; i < num_faces; i++)
		{
			cv::Rect faceROI = {faces[i].x, faces[i].y, faces[i].w, faces[i].h};
			if(faces[i].score > limitConfident && faceROI.area() < faceDetectImg.size().area())
			{
				//show the score of the face. Its range is [0-100]
				std::string sScore = std::format("{:.2f}", faces[i].score);
				cv::putText(faceDetectImgResult, sScore, cv::Point(faceROI.x, faceROI.y-3), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
				//draw face rectangle
				cv::rectangle(faceDetectImgResult, faceROI, cv::Scalar(0, 255, 0), 2);
				mFaces.emplace_back(faces[i]);
			}
			//print the result
			std::string logStr = std::format("face {}: confidence={:.2f}, [{}, {}, {}, {}]", 
					i, faces[i].score, faces[i].x, faces[i].y, faces[i].w, faces[i].h);
			debugLogs.push_back(logStr);
		}

		for (int i = 0; i < mFaces.size(); i++)
		{
			cv::Rect oriFace = GetScaleRect(cv::Rect(mFaces[i].x, mFaces[i].y, mFaces[i].w, mFaces[i].h), cv::Size(mWidth, mHeight), faceDetectImg.size());
			mFaces[i].x = oriFace.x;
			mFaces[i].y = oriFace.y;
			mFaces[i].w = oriFace.width;
			mFaces[i].h = oriFace.height;
		}

		faceDetectImgResult.release();
		faceDetectImg.release();
		mEnableFindFaceAuto = false;
	}
private:
	std::string mPath = "";
	bool mEnableFindFaceAuto = true;
	int mWidth;
	int mHeight;
	Texture2D mTexture;
	std::vector<FaceRect> mFaces;
};

class Application
{
public:
	Application() : mTexture{}
	{
		mTexture = ImageInfo::CreateTexture(cv::Mat::zeros(cv::Size(cm2pixel(mWidth), cm2pixel(mHeight)), CV_8UC3));
	}

	void MenuBarFunction()
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New File...", "Ctrl+N"))
				{
					Reset();
				}
				if (ImGui::MenuItem("Open File...", "Ctrl+O"))
				{
					nfdpathset_t outPaths;
					// nfdchar_t *outPath = NULL;
					nfdresult_t result = NFD_OpenDialogMultiple("png,jpg", NULL, &outPaths);
					if (result == NFD_OKAY)
					{
						puts("Success!");
						mImageList.clear();
						mCurrentMat.release();
						mPreviousIdex = mCurrentIdex = 0;
						for (size_t i = 0; i < NFD_PathSet_GetCount(&outPaths); ++i)
						{
							nfdchar_t *outPath = NFD_PathSet_GetPath(&outPaths, i);
							mImageList.push_back(CreateRef<ImageInfo>(outPath));
						}
					}
					else if (result == NFD_CANCEL)
					{
						puts("User pressed cancel.");
					}
					else
					{
						// log("Error: %s\n", NFD_GetError() );
					}
				}

				if (ImGui::MenuItem("Open Folder...", "Ctrl+Shift+O"))
				{
					nfdchar_t *outPath = NULL;
					nfdresult_t result = NFD_PickFolder( NULL, &outPath );
					if ( result == NFD_OKAY )
					{
						puts("Success!");
						puts(outPath);
						if(!mImageList.empty()) for(auto& img : mImageList) img->Release();
						mImageList.clear();
						mPreviousIdex = mCurrentIdex = 0;
						mCurrentMat.release();
						std::vector<std::string> extensions = { ".jpg", ".JPG", ".png", ".PNG" };
						for (const auto& entry : std::filesystem::directory_iterator(outPath)) {
							if (entry.is_regular_file()) {
								std::string file_path = entry.path().string();
								for (const auto& ext : extensions) {
									if (file_path.size() >= ext.size() && file_path.compare(file_path.size() - ext.size(), ext.size(), ext) == 0) {
										mImageList.push_back(CreateRef<ImageInfo>(file_path));
										break;
									}
								}
							}
						}
						free(outPath);
					}
					else if ( result == NFD_CANCEL )
					{
						puts("User pressed cancel.");
					}
					else 
					{
						printf("Error: %s\n", NFD_GetError() );
					}
				}

				if (ImGui::MenuItem("Save As...", "Ctrl+S"))
				{
					nfdchar_t *savePath = NULL;
					nfdresult_t result = NFD_SaveDialog("png;jpg;tif", mCurrentImagePath.c_str(), &savePath);
					if (result == NFD_OKAY)
					{
						puts("Success!");
						puts(savePath);
						SaveFile(savePath);
						free(savePath);
					}
					else if (result == NFD_CANCEL)
					{
						puts("User pressed cancel.");
					}
					else
					{
						printf("Error: %s\n", NFD_GetError());
					}
				}

				if (ImGui::MenuItem("Save All", "Ctrl+Shift+S"))
				{
					nfdchar_t *outPath = NULL;
					nfdresult_t result = NFD_PickFolder( NULL, &outPath );
					if ( result == NFD_OKAY )
					{
						puts("Success!");
						puts(outPath);
						SaveFolder(outPath);
						free(outPath);
					}
					else if ( result == NFD_CANCEL )
					{
						puts("User pressed cancel.");
					}
					else 
					{
						printf("Error: %s\n", NFD_GetError() );
					}
				}

				if (ImGui::MenuItem("Exit"))
				{
					exit_app = true;
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Edit"))
			{
				if (ImGui::MenuItem("Undo", "Ctrl+Z"))
				{
				}
				if (ImGui::MenuItem("Redo", "Ctrl+Y"))
				{
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Help"))
			{
				if (ImGui::MenuItem("About"))
				{
					ImGui::OpenPopup("AboutMe");

					if (ImGui::BeginPopupModal("AboutMe"))
					{
						ImGui::Text("This is about.");
						ImGui::EndPopup();
					}
				}

				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}
	}

	void ViewFunction()
	{
		ImGuiIO& io = ImGui::GetIO();
		ImVec2 screen_size = ImVec2(io.DisplaySize.x, io.DisplaySize.y);
		{
			// Set the next window position to the left side of the screen
			//ImGui::SetNextWindowPos(ImVec2(screen_size.x / 4, 0));
			ImGui::SetNextWindowSize(ImVec2(screen_size.x / 4, screen_size.y / 2));
			//ImGui::SetNextWindowPos(ImVec2(io.DisplacP, 0));
			ImGui::Begin("Setting", nullptr, ImGuiWindowFlags_NoCollapse); // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
			ImGui::Text("texture pos = %d", mTexture.id);
			cv::Size currentSize = {0, 0};

			if (!mImageList.empty())
			{
				currentSize = cv::Size(mImageList[mCurrentIdex]->GetWidth(),mImageList[mCurrentIdex]->GetHeight());
			}

			ImGui::Text("size = %d x %d", currentSize.width, currentSize.height);

			ImGui::PushMultiItemsWidths(2, ImGui::CalcItemWidth());
			ImGui::PushID("width");
			ImGuiContext &g = *GImGui;
			ImGui::TextUnformatted("width:");
			ImGui::SameLine(0, g.Style.ItemInnerSpacing.x);
			ImGui::DragFloat("##hidelabel", &mWidth, 0.1f, 1.0f, 100.0f);
			ImGui::PopID();
			ImGui::PushID("height");
			ImGui::SameLine(0, g.Style.ItemInnerSpacing.x);
			ImGui::TextUnformatted("height:");
			ImGui::SameLine(0, g.Style.ItemInnerSpacing.x);
			ImGui::DragFloat("##hidelabel", &mHeight, 0.1f, 1.0f, 100.0f);
			ImGui::PopItemWidth();
			ImGui::PopID();
			ImGui::PushID("Combo");
			ImGui::TextUnformatted("resize algorithm:");
			ImGui::SameLine(0, g.Style.ItemInnerSpacing.x);
			ImGui::Combo("##hidelabel", &mAlgorithmItem, algorithmItems, algorithmSize);
			ImGui::PopID();
			

			//if(mEnableFaceDetection)
			{
				ImGui::Separator();
				ImGui::PushID("Face");
				if(ImGui::Button("face detection"))
				{
					if(!mImageList.empty()) mImageList[mCurrentIdex]->FaceDetection(resizeFaceDetection, mLimitConfident, debugLog);
				}
				ImGui::PopID();
				ImGui::PushMultiItemsWidths(2, ImGui::CalcItemWidth());
				ImGui::PushID("resize.w");
				ImGui::TextUnformatted("limit resize(w,h):");
				ImGui::SameLine(0, g.Style.ItemInnerSpacing.x);
				ImGui::DragInt("##hidelabel", &resizeFaceDetection.width, 1, 100, 1000);
				ImGui::PopID();
				ImGui::PushID("resize.h");
				ImGui::SameLine(0, g.Style.ItemInnerSpacing.x);
				ImGui::TextUnformatted("x");
				ImGui::SameLine(0, g.Style.ItemInnerSpacing.x);
				ImGui::DragInt("##hidelabel", &resizeFaceDetection.height, 1, 100, 1000);
				ImGui::PopItemWidth();
				ImGui::PopID();
				ImGui::PushID("limitconfident");
				ImGui::TextUnformatted("limit confident:");
				ImGui::SameLine(0, g.Style.ItemInnerSpacing.x);
				ImGui::DragFloat("##hidelabel", &mLimitConfident, 0.01f, 0.0f, 1.0f);
				ImGui::PopItemWidth();
				ImGui::PopID();
			}
			ImGui::End();
		}

		{
			ImGui::SetNextWindowSize(ImVec2(screen_size.x * 2 / 3.0f, screen_size.y * 3 / 4.0f));
			ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(20, 20, 20, 255));
			ImGui::Begin("Resize", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
			// image_size = ImVec2(text.mWidth, text.mHeight);
			int border = 20;
			ImVec2 currentWindowSize = ImGui::GetWindowSize();
			ImVec2 windowSize(ImGui::GetWindowSize().x - border * 2, ImGui::GetWindowSize().y - border * 2);
			ImVec2 newSize = GetScaleImageSize(ImVec2(static_cast<int>(cm2pixel(mWidth)), static_cast<int>(mTexture.height)), windowSize);
			ImGui::SetCursorPos(ImVec2((ImGui::GetWindowSize().x - newSize.x) * 0.5f, (ImGui::GetWindowSize().y - newSize.y) * 0.5f + border / 2.0f));
			ImGui::Image((void *)(intptr_t)mTexture.id, newSize);
			ImGui::End();
			ImGui::PopStyleColor();
		}

		{
			ImGui::SetNextWindowSize(ImVec2(screen_size.x / 4.0f, screen_size.y / 2.0f));
			ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(20, 20, 20, 255));
			ImGui::Begin("Image List", NULL, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
			int border = 5;
			ImVec2 currentWindowSize = ImGui::GetWindowSize();
			int currentCursorPosX = border / 2.0f;
			int currentRow = 0;
			int currentColumn = 0;
			int numColumns = 5;
			for (int i = 0; i < mImageList.size(); i++)
			{
				auto tex = mImageList[i]->GetTexture();
				ImVec2 windowSize(ImGui::GetWindowSize().x - border * numColumns, ImGui::GetWindowSize().y - border * 2);
				ImVec2 image_size = ImVec2(static_cast<int>(windowSize.x / (float)numColumns), static_cast<int>(windowSize.x / (float)numColumns)); //GetScaleImageSize(ImVec2(static_cast<float>(tex.width), static_cast<float>(tex.height)), windowSize);
				ImVec2 image_pos = ImVec2(currentCursorPosX, currentRow * (image_size.y + border * 2) + 20);
				ImGui::SetCursorPos(image_pos);
				ImGui::Image((void *)(intptr_t)tex.id, image_size);

				// Check if the imgui::image was double-clicked
				if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && i != mCurrentIdex)
				{
					mPreviousIdex = mCurrentIdex;
					mCurrentIdex = i;
					mCurrentMat.release();
				}

				if(i == mCurrentIdex)
				{
					// Get the position and size of the imgui::image
					// Draw a border around the imgui::image
					//ImDrawList* draw_list = ImGui::GetWindowDrawList();
					//ImVec2 border_min = ImVec2(image_pos.x - 1, image_pos.y - 1);
					//ImVec2 border_max = ImVec2(image_pos.x + image_size.x + 1, image_pos.y + image_size.y + 1);
					//draw_list->AddRect(border_min, border_max, IM_COL32(0, 255, 255, 255));
				}

				currentColumn++;
				if (currentColumn == numColumns) {
					currentRow++;
					currentColumn = 0;
				}
				currentCursorPosX = currentColumn * (image_size.x + border) + border / 2.0f;
				//currentCursorPosX += image_size.x + border / 2;
			}
			// Get the maximum horizontal scrolling position
			//float max_scroll_x = ImGui::GetScrollMaxX();

			// Set the horizontal scrolling position
			//ImGui::SetScrollX(max_scroll_x);
			ImGui::End();
			ImGui::PopStyleColor();
		}

		{
			//ImGui::SetNextWindowSize(ImVec2(screen_size.x * 2 / 3.0f, screen_size.y * 3 / 4.0f));
			ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(20, 20, 20, 255));
			ImGui::Begin("Viewer", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
			ImGui::PushID("Viewer");
			static ImVec2 prev_window_pos = ImVec2(0, 0);
			ImVec2 window_pos = ImGui::GetWindowPos();
			ImVec2 offset = ImVec2(window_pos.x - prev_window_pos.x, window_pos.y - prev_window_pos.y);
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			// image_size = ImVec2(text.mWidth, text.mHeight);
			int border = 20;
			ImVec2 currentWindowSize = ImGui::GetWindowSize();
			ImVec2 windowSize(ImGui::GetWindowSize().x - border * 2.0f, ImGui::GetWindowSize().y - border * 2.0f);
			if (!mImageList.empty())
			{
				ImVec2 image_size = GetScaleImageSize(ImVec2(mImageList[mCurrentIdex]->GetWidth(), mImageList[mCurrentIdex]->GetHeight()), windowSize);
				ImVec2 image_pos = ImVec2(static_cast<int>((ImGui::GetWindowSize().x - image_size.x) * 0.5f), static_cast<int>((ImGui::GetWindowSize().y - image_size.y) * 0.5f + border / 2.0f));
				ImGui::SetCursorPos(image_pos);
				ImGui::Image((void *)(intptr_t)mImageList[mCurrentIdex]->GetTexture().id, image_size);

				auto faces = mImageList[mCurrentIdex]->GetFaces();
				if(faces.empty() && mImageList[mCurrentIdex]->CheckEnableFindFaceAuto())
				{
					mImageList[mCurrentIdex]->FaceDetection(resizeFaceDetection, mLimitConfident, debugLog);
				}
				else
				{
					image_pos.x += window_pos.x;
					image_pos.y += window_pos.y;
					for (int i = 0; i < faces.size(); i++)
					{
						cv::Rect faceROI = GetScaleRect(cv::Rect(faces[i].x, faces[i].y, faces[i].w, faces[i].h), cv::Size(image_size.x, image_size.y), mCurrentMat.size());
						//show the score of the face. Its range is [0-100]
						std::string sScore = std::format("{:.2f}", faces[i].score);
						//ImGui::SetCursorScreenPos(image_pos);

						ImVec2 border_min = ImVec2(image_pos.x + faceROI.x, image_pos.y + faceROI.y);
						ImVec2 border_max = ImVec2(border_min.x + faceROI.width, border_min.y + faceROI.height);
						//ImGui::SetCursorPos(border_min);
						draw_list->AddRect(border_min, border_max, IM_COL32(0, 255, 0, 255));
						//ImGui::SetCursorPos(border_min);
						draw_list->AddText(ImVec2(border_min.x, border_min.y - 15), IM_COL32(0, 255, 0, 255), sScore.c_str());
						
						//print the result
						//debugLog.push_back(std::format("face {}: confidence={:.2f}, [{}, {}, {}, {}]", i, faces[i].score, faces[i].x, faces[i].y, faces[i].w, faces[i].h));
						//debugLog.push_back(std::format(" image pos [{}, {}]", image_pos.x, image_pos.y));
						//debugLog.push_back(std::format(" new face ROI [{}, {}, {}, {}]", faceROI.x, faceROI.y, faceROI.width, faceROI.height));
						//debugLog.push_back(std::format(" border_min pos [{}, {}]", border_min.x, border_min.y));
						//debugLog.push_back(std::format(" border_max pos [{}, {}]", border_max.x, border_max.y));
						// cv::putText(tmpImg, sScore, cv::Point(faceROI.x, faceROI.y-13), cv::FONT_HERSHEY_SIMPLEX, 2.5, cv::Scalar(0, 255, 0), 10);
						// draw face rectangle
						// cv::rectangle(tmpImg, faceROI, cv::Scalar(0, 255, 0), 10);
					}
				}
			}
			ImGui::PopID();
			ImGui::End();
			ImGui::PopStyleColor();
			prev_window_pos = window_pos;
		}

		{
			ImGui::Begin("Debug");
			if (ImGui::Button("Clear"))
				debugLog.clear();
			for (auto &str : debugLog)
				ImGui::TextWrapped(str.c_str());
			ImGui::End();
		}
	}

	void SaveFile(std::string path)
	{
		// Bind the texture
		glBindTexture(GL_TEXTURE_2D, mTexture.id);

		// Allocate buffer to store pixel data
		std::vector<unsigned char> buffer(mTexture.width * mTexture.height * 3);

		// Retrieve the pixel data from the texture
		glGetTexImage(GL_TEXTURE_2D, 0, GL_BGR, GL_UNSIGNED_BYTE, buffer.data());

		// Create cv::Mat object with texture data
		cv::Mat img(mTexture.height, mTexture.width, CV_8UC3, buffer.data());

		// convert image data to BGR format
		// cv::Mat bgrImage;
		// cv::cvtColor(img, bgrImage, cv::COLOR_RGB2RGB);

		// write image to file using imwrite
		cv::imwrite(path, img);
	}

	void SaveFolder(std::string folderPath)
	{
	}

	void Inspection()
	{
		cv::Scalar bgColor = vec2scalar(mBgColor);
		cv::Size size = cv::Size(cm2pixel(mWidth), cm2pixel(mHeight));
		// crash when input width, height
		if (!size.empty())
		{
			cv::Mat resizeImg = cv::Mat(size, CV_8UC3, bgColor);
			if (!mImageList.empty())
			{
				// if(mText.cols > mText.rows)
				// {
				// 	cv::rotate(mText, mText, cv::ROTATE_90_CLOCKWISE);
				// 	borderRect = cv::Rect(borderOfsetPixel*2, borderOfsetPixel, size.width - borderOfsetPixel*3, size.height - borderOfsetPixel*2);
				// }
				if(mPreviousIdex != mCurrentIdex || mCurrentMat.empty()) mCurrentMat = mImageList[mCurrentIdex]->GetMat();


				cv::Mat tmpImg = mCurrentMat.clone();

				cv::Mat protectMat;
				auto faces = mImageList[mCurrentIdex]->GetFaces();
				if(!faces.empty())
				{
					protectMat = cv::Mat::zeros(mCurrentMat.size(), mCurrentMat.type());
					for (int i = 0; i < faces.size(); i++)
					{
						cv::Rect faceROI = {faces[i].x, faces[i].y, faces[i].w, faces[i].h};
						//show the score of the face. Its range is [0-100]
						//std::string sScore = std::format("{:.2f}", faces[i].score);
						//cv::putText(tmpImg, sScore, cv::Point(faceROI.x, faceROI.y-13), cv::FONT_HERSHEY_SIMPLEX, 2.5, cv::Scalar(0, 255, 0), 10);
						//draw face rectangle
						//cv::rectangle(tmpImg, faceROI, cv::Scalar(0, 255, 0), 10);
						cv::rectangle(protectMat, faceROI, cv::Scalar(255, 255, 255), -1);
					}
				}
				
				if(mAlgorithmItem == 0)
				{
					resizeImg = resizeKeepAspectRatio(tmpImg, size, bgColor, true);

					int vertical = (size.height - resizeImg.rows) / 2;
					int horizontal = (size.width - resizeImg.cols) / 2;
					SeamCarver seamCarver;
					seamCarver.SetKernelSize(3);
					seamCarver.SetProtectionMask(protectMat);
					seamCarver.Inspection(resizeImg);
				}
				else if(mAlgorithmItem == 1)
				{
				}
				tmpImg.release();
			}
			// update OpenGL texture if size has changed
			if (size.width != mTexture.width || size.height != mTexture.height)
			{
				ImageRelease(mTexture);
				mTexture = ImageInfo::CreateTexture(cv::Mat::zeros(size, CV_8UC3));
				mTexture.width = size.width;
				mTexture.height = size.height;
				// glTexImage2D(GL_TEXTURE_2D, 0, GL_BGR, mTexture.width, mTexture.height, 0, GL_BGR, GL_UNSIGNED_BYTE, NULL);
			}
			glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)mTexture.id);

			// set alignment explicitly to 1
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, resizeImg.cols, resizeImg.rows, GL_BGR, GL_UNSIGNED_BYTE, resizeImg.data);
			resizeImg.release();
		}
	}

	void ImageRelease(Texture2D &text)
	{
		if (text.id)
		{
			glDeleteTextures(1, &text.id);
			text.width = 0;
			text.height = 0;
		}
	}

	void Reset()
	{
		for (auto &image : mImageList)
			image->Release();
		mImageList.clear();
		mCurrentIdex = mPreviousIdex = 0;
		mCurrentMat.release();
		mWidth = 6;
		mHeight = 9;
		mBgColor = {1.0f, 1.0f, 1.0f, 1.0f};
		ImageRelease(mTexture);
		mTexture = ImageInfo::CreateTexture(cv::Mat::zeros(cv::Size(cm2pixel(mWidth), cm2pixel(mHeight)), CV_8UC3));
		debugLog.clear();
	}

	~Application()
	{
		ImageRelease(mTexture);
		for (auto &image : mImageList)
			image->Release();
	}

public:
	bool exit_app = false;

private:
	std::string mCurrentImagePath{};
	std::string mFolderPath{};
	std::string mSaveFolderPath{};
	float mWidth = 6;
	float mHeight = 9;
	ImVec4 mBgColor = {1.0f, 1.0f, 1.0f, 1.0f};
	std::vector<Ref<ImageInfo>> mImageList{};
	int mCurrentIdex;
	int mPreviousIdex;
	cv::Mat mCurrentMat;
	Texture2D mTexture;
	int mAlgorithmItem = 0;
	bool mEnableFaceDetection = false;
	float mLimitConfident = 0.5;
	float mPreviousLimitConfident = 0.5;
	cv::Size resizeFaceDetection {300,300};
	cv::Size previousResizeFaceDetection {300,300};
	std::vector<std::string> debugLog;
};

int main()
{
	Window window("Resize", 1080, 720, true);

	window.set_key_callback([&](int key, int action) noexcept
							{
        if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
            window.set_should_close();
        } });

	Application app;
	window.run([&]
			   {
		app.MenuBarFunction();
		app.Inspection();
        if(app.exit_app)
            window.set_should_close();
        app.ViewFunction(); });
	return 0;
}