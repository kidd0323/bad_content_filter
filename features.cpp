#include "stdafx.h"
#include "features.h"
#include <stdio.h>
#include <highgui.h>
#include "findregions.h"
#define M_PI        3.14159265358979323846
void gray2bin(IplImage *skin_mask,IplImage *binimg,int ithreshold)
{
	int i,j;

	for(i = 0;i < skin_mask->width;i ++)
		for(j = 0;j < skin_mask->height;j ++)
		{
			if(getPixelValue(skin_mask,j,i) > ithreshold)
				((unsigned char*)(binimg->imageData + j * binimg->widthStep))[i] = 0;
			else
				((unsigned char*)(binimg->imageData + j * binimg->widthStep))[i] = 1;
		}	
}

float skin_area(IplImage *binimg)
{
	float area = 0.0;
	int i,j;
	for(i = 0; i < binimg->width; i++)
		for(j = 0; j < binimg->height; j++)
			area += ((unsigned char*)(binimg->imageData + j * binimg->widthStep))[i];
	return area;
}

void cal_ellipse_features(IplImage *img, struct TELLIPSE *tEllipse)
{
	float A,r_,c_,u_rr,u_cc,u_rc,d,e,f;
	float delta;
	float fmjlen,fmnlen,farea,fareaw;
	float fdist,fang,fabsang,fratl; // ratl = minor_length / major_length
	float frata; // rata = area_ellipse / area_image
	float favgin,favgout; // average skinness inside and outside
	int icnt,ib,it,jl,jr;
	int i,j,k;
	int Height = img->height;
	int Width = img->width;

	tEllipse->bnoellipse=1;

	/* calculating A, r_, c_, u_rr, u_cc and u_rc */
	A=0.0;
	for(i=0;i<Height;i++)
		for(j=0;j<Width;j++)
			if (getPixelValue(img,i,j) > 0)
				A ++;
	if(A == 0.0) {
		/* no skin in this image, assume no ellipse */
#ifdef DEBUG
		printf("cal_ellipse_features : A=0\n" );
#endif
		return;
	}

	r_=c_=0.0;
	for(i=0;i<Height;i++)
		for(j=0;j<Width;j++)
		{
			//k=getPixelValue(img,i,j);
			if (getPixelValue(img,i,j) > 0)
				k = 1;
			else k = 0;

			r_+=i*k;
			c_+=j*k;
		}
		r_/=A;
		c_/=A;

		u_rr=u_cc=u_rc=0.0;
		for(i=0;i<Height;i++)
			for(j=0;j<Width;j++)
			{
				//k=getPixelValue(img,i,j);
				if (getPixelValue(img,i,j) > 0)
					k = 1;
				else k = 0;

				u_rr+=i*i*k;
				u_cc+=j*j*k;
				u_rc+=i*j*k;
			}
			u_rr=u_rr/A-r_*r_;
			u_cc=u_cc/A-c_*c_;
			u_rc=u_rc/A-r_*c_;


			/* delta */
			delta=u_rr*u_cc-u_rc*u_rc;
			if(delta<=0.0) {
				/* image has no ellipse, assume no ellipse */
#ifdef DEBUG
				printf("cal_ellipse_features : delta<=0.0\n" );
#endif
				return;
			}

			/* d, e and f */
			d=u_cc/(4.0*delta);
			e=-u_rc/(4.0*delta);
			f=u_rr/(4.0*delta);

			/* distance between the centroid and the center */
			fdist=sqrt((r_-Height/2.0)*(r_-Height/2.0)+(c_-Width/2.0)*(c_-Width/2.0));
			fdist/=sqrt((double)Width*Width+Height*Height);

			/* four cases for the axis and angle */
			if(u_rc==0.0 && u_rr>u_cc)
			{
				fang=-M_PI/2.0;
				fmjlen=4.0*sqrt(u_rr);
				fmnlen=4.0*sqrt(u_cc);
			}
			else if(u_rc==0.0 && u_rr<=u_cc)
			{
				fang=0.0;
				fmjlen=4.0*sqrt(u_cc);
				fmnlen=4.0*sqrt(u_rr);
			}
			else if(u_rc!=0.0 && u_rr<=u_cc)
			{
				fang=atan(-2.0*u_rc/(u_cc-u_rr+sqrt((u_cc-u_rr)*(u_cc-u_rr)+4.0*u_rc*u_rc)));
				fmjlen=sqrt(8.0*(u_rr+u_cc+sqrt((u_rr-u_cc)*(u_rr-u_cc)+4.0*u_rc*u_rc)));
				fmnlen=sqrt(8.0*(u_rr+u_cc-sqrt((u_rr-u_cc)*(u_rr-u_cc)+4.0*u_rc*u_rc)));
			}
			else if(u_rc!=0.0 && u_rr>u_cc)
			{
				fang=atan((u_rr-u_cc+sqrt((u_rr-u_cc)*(u_rr-u_cc)+4.0*u_rc*u_rc))/(-2.0*u_rc));
				fmjlen=sqrt(8.0*(u_rr+u_cc+sqrt((u_rr-u_cc)*(u_rr-u_cc)+4.0*u_rc*u_rc)));
				fmnlen=sqrt(8.0*(u_rr+u_cc-sqrt((u_rr-u_cc)*(u_rr-u_cc)+4.0*u_rc*u_rc)));
			}
			else
			{
				printf("Wrong case: u_rc=%f u_rr=%f u_cc=%f!\nA=%f\n\n"
					,u_rc,u_rr,u_cc,A);
				//assuming no ellipse
				printf("cal_ellipse_features : skip reason 3\n" );
				return;
			}

			fabsang=fabs(fang)/(M_PI/2.0);
			if(fmjlen!=0.0)
				/* in fact, when delta>0, we're sure fmjlen>fmnlen>0.0 */
				fratl=fmnlen/fmjlen;
			else
			{
				/* image has no ellipse, assume no ellipse */
#ifdef DEBUG
				printf("cal_ellipse_features : skip reason 4\n" );
#endif
				return;
			}

			/* ratio of the area of the ellipse to that of the image */
			farea=4.0*M_PI*sqrt(delta);
			frata=farea / (Width*Height);

			/* favgin, favgout */
			icnt=0;
			fareaw=0.0;
			jl=(int)(-d/sqrt(d*(d*f-e*e)));
			jr=(int)(d/sqrt(d*(d*f-e*e)));
			for(j=jl;j<=jr;j++)
			{
				if(j+(int)c_>=0 && j+(int)c_<Width)
				{
					it=(int)((-e*j-sqrt(e*e*j*j-d*(f*j*j-1.0)))/d);
					ib=(int)((-e*j+sqrt(e*e*j*j-d*(f*j*j-1.0)))/d);
					for(i=it;i<=ib;i++)
						if(i+(int)r_>=0 && i+(int)r_<Height)
						{
							k=getPixelValue(img,i+(int)r_,j+(int)c_);

							fareaw+=k;
							icnt++;
						}
				}
			}
			favgin=(fareaw/icnt)/255.0;
			if(Width*Height-icnt==0)
				favgout=favgin;
			else
				favgout=((A-fareaw)/(Width*Height-icnt))/255.0;

			/* copy values of features to struct tEllipse */
			tEllipse->bnoellipse=0;
			tEllipse->r_=r_;
			tEllipse->c_=c_;
			tEllipse->d=d;
			tEllipse->e=e;
			tEllipse->f=f;
			tEllipse->fmjlen=fmjlen;
			tEllipse->fmnlen=fmnlen;
			tEllipse->fang=fang;
			tEllipse->fdist=fdist;
			tEllipse->fabsang=fabsang;
			tEllipse->fratl=fratl;
			tEllipse->frata=frata;
			tEllipse->favgin=favgin;
			tEllipse->favgout=favgout;
}
void add_edge(IplImage *binimg,IplImage *gray_img)
{
	int i,j;
	IplImage *pSobelYImg = NULL;
	IplImage *pSobelXImg = NULL;
	IplImage *pSobelTmpImg = NULL;

	pSobelYImg = cvCreateImage(cvGetSize(gray_img),IPL_DEPTH_8U,1);
	pSobelXImg = cvCreateImage(cvGetSize(gray_img),IPL_DEPTH_8U,1);
	pSobelTmpImg = cvCreateImage(cvGetSize(gray_img),IPL_DEPTH_16S,1);

	cvSobel(gray_img, pSobelTmpImg, 1, 0, 3);
	cvConvertScaleAbs(pSobelTmpImg,pSobelXImg);
	cvSobel(gray_img, pSobelTmpImg, 0, 1, 3);
	cvConvertScaleAbs(pSobelTmpImg,pSobelYImg);
	/* 
	for(i = 0;i < pImg->width;i ++)
	for(j = 0;j < pImg->height;j ++)
	{
	x = ((unsigned char *)(pSobelXImg->imageData + j * pSobelXImg->widthStep))[i];
	y = ((unsigned char *)(pSobelYImg->imageData + j * pSobelYImg->widthStep))[i];

	if (x > 0 && y > 0)
	((unsigned char *)(pSobelImg->imageData + j * pSobelImg->widthStep))[i] = x > y?x:y;
	else
	((unsigned char *)(pSobelImg->imageData + j * pSobelImg->widthStep))[i] = 0;
	}
	*/	
	for(i = 0;i < binimg->height;i ++)
		for(j = 0;j < binimg->width;j ++)
		{
			if (getPixelValue(pSobelXImg,i,j) > 128 || getPixelValue(pSobelYImg,i,j) > 128)
				setPixelValue(binimg,i,j,1);
		}
		cvReleaseImage(&pSobelTmpImg);
		cvReleaseImage(&pSobelXImg);
		cvReleaseImage(&pSobelYImg);
		return;
}
void change2show(IplImage *binimg,IplImage *showimg)
{
	int i,j;
	for(i = 0;i < binimg->height;i ++)
		for(j = 0;j < binimg->width;j ++)
		{
			if (getPixelValue(binimg,i,j) > 0)
				setPixelValue(showimg,i,j,255);
			else
				setPixelValue(showimg,i,j,0);
		}
		return;
}
void change2showRGB(IplImage *labelarray,IplImage *showimg)
{
	int R[8]={0,0,0,0,255,255,255,255};
	int G[8]={0,128,255,255,128,255,0,0};
	int B[8]={255,255,255,0,0,0,0,255};
	int i,j,k;

	for(i=0;i<labelarray->height;i++)
		for(j=0;j<labelarray->width;j++)
		{
			k = getMatValue(labelarray,i,j);
			if (k == 0)
			{
				setPixelValueRGB(showimg,i,j,0,0,0);
			}else
			{
				setPixelValueRGB(showimg,i,j,R[k%8],G[k%8],B[k%8]);
			}
		}
		return;
}

