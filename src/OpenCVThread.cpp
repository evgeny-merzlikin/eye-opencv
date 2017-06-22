#include "GLFWMain.h"

#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp";;

void openCVThreadFunc()
{

	cv::Mat bayerRaw( eye->getHeight(), eye->getWidth(), CV_8UC1 ); 
	cv::Mat rgb, grey, mask;

	grayTexture.data = nullptr;
	rgbTexture.data = nullptr;

	while ( eye && eye->isStreaming() ) {
		eye->getFrame( (uint8_t*)bayerRaw.data );
		cvtColor( bayerRaw, rgb, cv::COLOR_BayerGB2RGB );
		cvtColor( bayerRaw, grey, cv::COLOR_BayerGB2GRAY );

		switch ( opencvParams.mode ) {
		case OpenCVParameters::CurrentMode::ONLY_STREAM : // show rgb image

			
			grayTexture.data = nullptr;
			rgbTexture.data = rgb.data;
			break;

		case OpenCVParameters::CurrentMode::SETUP_POSITION : // show rgb image + position png

			grayTexture.data = nullptr;
			rgbTexture.data = rgb.data;
			break;

		case OpenCVParameters::CurrentMode::BW_TRACKING: // binary threshold of grey image
			// adjust brightness and contrast of grey image

			// binary threshold
			threshold( greyImage, greyMask, opencvParams.binary.min, opencvParams.binary.max, cv::THRESH_BINARY );

			// detect contours
			contours.clear(); circles.clear();
			
			findContours( greyMask, contours, cv::RETR_LIST , cv::CHAIN_APPROX_SIMPLE);
			for( size_t i = 0; i< contours.size(); i++ ) // iterate through each contour.
				{
        int area = ROUND_2_INT( contourArea( contours[i] ) );  //  Find the area of contour
				
        if (( opencvParams.contoursArea.min < area ) && ( opencvParams.contoursArea.max > area ))
        {
					circles.push_back(contours[i]);
        }
    }

    drawContours( greyImage, circles, -1, cv::Scalar( 0, 255, 0 ), 2 ); // Draw the largest contour using previously stored index.


			grayTexture.data = greyImage.data;
			rgbTexture.data = nullptr;

			// un lock mutex
			mtx.unlock();
			break;

		case OpenCVParameters::CurrentMode::COLOR_TRACKING: //  color tracking 

			cvtColor( bayerRaw, temp2, cv::COLOR_BayerGB2RGB );

			if (eye->getWidth() == 320) {
				resize(temp2, rgbImage, greyImage.size(), 0, 0, static_cast<cv::InterpolationFlags>(opencvParams.resizeInterpolation));
			} else {
				rgbImage = temp2;
			}

			medianBlur(rgbImage, rgbImage, 3);

			cv::Mat hsvImage;
			cvtColor(rgbImage, hsvImage, cv::COLOR_RGB2HSV);

			cv::Mat lower_red_hue_range;
			cv::Mat upper_red_hue_range;
			cv::inRange(hsvImage, cv::Scalar(0, 100, 100), cv::Scalar(10, 255, 255), lower_red_hue_range);
			cv::inRange(hsvImage, cv::Scalar(160, 100, 100), cv::Scalar(179, 255, 255), upper_red_hue_range);

			//Mat red_hue_image;
			cv::addWeighted(lower_red_hue_range, 1.0, upper_red_hue_range, 1.0, 0.0, greyMask);

			cv::GaussianBlur(greyMask, greyMask, cv::Size(9, 9), 2, 2);


			grayTexture.data = greyMask.data;
			rgbTexture.data = nullptr; //rgbImage.data;
			break;

		} // end switch

	} // end while

	//keypoints.clear();
	grayTexture.data = nullptr;
	rgbTexture.data = nullptr;

}

void startOpenCVThread()
{
	std::thread cv_thread(openCVThreadFunc);
	cv_thread.detach();
}