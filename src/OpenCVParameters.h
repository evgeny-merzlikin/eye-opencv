#ifndef _OPENCV_PARAMETERS_
#define _OPENCV_PARAMETERS_

struct OpenCVParameters {

	enum CurrentMode { ONLY_STREAM, SETUP_POSITION, ADJUSTING_EXPOSURE, BW_TRACKING, COLOR_TRACKING };

	CurrentMode mode;
	
	double brightness, contrast;
	int binaryThreshold;
	double areaThreshold;

};

#endif