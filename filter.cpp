// filter.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include <stdio.h>
#include <cv.h>
#include <highgui.h>
#include <ml.h>
#include "findregions.h"
#include "features.h"
#include<fstream>
#include<iostream>
using namespace std;
using namespace cv;

float skin_model[32][32][32];
CvHaarClassifierCascade *cascade = NULL;
CvANN_MLP mlp;

IplImage *original_img = NULL;
IplImage *gray_img = NULL;
IplImage *canny_img = NULL;
IplImage *skin_mask = NULL;
CvSeq *faces = NULL;
CvMemStorage *storage;
//初始载入检测模型
int init()		
{
	FILE *fp;
	int b,g,r;
	float p;
	// load skin model
	//printf("load skin model\n");
	for (r = 0;r < 32;r ++)
		for(g= 0; g < 32; g ++)
			for (b = 0; b < 32; b ++)
				skin_model[r][g][b] = 0.0;
	fp = fopen("./models/skin_model.txt","r");
	while(! feof(fp))
	{	
		fscanf(fp,"%d,%d,%d,%f",&b,&g,&r,&p);
		skin_model[b][g][r] = p;
	}
	fclose(fp);
	//printf("load cascade model\n");
	// load face detection model
	cascade = (CvHaarClassifierCascade*)cvLoad("./models/haarcascade_frontalface_alt.xml",0,0,0);
	// load MLP model
	mlp.load("./models/mlp_model.txt");
	return 0;
}
//载入图片并适当缩放（可能）
int load_image(char *filepath)
{
	IplImage *tmp_img;
	float size_rate,resize_rate;
	float original_size_big,original_size_small;

	tmp_img = cvLoadImage(filepath,-1);
	if (tmp_img == NULL || tmp_img->nChannels == 1)
	{
		cvReleaseImage(&tmp_img);
		return 1;
	}

	if (tmp_img->width * tmp_img->height < 4800)
	{
		cvReleaseImage(&tmp_img);
		return 1;
	}
	if (tmp_img->width > tmp_img->height)
	{
		original_size_big = tmp_img->width;
		original_size_small = tmp_img->height;	
	}else
	{
		original_size_big = tmp_img->height;
		original_size_small = tmp_img->width;
	}
	size_rate = original_size_big / original_size_small; 

	if (size_rate > 3.0)
	{
		cvReleaseImage(&tmp_img);
		return 1;
	}
	// resize image
	float a = 320.0 / original_size_big;
	float b = 240.0 / original_size_small;
	resize_rate = a < b? a:b;
	if (resize_rate < 1.0)
	{	
		int w = tmp_img->width * resize_rate;
		int h = tmp_img->height * resize_rate;
		original_img = cvCreateImage(cvSize(w,h),IPL_DEPTH_8U,3);
		cvResize(tmp_img,original_img,CV_INTER_AREA);
	}else
		original_img = cvCloneImage(tmp_img);
	cvReleaseImage(&tmp_img);


	return 0;
}
unsigned char panel[256][256][32];	
//maybe too large
//统计一个图像的所有颜色，如果超过1000，则返回
int color_number_24bit(IplImage *rgb_img)
{
	int i,j,k;
	int width,height;
	unsigned sum_of_color = 0;
	unsigned char r,g,b,mask;
	unsigned char init_mask = 1;
	unsigned char sub_mask = 7;

	memset(panel,0,sizeof(panel));
	width = rgb_img->width;
	height = rgb_img->height;

	for(i = 0;i < width;i ++)
		for(j = 0;j < height;j ++)
		{
			b = ((unsigned char*)(rgb_img->imageData + j*rgb_img->widthStep))[i*rgb_img->nChannels + 0];
			g = ((unsigned char*)(rgb_img->imageData + j*rgb_img->widthStep))[i*rgb_img->nChannels + 1];
			r = ((unsigned char*)(rgb_img->imageData + j*rgb_img->widthStep))[i*rgb_img->nChannels + 2];

			k = r >> 3;
			mask = init_mask << (r & sub_mask);
			if ((panel[b][g][k] & mask) == 0)
			{
				panel[b][g][k] = panel[b][g][k] | mask;
				sum_of_color ++;
				if (sum_of_color > 1000)
					return 0;
			}
		}
		return 1;
}
//肤色检测
float skin_detection(IplImage *img, IplImage *mask)
{
	int i,j,r,g,b,p;
	float score = 0.0;

	for(i = 0;i < img->width;i ++)
		for(j = 0;j < img->height;j ++)
		{
			b = ((unsigned char *)(img->imageData + j*img->widthStep))[i * img->nChannels + 0];
			g = ((unsigned char *)(img->imageData + j*img->widthStep))[i * img->nChannels + 1];
			r = ((unsigned char *)(img->imageData + j*img->widthStep))[i * img->nChannels + 2];
			b = b / 8;
			g = g / 8;
			r = r / 8;
			p = int(skin_model[b][g][r]);
			if (p > 5) score ++;
			((unsigned char *)(mask->imageData + j*mask->widthStep))[i] = p;
		}
		score = score / (img->width * img->height);
		return score;
}
//边缘检测
float edge_rate()
{
	int i,j;
	float edge_num = 0.0;

	gray_img = cvCreateImage(cvGetSize(original_img),IPL_DEPTH_8U,1);
	cvCvtColor(original_img,gray_img,CV_BGR2GRAY);
	canny_img = cvCreateImage(cvGetSize(gray_img),IPL_DEPTH_8U,1);
	cvCanny(gray_img,canny_img,50,200,3);

	for(i = 0;i < canny_img->width;i ++)
		for(j = 0;j < canny_img->height;j ++)
		{
			if (((unsigned char *)(canny_img->imageData + j * canny_img->widthStep))[i] > 0)
				edge_num ++;
		}

		edge_num = edge_num / (canny_img->width * canny_img->height);
		return edge_num;
}
//人脸检测
float face_detection(struct TFACE *tFace)
{
	int nfaces,cnt,idx;
	float max_face_area = 0.0;
	float skin_sum,skin_rate;
	int ix,iy;
	int faceA_area,faceB_area;
	int overlap_x1,overlap_x2,overlap_y1,overlap_y2;

	tFace->nfaces = 0;
	tFace->face_skin_area = 0.0;
	storage = cvCreateMemStorage(0);
	cvClearMemStorage(storage);
	faces = cvHaarDetectObjects(gray_img,cascade,storage,1.1,2,0,cvSize(30,30));

	if (faces != NULL)
	{
		nfaces = faces->total;
		int i = 0;
		while (i < nfaces)
		{
			CvRect *r = (CvRect *)cvGetSeqElem(faces,i);
			int rxp = r->x + r->width;
			int ryp = r->y + r->height;
			faceA_area = r->width * r->height;

			int j = i + 1;
			while(j < nfaces)
			{
				CvRect *rt = (CvRect *)cvGetSeqElem(faces,j);
				int rtxp = rt->x + rt->width;
				int rtyp = rt->y + rt->height;
				faceB_area = rt->width * rt->height;

				overlap_x1 = rt->x > r->x ? rt->x : r->x;
				overlap_x2 = rtxp < rxp ? rtxp : rxp;
				int w = overlap_x2 - overlap_x1;

				overlap_y1 = rt->y > r->y ? rt->y : r->y;
				overlap_y2 = rtyp < ryp ? rtyp : ryp;
				int h = overlap_y2 - overlap_y1;

				if ((w > 0 && h > 0) && ((w * h * 3 > faceA_area * 2) || ( w * h * 3 > faceB_area * 2)))
				{// exist overlap
					if (faceA_area > faceB_area)
					{
						cvSeqRemove(faces,i);
						nfaces --;
						break;
					}else
					{
						cvSeqRemove(faces,j);
						nfaces --;
					}
				}else
					j ++;
			}
			if (j == nfaces)
				i ++;
		}
	}
	if (faces != NULL)
	{
		nfaces = faces->total;
		int *facesIdxToDelete = new int[nfaces];
		cnt = 0;
		for(int i = 0;i < nfaces;i ++)
		{
			CvRect *r = (CvRect *)cvGetSeqElem(faces,i);
			skin_sum = 0.0;
			for(ix = r->x;ix < r->x + r->width;ix ++)
				for(iy= r->y;iy < r->y + r->height;iy ++)
				{
					if (getPixelValue(skin_mask,iy,ix) > 50)
						skin_sum ++;
				}

				skin_rate = skin_sum /(r->width * r->height);
				if (skin_rate < 0.3)
				{
					facesIdxToDelete[cnt] = i;
					cnt ++;
				}else
				{
					if (r->width * r->height > max_face_area)
					{
						max_face_area = r->width * r->height;
						tFace->face_x = r->x;
						tFace->face_y = r->y;
						tFace->face_size = (r->width + r->height) / 2;
					}
					tFace->face_skin_area = tFace->face_skin_area + skin_sum;
					for(ix = r->x;ix < r->x + r->width;ix ++)
						for(iy= r->y;iy < r->y + r->height;iy ++)
							setPixelValue(skin_mask,iy,ix,0);
				}
		}
		for(int i = cnt - 1;i >= 0;i --)
			cvSeqRemove(faces,facesIdxToDelete[i]);
		delete [] facesIdxToDelete;
	}
	tFace->nfaces = faces->total;
	max_face_area = max_face_area / (gray_img->width * gray_img->height);
	return max_face_area;
}
//行人检测
int people_detection(IplImage *img)
{
	cv::Mat Img(img, 0);
	HOGDescriptor hog;
	hog.setSVMDetector(HOGDescriptor::getDefaultPeopleDetector());
	fflush(stdout);
	vector<Rect> found, found_filtered;
	hog.detectMultiScale(img, found, 0, Size(8,8), Size(32,32), 1.05, 2);
	int i,j;
	for(i = 0; i < found.size(); i++ )
	{
		Rect r = found[i];
		for(j = 0; j < found.size(); j++ )
			if( j != i && (r & found[j]) == r)
				break;
		if( j == found.size() )
			found_filtered.push_back(r);
	}
	imwrite("result.jpg",img);
	return found_filtered.size();
}
//不良内容检测
int porn_detection(float features[],int n_features)
{
	int score;
	CvMat *mlp_response=0;
	CvMat *vector;
	vector = cvCreateMat(1,n_features,CV_32F);
	//对特征向量进行预测
	for (int i = 0;i < n_features;i ++)
		vector->data.fl[i] = features[i];
	mlp_response = cvCreateMat(1,1,CV_32F);
	mlp.predict(vector,mlp_response);
	if (mlp_response->data.fl[0] > 1.0)
		score = 100;
	else if (mlp_response->data.fl[0] < 0.0)
		score = 0;
	else
		score = mlp_response->data.fl[0] * 100;
	return score;
}
void releaseALL()
{
	cvReleaseImage(&skin_mask);
	cvReleaseImage(&gray_img);
	cvReleaseImage(&original_img);

}
int main(int argc,char *argv[])
{
	char image_file_name[255];
	FILE *taskFileHandle = NULL;
	FILE *fp;
	float skin_score,face_rate;
	float features[9];
	int score,id;
	struct TFACE tFace;
	char err_img_name[255];
	char err_img_mask[255];

	init();
	taskFileHandle = fopen(argv[1],"r");
	//fp = fopen("/home/mag/2_log.txt","w");
	image_file_name[0] = '\0';
	ofstream outfile("score.txt",ios::out);

	while (! feof(taskFileHandle))
	{
		image_file_name[0] = '\0';
		fscanf(taskFileHandle,"%d,%s",&id,image_file_name);
		if (image_file_name[0] != '\0')
		{
			if (load_image(image_file_name) == 1)
			{
				releaseALL();
				printf("%d,0\n",id);
				continue;
			}

			skin_mask = cvCreateImage(cvGetSize(original_img),IPL_DEPTH_8U,1);
			skin_score = skin_detection(original_img,skin_mask);
			if (skin_score < 0.01) 							//如果皮肤太少，可以过滤掉
			{
				releaseALL();
				printf("%d,0\n",id);
				continue;
			}
			
			if (color_number_24bit(original_img) == 1)		//如果一个图像的颜色过于丰富，则可以适当过滤掉
			{
				releaseALL();
				printf("%d,0\n",id);
				continue;
			}

			float erate = edge_rate();
			if (erate > 0.4 || erate < 0.001)				//边缘所占的数量太过极端，可以过滤掉
			{
				releaseALL();
				printf("%d,0\n",id);
				continue;
			}
			
			double t = (double)getTickCount();
			face_rate = face_detection(&tFace);
			t = (double)getTickCount() - t;
			printf("tdetection time = %gms\n", t*1000./cv::getTickFrequency());
			if (face_rate > 0.5) 							//人脸占的面积太大，可以过滤掉
			{
				releaseALL();
				printf("%d,0\n",id);
				continue;
			}

			/*int people_num=people_detection(original_img);
			if(people_num>1)
			{
				releaseALL();
				printf("%d\n",people_num);
				continue;
			}
			*/

			if (tFace.nfaces > 0)
			{
				//如果face的个数大于0,特征向量的维数为8维
				//printf("%d,%d,%d,%d,%f\n",tFace.nfaces,tFace.face_x,tFace.face_y,tFace.face_size,tFace.face_skin_area);
				//printf("%d,%s\n",id,image_file_name);
				cal_features(skin_mask,gray_img,&tFace,features,id);
				score = porn_detection(features,8);
			}
			else 
			{	//计算特征和分数
				//特征向量的维数为7维
				cal_features(skin_mask,gray_img,&tFace,features,id);		
				score = porn_detection(features,7);		
			}
			//特征维数是否是这样取？
			printf("%d,%s,%d\n",id,image_file_name,score);
			outfile<<image_file_name<<","<<score;
			/*for( int i=0; i<9; i++)
			{
				outfile<<features[i]<<",";
			}*/
			outfile<<endl;
			releaseALL();
		}
	}
	fclose(taskFileHandle);
}