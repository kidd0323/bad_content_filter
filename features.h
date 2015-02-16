#ifndef FEATURES_H
#define FEATURES_H
#include <cv.h>

struct TELLIPSE
{
        int bnoellipse;
        float r_; // the row coordinate of the centroid
        float c_; // the column coordinate of the centroid
        float d,e,f;
        float fmjlen; // the length of the major axis
        float fmnlen; // the length of the minor axis
        float fang; // the angle in radius
        float fabsang;
        float fdist;
        float fratl; // ratl = minor_length / major_length
        float frata; // rata = area_ellipse / area_image
        float favgin; // average skinness inside
        float favgout; // average skinness outside
};

struct TFACE
{
	int nfaces;
	int face_x;
	int face_y;
	int face_size;
	float face_skin_area;
};

void cal_features(IplImage *skin_mask,IplImage * gray_img,struct TFACE *tFace,float features[],int id);


#endif //FEATURES_H
