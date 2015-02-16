#ifndef FIND_REGIONS_H
#define FIND_REGIONS_H

#include <cv.h>

#define MAX_COMPONENTS 1024
#define MINIMAL_AREA_LIMIT 80
#define NUM_COMPONENTS 8



inline int getPixelValue(IplImage *img,int x,int y)
{
	return ((unsigned char*)(img->imageData + x*img->widthStep))[y];
}
inline void setPixelValue(IplImage *img,int x,int y,int value)
{
	((unsigned char*)(img->imageData + x*img->widthStep))[y] = value;
}

inline int getMatValue(IplImage *M,int x,int y)
{
	return ((unsigned short*)(M->imageData + x * M->widthStep))[y];
}
inline void setMatValue(IplImage *M,int x,int y,int value)
{
	((unsigned short*)(M->imageData + x * M->widthStep))[y] = value;
}

inline void setPixelValueRGB(IplImage *img,int x,int y,int R,int G,int B)
{
	((unsigned char*)(img->imageData + x*img->widthStep))[y * img->nChannels + 0] = B;
	((unsigned char*)(img->imageData + x*img->widthStep))[y * img->nChannels + 1] = G;
	((unsigned char*)(img->imageData + x*img->widthStep))[y * img->nChannels + 2] = R;
}

void get_labelarray_of_components(IplImage *inbin,IplImage *labelarray);
int get_number_of_components(IplImage *labelarray,int face_limit);
void get_largest_component(IplImage *largest_component,IplImage *grayimg,IplImage *labelarray);
void get_brightest_component(IplImage *brightest_component,IplImage *grayimg,IplImage *labelarray);
float get_mostskn_component(IplImage *mostskn_component,IplImage *grayimg,IplImage *labelarray);
float get_perimeter(IplImage *mostskn_component);
void clear_small_components(IplImage *labelarray,IplImage *skin_mask);
#endif //FIND_REGIONS_H
