#ifndef _PS3_EYE_PARAMETERS_
#define _PS3_EYE_PARAMETERS_

#include "GLFWMain.h"

struct PS3EyeParameters {

	bool autoGain;
	bool autoExposure;    
	bool autoWB;  
	bool horizontalFlip;  
	bool verticalFlip;    

	bool cameraFound;

	int width;
	int height;
	int fps;

	int gain;
	int redBalance;
	int blueBalance;
	int greenBalance;
	int exposure;    
	int sharpness;   
	int contrast;    
	int brightness;  
	int hue;         

	int view_mode;

	void update( bool updateAll = false ) 
	{

		if ( updateAll ) {

			autoGain = eye->getAutogain();
			autoExposure = eye->getAutoExposure();
			autoWB = eye->getAutoWhiteBalance();
			gain = eye->getGain();  
			exposure = eye->getExposure();  
			blueBalance = eye->getBlueBalance();
			greenBalance = eye->getGreenBalance();
			redBalance = eye->getRedBalance();
			sharpness = eye->getSharpness();   
			contrast = eye->getContrast();    
			brightness = eye->getBrightness();  
			hue = eye->getHue();         
			horizontalFlip = eye->getFlipH();  
			verticalFlip = eye->getFlipV();    

			return;
		}

		if ( autoGain ) 
			gain = eye->getGain();  

		if ( autoExposure )
			exposure = eye->getExposure();  

		if ( autoWB ) {
			blueBalance = eye->getBlueBalance();
			greenBalance = eye->getGreenBalance();
			redBalance = eye->getRedBalance();
		}

	}

};

#endif