void cal_features(IplImage *skin_mask,IplImage *gray_img,struct TFACE *tFace,float features[],int id)
{
	IplImage *binimg = NULL;
	IplImage *labelarray = NULL;
	IplImage *mostskn_region = NULL;
	IplImage *showImg = NULL;
	IplImage *RGB = NULL;
	char skin_mask_path[255] = "./mask/";
	char skin_mask_filename[255];
	int number_of_regions;
	float perimeter,component_area,img_area,global_skin_area,sec_area;
	float dx,dy;
	struct TELLIPSE tEllipse;
	int i;

	img_area = skin_mask->width * skin_mask->height;
	for (i = 0 ;i < 9;i ++)
		features[i] = 0.0;
	binimg=cvCreateImage(cvGetSize(skin_mask),IPL_DEPTH_8U,1);
	gray2bin(skin_mask,binimg,4);

	//	showImg = cvCreateImage(cvGetSize(skin_mask),IPL_DEPTH_8U,1);
	/*	change2show(binimg,showImg);
	cvSaveImage("./0.jpg",showImg);
	*/

	if (skin_mask->width > 160 && skin_mask->height > 160)
	{
		add_edge(binimg,gray_img);
		// morph_open
		cvErode(binimg,binimg,NULL,1);
		cvDilate(binimg,binimg,NULL,1);
		// morph_close
		cvDilate(binimg,binimg,NULL,1);
		cvErode(binimg,binimg,NULL,1);
	}else
	{
		// morph_close
		cvDilate(binimg,binimg,NULL,1);
		cvErode(binimg,binimg,NULL,1);
		// morph_open
		cvErode(binimg,binimg,NULL,1);
		cvDilate(binimg,binimg,NULL,1);
	}
	global_skin_area = skin_area(binimg);



	//	add_edge(binimg,gray_img);

	//	change2show(binimg,showImg);
	//	cvSaveImage("./1.jpg",showImg);
	if( tFace->nfaces==0)
	{
		labelarray=cvCreateImage(cvGetSize(skin_mask),IPL_DEPTH_16U,1);
		get_labelarray_of_components(binimg,labelarray);
		/*
		RGB=cvCreateImage(cvGetSize(skin_mask),IPL_DEPTH_8U,3);
		change2showRGB(labelarray,RGB);	
		cvSaveImage("./rgb.jpg",RGB);
		cvReleaseImage(&RGB);
		*/
		number_of_regions=get_number_of_components(labelarray,0);
		/*	
		clear_small_components(labelarray,showImg);
		sprintf(skin_mask_filename,"%s%d.jpg",skin_mask_path,id);
		cvSaveImage(skin_mask_filename,showImg);
		cvReleaseImage(&showImg);
		*/
		mostskn_region=cvCreateImage(cvGetSize(skin_mask),IPL_DEPTH_8U,1);
		component_area = get_mostskn_component(mostskn_region,skin_mask,labelarray);
		cal_ellipse_features(mostskn_region,&tEllipse);

		perimeter = get_perimeter(mostskn_region);
		cvReleaseImage(&mostskn_region);

		sec_area = get_mostskn_component(NULL,skin_mask,labelarray);
		cvReleaseImage(&labelarray);

		features[0] = number_of_regions;
		features[1] = log(global_skin_area) - log(img_area);
		if (features[0] > 0)
		{
			features[2] = log(component_area) - log(img_area);
			features[3] = 1 - 4*3.14159*component_area / (perimeter * perimeter);
			if (tEllipse.bnoellipse == 0)
			{
				features[4] = tEllipse.fang;
				features[5] = tEllipse.fratl;	
			}
			if (features[0] > 1)
				features[6] = log(sec_area) - log(component_area);
		}
		/*
		printf("%f,%f,%f,",features[0],features[1],features[2]);
		printf("%f,%f,%f,",features[3],features[4],features[5]);
		printf("%f,",features[6]);
		*/
		
	}
	else
	{
		// for face_features
		labelarray=cvCreateImage(cvGetSize(skin_mask),IPL_DEPTH_16U,1);
		get_labelarray_of_components(binimg,labelarray);
		number_of_regions=get_number_of_components(labelarray,tFace->face_size);

		mostskn_region=cvCreateImage(cvGetSize(skin_mask),IPL_DEPTH_8U,1);
		component_area = get_mostskn_component(mostskn_region,skin_mask,labelarray);
		cal_ellipse_features(mostskn_region,&tEllipse);

		perimeter = get_perimeter(mostskn_region);
		cvReleaseImage(&mostskn_region);

		cvReleaseImage(&labelarray);

		for (i = 0 ;i < 9;i ++)
			features[i] = 0.0;
		features[0] = number_of_regions;
		features[1] = tFace->nfaces;
		features[2] = tFace->face_size * 2.0 / (skin_mask->width + skin_mask->height);
		features[3] = log(tFace->face_skin_area) - log(global_skin_area);
		dx = dy = 0.0;
		if(number_of_regions > 0 && tEllipse.bnoellipse == 0)
		{
			features[4] = log(component_area) - log((double)tFace->face_size * tFace->face_size);
			if (perimeter > 0.0)
				features[5] = 1 - 4*3.14159*component_area / (perimeter * perimeter);
			dx = tEllipse.r_ - tFace->face_y;
			dy = tEllipse.c_ - tFace->face_x;

			features[6] = sqrt(dx * dx + dy * dy) / tFace->face_size;
			if (tEllipse.c_ != tFace->face_x)
				features[7] = dy / dx;
			else
				features[7] = 100.0;
		}
		//printf("%f,%f,%f,",features[0],features[1],features[2]);
		//printf("%f,%f,%f,",features[3],features[4],features[5]);
		//printf("%f,%f,0,%f,%f,%d\n",features[6],features[7],dx,dy,tFace->face_size);
	}
	cvReleaseImage(&binimg);
}
