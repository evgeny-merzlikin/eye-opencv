#include "GLFWMain.h"

void openCVThreadFunc()
{

	cv::Mat bayerRaw( eye->getHeight(), eye->getWidth(), CV_8UC1 ); 
	cv::Mat temp( eye->getHeight(), eye->getWidth(), CV_8UC1 );
	cv::Mat temp2( eye->getHeight(), eye->getWidth(), CV_8UC3 );
	cv::Mat greyImage( 480, 640, CV_8UC1 );
	//Mat greyMask( eye->getHeight(), eye->getWidth(), CV_8UC1 );
	cv::Mat greyMask( 480, 640, CV_8UC1 );
	//Mat rgbImage( eye->getHeight(), eye->getWidth(), CV_8UC3 );
	cv::Mat rgbImage( 480, 640, CV_8UC3 );
	//Mat hsvImage( 480, 640, CV_8UC3 );
	std::vector<std::vector<cv::Point> > contours;
	std::vector<std::vector<cv::Point> > circles;

	grayTexture.data = nullptr;
	rgbTexture.data = nullptr;

	while ( eye && eye->isStreaming() ) {
		eye->getFrame( (uint8_t*)bayerRaw.data );


		switch ( cameraParameters.view_mode ) {
		case 0: // show bayer image
			if (eye->getWidth() == 320) {
				resize(bayerRaw, greyImage, greyImage.size(), 0, 0, static_cast<cv::InterpolationFlags>(opencvParams.resizeInterpolation));
				grayTexture.data = greyImage.data;
			} else {
				grayTexture.data = bayerRaw.data;
			}
			rgbTexture.data = nullptr;
			break;

		case 1: // show grey image
			cvtColor( bayerRaw, temp, cv::COLOR_BayerGB2GRAY );



			if (eye->getWidth() == 320) {
				resize(temp, greyImage, greyImage.size(), 0, 0, static_cast<cv::InterpolationFlags>(opencvParams.resizeInterpolation));
			} else {
				greyImage = temp;
			}

			grayTexture.data = greyImage.data;
			rgbTexture.data = nullptr;
			break;

		case 2:	 // show bgr color image
			cvtColor( bayerRaw, temp2, cv::COLOR_BayerGB2RGB );

			if (eye->getWidth() == 320) {
				resize(temp2, rgbImage, greyImage.size(), 0, 0, static_cast<cv::InterpolationFlags>(opencvParams.resizeInterpolation));
			} else {
				rgbImage = temp2;
			}

			grayTexture.data = nullptr;
			rgbTexture.data = rgbImage.data;
			break;

		case 3: // binary threshold of grey image
			cvtColor( bayerRaw, temp, cv::COLOR_BayerGB2GRAY );

			// lock mutex
			mtx.lock();
			if (eye->getWidth() == 320) {
				resize(temp, greyImage, greyImage.size(), 0, 0, static_cast<cv::InterpolationFlags>(opencvParams.resizeInterpolation));
			} else {
				greyImage = temp;
			}
			threshold( greyImage, greyMask, opencvParams.binary.min, opencvParams.binary.max, cv::THRESH_BINARY );
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

		case 4: // adaptive threshold of grey image
			cvtColor( bayerRaw, temp, cv::COLOR_BayerGB2GRAY );
			// lock mutex
			mtx.lock();

			if (eye->getWidth() == 320) {
				resize(temp, greyImage, greyImage.size(), 0, 0, static_cast<cv::InterpolationFlags>(opencvParams.resizeInterpolation));
			} else {
				greyImage = temp;
			}
			adaptiveThreshold( greyImage, greyMask, opencvParams.gauss.maxValue, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, opencvParams.gauss.blockSize, opencvParams.gauss.floatC );
			grayTexture.data = greyMask.data;
			rgbTexture.data = nullptr;

			// unlock mutex
			mtx.unlock();
			break;

		case 5: // red color threshold 

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
			/*
			vector<Vec3f> circles;
			HoughCircles(red_hue_image, circles, HOUGH_GRADIENT, 1, red_hue_image.rows/8, 100, 20, 0, 0);


			for(size_t current_circle = 0; current_circle < circles.size(); ++current_circle) {
			Point center(round(circles[current_circle][0]), round(circles[current_circle][1]));
			int radius = round(circles[current_circle][2]);

			circle(rgbImage, center, radius, Scalar(0, 255, 0), 5);
			}
			*/

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