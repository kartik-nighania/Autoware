/*
 *  Copyright (c) 2015, Nagoya University
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither the name of Autoware nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <cstdlib>
#include <cstdint>
#include <iostream>

#include <opencv/cv.h>
#include <opencv/highgui.h>

#include <ros/ros.h>
#include <sensor_msgs/image_encodings.h>
#include <sensor_msgs/Image.h>
#include <dynamic_reconfigure/server.h>
#include <fake_drivers/FakeCameraConfig.h>

static void update_img_msg(const char* image_file, IplImage** img_p, sensor_msgs::Image* msg)
{
	if (image_file == nullptr || image_file[0] == '\0') {
		return;
	}
	std::cerr << "Image='" << image_file << "'" << std::endl;
	IplImage* img = cvLoadImage(image_file, CV_LOAD_IMAGE_ANYDEPTH | CV_LOAD_IMAGE_ANYCOLOR);
	if (img == nullptr) {
		std::cerr << "Can't load " << image_file << "'" << std::endl;
		return;
	}
	if (*img_p != nullptr) {
		cvReleaseImage(img_p); // Free allocated data
	}
	*img_p = img;

	msg->width = img->width;
	msg->height = img->height;
	msg->is_bigendian = 0;
	msg->step = img->widthStep;

	uint8_t *data_ptr = reinterpret_cast<uint8_t*>(img->imageData);
	std::vector<uint8_t> data(data_ptr, data_ptr + img->imageSize);
	msg->data = data;

	msg->encoding = (img->nChannels == 1) ? 
		sensor_msgs::image_encodings::MONO8 : 
		sensor_msgs::image_encodings::RGB8;
}

static void callback(fake_drivers::FakeCameraConfig &config, uint32_t level,
		     IplImage** img_p, sensor_msgs::Image* msg, ros::Rate** rate_p)
{
	std::cerr << "image_file=" << config.image_file << std::endl;
	update_img_msg(config.image_file.c_str(), img_p, msg);

	std::cerr << "fps=" << config.fps << std::endl;
	if (*rate_p != nullptr) {
		delete *rate_p;
	}
	*rate_p = new ros::Rate(config.fps);
}

int main(int argc, char **argv)
{
	ros::init(argc, argv, "fake_camera");
	ros::NodeHandle n;

	if (argc < 2) {
		std::cerr << "Usage: fake_driver image_file" << std::endl;
		std::exit(1);
	}
	
	IplImage* img = nullptr;
	sensor_msgs::Image msg;
	ros::Rate* rate = nullptr;

	dynamic_reconfigure::Server<fake_drivers::FakeCameraConfig> server;
	dynamic_reconfigure::Server<fake_drivers::FakeCameraConfig>::CallbackType f;
	f = boost::bind(&callback, _1, _2, &img, &msg, &rate);
	server.setCallback(f);

	update_img_msg(argv[1], &img, &msg);

	ros::Publisher pub = n.advertise<sensor_msgs::Image>("image_raw", 1000);

	uint32_t count = 0;
	while (ros::ok()) {
		msg.header.seq = count;
		msg.header.frame_id = count;
		msg.header.stamp.sec = ros::Time::now().toSec();
		msg.header.stamp.nsec = ros::Time::now().toNSec();
		pub.publish(msg);
		ros::spinOnce();
		if (rate != nullptr) {
			rate->sleep();
		}
		count++;
	}
	cvReleaseImage(&img);//Free allocated data
	return 0;
}
