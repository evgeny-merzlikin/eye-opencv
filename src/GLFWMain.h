#ifndef _GLFW_MAIN_H_
#define _GLFW_MAIN_H_

#include <stdlib.h>     // atoi
#include <thread>
#include <mutex>
#include <atomic>

#include "imgui.h"
#include "imgui_impl_glfw.h"

#include "ps3eye.h"
#include "GLFW/glfw3.h"
#include "teVirtualMIDI.h"
#include "opencv2/opencv.hpp"
namespace cv {}

#include "GLTexture.h"
#include "PS3EyeParameters.h"
#include "OpenCVParameters.h"

#define MAX_SYSEX_BUFFER	65535
#define IM_ARRAYSIZE(_ARR)  ((int)(sizeof(_ARR)/sizeof(*_ARR)))
#define IM_MAX(_A,_B)       (((_A) >= (_B)) ? (_A) : (_B))
#define ROUND_2_INT(f) ((int)(f >= 0.0 ? (f + 0.5) : (f - 0.5)))
#define PORTNAME L"EyeMidiPort"

extern OpenCVParameters opencvParams;
extern PS3EyeParameters cameraParameters;
extern ps3eye::PS3EYECam::PS3EYERef eye;
extern GLFWwindow* window;
extern LPVM_MIDI_PORT MIDIport;
extern std::mutex mtx;  
extern GLTexture grayTexture, rgbTexture;

//opencv 
void startOpenCVThread();

//gui
void createGUI();

//opengl functions - opengl.cpp
void initOpenGL();
void terminateOpenGL();
void render();

// math for round 
inline double round(double value) { return value < 0 ? -std::floor(0.5 - value) : std::floor(0.5 + value); }
inline double round(float value) { return value < 0 ? -std::floor(0.5 - value) : std::floor(0.5 + value); }

// midi functions - MIDI.cpp
void sendMidiNote(unsigned char channel, unsigned char pitch, unsigned char volume);
void sendMidiNoteOn(unsigned char channel, unsigned char pitch, unsigned char volume);
void createMIDIPort();
void destroyMIDIPort();

#endif