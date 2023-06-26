# Opencv Face Detection Imgui
[![Build Status](https://github.com/kybuivan/opencv-libfacedetection-imgui/actions/workflows/windows.yml/badge.svg)](https://github.com/kybuivan/opencv-libfacedetection-imgui/actions)  
Resize is an open-source image processing application built using C++. It allows users to perform a variety of image manipulation tasks, such as resizing, blurring, sharpening, and colorization.

![screenshot](/screenshot/Capture1.PNG "screenshot")

## Getting started
Prerequisites
To build and run Resize, you will need the following:

- C++20
- CMake 3.16 or higher
- OpenGL 3.3 or higher
- [GLFW](https://github.com/glfw/glfw)
- [GLAD](https://github.com/Dav1dde/glad)
- [ImGui](https://github.com/ocornut/imgui)
- [nfd](https://github.com/mlabbe/nativefiledialog)
- [OpenCV](https://github.com/opencv/opencv)

## Building
To build Resize:

1. Clone the repository:
```bash
git clone https://github.com/kybuivan/opencv-libfacedetection-imgui.git
```
2. Initialize the cmake submodule recursively:
```bash
cd polaroid
git submodule update --init --recursive
```
3. Create a build directory:
```bash
mkdir build
cd build
```
4. Run CMake:
```bash
cmake ..
```
5. Build the project:
```bash
cmake --build .
```

## Running
To run Resize, simply execute the `resize` executable that was built in the previous step.

## Usage
Resize has a simple menu bar that allows users to open images and perform basic operations. The following options are available:

- File
	- New: Creates a new image window.
	- Open File...: Opens a file dialog that allows users to select an image file to open.
	- Open Folder...: Opens a file dialog that allows users to select a folder containing images to open.
	- Save As...: Saves the current image in the active window.
	- Save All: Saves all the images in the image list.
	- Exit: Exits the application.
- Edit
	- Undo: Not implemented.
	- Redo: Not implemented.
- Help
	- About: Not implemented.

## License
This project is licensed under the Apache-2.0 license - see the [LICENSE](https://github.com/kybuivan/opencv-libfacedetection-imgui/blob/main/LICENSE) file for details.

## Acknowledgments
This project was built using the following libraries: GLFW, GLAD, ImGui, nfd, and OpenCV.
