#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/core/core.hpp>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <iostream>
#include <sstream>

#include "common.h"
#include "videodevice.h"
#include "serial.h"
using namespace std;
using namespace cv;


int gatevalue;
int pointnum;
int minarea;
int maxarea;
int maxHWrate100;
int minHWrate100;
int morph_size;
int PolyEpsilon;
float lowesthighestvalidrate;

unsigned char * yuv_buffer_pointer;
size_t len;

int convertyuv2mat(unsigned char* yuv_buffer_pointer,Mat& out,size_t len);

Mat src_gray(IMG_HEIGTH,IMG_WIDTH,CV_8UC1);

Mat grad, grad2bit, drawing;



string window_name = "Gray Img";
string window_nameG = "2-Bit Img";
string window_nameC = "Contours";
string window_nameM = "morph";



bool ifshowimg = false;
bool ifshowtime = false;
bool ifshowctrl = false;

/** @function main */
int main(int argc, char** argv)
{
	gatevalue = 68;
	pointnum = 20/2;
	minarea = 400/4;
	maxarea = 40000/4;
	maxHWrate100 = 1000;
	minHWrate100 = 200;
	morph_size = 1;
	PolyEpsilon = 2;
	lowesthighestvalidrate = 0.9;

	if(argc == 2)
	{
		switch(argv[1][0])
		{
			case 'g':
				ifshowimg = true;
				break;
			case 't':
				ifshowtime = true;
				break;
			case 'a':
				ifshowimg = true;
				ifshowtime = true;
				break;
			default :
				break;
		}
	}
	
	

	
	
	
	serial_init();
	/// Load an image

	VideoDevice vd("/dev/video0");
	int retry = 10;
	while(vd.get_frame(&yuv_buffer_pointer,&len) == false)
	{
		retry --;
		vd.unget_frame();
		if(retry <= 0)
		{
			cout << "failed to get img\n";
			return -1;
		}
	}
	cout << "get image OK\n";
	vd.unget_frame();
	
	
	if(ifshowimg == true)
	{
//		namedWindow(window_name, CV_WINDOW_AUTOSIZE);
		namedWindow(window_nameC, CV_WINDOW_AUTOSIZE);
		namedWindow(window_nameG, CV_WINDOW_AUTOSIZE);
		namedWindow(window_nameM, CV_WINDOW_AUTOSIZE);
		
		createTrackbar("GateValue", window_nameG, &gatevalue, 0xff);	
		createTrackbar("MinArea", window_nameC, &minarea, minarea*3);
		createTrackbar("MaxArea", window_nameC, &maxarea, maxarea*3);
		createTrackbar("maxHW100", window_nameC, &maxHWrate100, 1000);
		createTrackbar("minHW100", window_nameC, &minHWrate100, 1000);
		createTrackbar("MorphSize", window_nameM, &morph_size, 10);
	}
	
	
	int offset = 0;
	bool sendflag = false;
	int badframe = 0;
	while (1)
	{
		double timestart = getTickCount();
        ///////////////////////////////////STEP 1//////////////////////////////////////
      	while(vd.get_frame(&yuv_buffer_pointer,&len)==false)//wait for next frame
       		;
		double times1 = getTickCount();
		///////////////////////////////////STEP 2//////////////////////////////////////
       	serial_send_int(offset,sendflag);
		convertyuv2mat(yuv_buffer_pointer,src_gray,len);
		vd.unget_frame();
		double times2 = getTickCount();
        ///////////////////////////////////STEP 3//////////////////////////////////////
		threshold(src_gray, grad2bit, gatevalue, 0xff, CV_THRESH_BINARY_INV);
		if(ifshowimg == true)
			imshow(window_nameG,grad2bit);
		double times3 = getTickCount();
        ///////////////////////////////////STEP 4//////////////////////////////////////
		Mat element = getStructuringElement(MORPH_RECT,Size(2 * morph_size + 1, 2 * morph_size + 1));
		morphologyEx(grad2bit, grad2bit, MORPH_CLOSE, element);
		double times4 = getTickCount();
		if(ifshowimg == true)
			imshow(window_nameM,grad2bit);
        ///////////////////////////////////STEP 5//////////////////////////////////////
		vector<vector<Point> > contours(0);
		vector<vector<Point> > contour(0);
		findContours(grad2bit, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);
		double times5 = getTickCount();
        ///////////////////////////////////STEP 6//////////////////////////////////////
		int validarea = 0;
		Scalar color = Scalar(255, 255, 255);
		Scalar colorR = Scalar(0, 0, 255);
		for (vector<vector<Point> > ::iterator itc = contours.begin(); itc != contours.end(); ++itc)
		{
			if (itc->size() >= pointnum)
				contour.push_back(*itc);
		}
        double times6 = getTickCount();
		///////////////////////////////////STEP 7//////////////////////////////////////
		
		for (vector<vector<Point> > ::iterator itc = contour.begin(); itc != contour.end();)
		{
			Rect minRect = boundingRect(*itc);
			float HWrate = (float)(minRect.height) / minRect.width;
			if ((HWrate * 100 < minHWrate100) || (HWrate * 100 > maxHWrate100))
				itc = contour.erase(itc);
			else
			{
				++itc;
			}
		}
		

        double times7 = getTickCount();
		///////////////////////////////////STEP 8//////////////////////////////////////
		for (vector<vector<Point> > ::iterator itc = contour.begin(); itc != contour.end();)
		{
			double area = contourArea(*itc);
			if ((area < minarea) || (area > maxarea))
				itc = contour.erase(itc);
			else
				++itc;
		}
        double times8 = getTickCount();
        ///////////////////////////////////STEP 9//////////////////////////////////////
		drawing = Mat::zeros(grad2bit.size(), CV_8UC3);
		//if there is a point
		if (contour.size() >= 1)//verify
		{
			vector<float> validpointrate(0);
			for (vector<vector<Point> > ::iterator itc = contour.begin(); itc != contour.end(); ++itc)
			{
				int pointvalidnum = 0;
				int pointinvalidnum = 0;
				Point2f point2test;
				Rect minRect = boundingRect(*itc);


				vector<Point> Cross_points(0);
				float x = minRect.width;
				float y = minRect.height;
				float rx = 0.25;
				float ry = 0.4;
				float rrx = 0;
				float rry = 0;
				Point s = minRect.tl();
				Cross_points.push_back(s + Point(x * rx, y * rry));
				Cross_points.push_back(s + Point(x * (1 - rx), y * rry));
				Cross_points.push_back(s + Point(x * (1 - rx), y * ry));
				Cross_points.push_back(s + Point(x * (1 - rrx), y * ry));
				Cross_points.push_back(s + Point(x * (1 - rrx), y * (1 - ry)));
				Cross_points.push_back(s + Point(x * (1 - rx), y * (1 - ry)));
				Cross_points.push_back(s + Point(x * (1 - rx), y * (1 - rry)));
				Cross_points.push_back(s + Point(x * rx, y * (1 - rry)));
				Cross_points.push_back(s + Point(x * rx, y * (1 - ry)));
				Cross_points.push_back(s + Point(x * rrx, y * (1 - ry)));
				Cross_points.push_back(s + Point(x * rrx, y * ry));
				Cross_points.push_back(s + Point(x * rx, y * ry));

				for (int ii = 0; ii < Cross_points.size(); ii++)
					if (pointPolygonTest(*itc, Cross_points[ii], false) <= 0)//outside
						pointvalidnum++;
					else
						pointinvalidnum++;
				if(ifshowimg == true)
					polylines(drawing, Cross_points, true, colorR);

				Cross_points.clear();
				x = minRect.width;
				y = minRect.height;
				rx = 0.45;
				ry = 0.48;
				rrx = 0.1;
				rry = 0.05;
				s = minRect.tl();
				Cross_points.push_back(s + Point(x * rx, y * rry));
				Cross_points.push_back(s + Point(x * (1 - rx), y * rry));
				Cross_points.push_back(s + Point(x * (1 - rx), y * ry));
				Cross_points.push_back(s + Point(x * (1 - rrx), y * ry));
				Cross_points.push_back(s + Point(x * (1 - rrx), y * (1 - ry)));
				Cross_points.push_back(s + Point(x * (1 - rx), y * (1 - ry)));
				Cross_points.push_back(s + Point(x * (1 - rx), y * (1 - rry)));
				Cross_points.push_back(s + Point(x * rx, y * (1 - rry)));
				Cross_points.push_back(s + Point(x * rx, y * (1 - ry)));
				Cross_points.push_back(s + Point(x * rrx, y * (1 - ry)));
				Cross_points.push_back(s + Point(x * rrx, y * ry));
				Cross_points.push_back(s + Point(x * rx, y * ry));

				for (int ii = 0; ii < Cross_points.size(); ii++)
					if (pointPolygonTest(*itc, Cross_points[ii], false) >= 0)//inside
						pointvalidnum++;
					else
						pointinvalidnum++;
				validpointrate.push_back(pointvalidnum / (float)(pointvalidnum + pointinvalidnum));
				if(ifshowimg == true)
					polylines(drawing, Cross_points, true, colorR);
			}
			float highestvalidrate = -1;
			for (int ii = 0; ii < validpointrate.size(); ii++)
			{
				if (validpointrate[ii] >= highestvalidrate)
					highestvalidrate = validpointrate[ii];
			}
			vector<vector<Point> > ::iterator itc = contour.begin();
			vector<float> ::iterator itcrate = validpointrate.begin();
			for (; itc != contour.end();)
			{
				if ((*itcrate != highestvalidrate)||highestvalidrate <= lowesthighestvalidrate)
				{
					itcrate = validpointrate.erase(itcrate);
					itc = contour.erase(itc);
				}
				else
				{
					itcrate++;
					itc++;
				}
			}
		}
		if(ifshowimg == true)
			imshow(window_nameC, drawing);
		double times9 = getTickCount();
		///////////////////////////////////STEP 10//////////////////////////////////////
		if(ifshowimg == true)
		{
			for (int ii = 0; ii < contour.size(); ii++)
			{
				drawContours(drawing, contour, ii, color);
			}
			imshow(window_nameC, drawing);
		}
		if(contour.size() == 1)
		{
			Rect minRect = boundingRect(contour[0]);
			Point middle = minRect.tl() + minRect.br();
			offset = (middle.x - (int)IMG_WIDTH)/2;
			sendflag = true;
		}
		else
		{
			offset = 0;
			sendflag = false;
		}
        double times10 = getTickCount();
		///////////////////////////////////// end /////////////////////////////////////
		if(ifshowtime == true)
		{
			double freq = getTickFrequency();

			cout << "s1:"<< (times1-timestart)/freq*1000;
			cout << "\ts2:"<< (times2 - times1)/freq*1000;
			cout << "\ts3:"<< (times3 - times2)/freq*1000;
			cout << "\ts4:"<< (times4 - times3)/freq*1000;
			cout << "\ts5:"<< (times5 - times4)/freq*1000;
			cout << "\ts6:"<< (times6 - times5)/freq*1000;
			cout << "\ts7:"<< (times7 - times6)/freq*1000;
			cout << "\ts8:"<< (times8 - times7)/freq*1000;
			cout << "\ts9:"<< (times9 - times8)/freq*1000;
			cout << "\ts10:"<< (times10 - times9)/freq*1000;
			cout << "\tall:"<< (times10 - timestart)/freq*1000;
			cout << "\tnum: " << contour.size();
			if((times10 - timestart)/freq*1000 > 70)
				badframe++;
			cout << "\tb:"<<badframe << endl;
		}
		if(ifshowimg)
			waitKey(1);
	}
	serial_deinti();
	return 0;
}

int convertyuv2mat(unsigned char* yuv_buffer_pointer,Mat& matout,size_t len)
{
    unsigned long in = 0;
    unsigned long out = 0;
    int y0,y1;
    unsigned char* outbuffer;
    if(!matout.isContinuous())
        return -1;
    outbuffer = matout.ptr<unsigned char>(0);
    for(in = 0; in < IMG_WIDTH * IMG_HEIGTH * 2; in += 4)
    {
        y0 = yuv_buffer_pointer[in + 0];
        y1 = yuv_buffer_pointer[in + 2];

        outbuffer[out++] = y0;
        outbuffer[out++] = y1;

    }
    return 0;
}
