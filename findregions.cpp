/*
 * Mark all the regions in the given PBM binary images.
 * 
 * Author: Huicheng Zheng @ ENIC TELECOME LILLE 1
 * Email: zheng@enic.fr
 * 04/18/2003
 */
#include"stdafx.h"
#include "findregions.h"
#include <stdio.h>
#include <assert.h>

int get_next_unused_label_number(int *label_number_status)
{
	int i,label;
	int available;
	
	available=0;
	for( i = 0; i<MAX_COMPONENTS; i++)
		if(!label_number_status[i])
		{
			available=1;
			break;
		}

	if(available)
		label=i;
	else
	{
		label=i-1;
		printf("warning: label buffer exausted, use %d instead.\n",label);
	}
	return label;
}

void merge_label(IplImage *outgray,int label_number,int merged_label_number,int row,int column)
{
	int i,j;
	int iwidth = outgray->width;

	for(i=0;i<row;i++)
		for(j=0;j<iwidth;j++)
			if(getMatValue(outgray,i,j)==merged_label_number)
				setMatValue(outgray,i,j,label_number);

	for(j=0;j<column;j++)
		if(getMatValue(outgray,row,j)==merged_label_number)
			setMatValue(outgray,row,j,label_number);
}

/*
 * detect all the connected regions in a given binary image and get some features
 * concerning regions.
 * inbin: input binary image, in which "1" means "black"(background) and
 *        "0" means "white"(foreground)
 * labelarray: the output label array, with labels for all the regions
 */
void get_labelarray_of_components(IplImage *inbin,IplImage *labelarray)
{
	int label_number_status[MAX_COMPONENTS];//record whether the label number is used or not;
	int label_number, label_temp1, label_temp2, merged_label_number;
	int up_connected, left_connected;
	int iheight,iwidth;
	int i,j;

	iheight = inbin->height;
	iwidth = inbin->width;
	//initialization
	//label "0" will be the black areas
	label_number_status[0] = 1;
	for(i=1;i<MAX_COMPONENTS;i++)
		label_number_status[i] = 0;
	
	for(i=0;i<iheight;i++)
		for(j=0;j<iwidth;j++)
			//((unsigned short *)(labelarray->imageData + i * labelarray->widthStep))[j] = 0;
			setMatValue(labelarray,i,j,0);

	//label the first row
	for(j=0;j<iwidth;j++)
	{
		if(getPixelValue(inbin,0,j) == 1)
			continue;
	
		if(j==0)
		{
			label_number=get_next_unused_label_number(label_number_status);
			setMatValue(labelarray,0,j,label_number);
			label_number_status[label_number]=1;
		}
		else if(getPixelValue(inbin,0,j-1)==1)
		{
			label_number=get_next_unused_label_number(label_number_status);
			setMatValue(labelarray,0,j,label_number);
			label_number_status[label_number]=1;
		}
		else
			setMatValue(labelarray,0,j,getMatValue(labelarray,0,j-1));
	}

	//for the other rows
	for(i=1;i<iheight;i++)
	{
		//for the first column
		if(getPixelValue(inbin,i,0)==0)
		{
			if(getPixelValue(inbin,i-1,0)==0)
				setMatValue(labelarray,i,0,getMatValue(labelarray,i-1,0));
			else
			{
				label_number=get_next_unused_label_number(label_number_status);
				setMatValue(labelarray,i,0,label_number);
				label_number_status[label_number]=1;
			}
		}

		//for the other columns
		for(j=1;j<iwidth;j++)
		{
			if(getPixelValue(inbin,i,j)==1)
				continue;

			up_connected=0;
			left_connected=0;

			label_temp1=getMatValue(labelarray,i-1,j);
			label_temp2=getMatValue(labelarray,i,j-1);

			if(getPixelValue(inbin,i-1,j)==0)//up connected
				up_connected=1;

			if(getPixelValue(inbin,i,j-1)==0)//left connected
				left_connected=1;

			if(up_connected && left_connected)
			{
				if(label_temp1 != label_temp2)
				{
					label_number=(label_temp1<label_temp2)?label_temp1:label_temp2;
					merged_label_number=(label_temp1<label_temp2)?
						label_temp2:label_temp1;

					setMatValue(labelarray,i,j,label_number);
					merge_label(labelarray,label_number,merged_label_number,i,j);

					label_number_status[merged_label_number]=0;
				}
				else
					setMatValue(labelarray,i,j,label_temp1);
			}
			else if(up_connected)//up_connected && !left_connected
				setMatValue(labelarray,i,j,label_temp1);
			else if(left_connected)//!up_connected && left_connected
				setMatValue(labelarray,i,j,label_temp2);
			else//!up_connected && !left_connected
			{
				label_number=get_next_unused_label_number(label_number_status);
				setMatValue(labelarray,i,j,label_number);
				label_number_status[label_number]=1;
			}

		}//for each column
	}//for each row
	
}
void clear_small_components(IplImage *labelarray,IplImage *skin_mask)
{
	int i,j;
	for (i = 0;i < skin_mask->height;i ++)
		for(j = 0;j < skin_mask->width;j ++)
		{
			if (getMatValue(labelarray,i,j) > 0)
				setPixelValue(skin_mask,i,j,255);
			else
				setPixelValue(skin_mask,i,j,0);
		}
}
/*
 * get the number of components through the labelarray
 * labelarray: the input label array, with labels for all the regions
 */
int get_number_of_components(IplImage *labelarray,int face_limit)
{
	int number_of_components;
	int component_area[MAX_COMPONENTS];
	int iheight = labelarray->height;
	int iwidth = labelarray->width;
	int i,j,limit;

	if (face_limit == 0)
		limit = iheight * iwidth / 100;
	else
		limit = face_limit * face_limit;

	// get the areas of all the components except the background
	for(i=0;i<MAX_COMPONENTS;i++)
		component_area[i]=0;
	for(i=0;i<iheight;i++)
		for(j=0;j<iwidth;j++)
		{
			if(getMatValue(labelarray,i,j)==0)
				continue;
			component_area[getMatValue(labelarray,i,j)]++;
		}

	// get the number of components larger than limit
	number_of_components=0;
	for(i=1;i<MAX_COMPONENTS;i++)
		if(component_area[i]>=limit)
			number_of_components++;

	// clear small components
	for(i=0;i < iheight;i ++)
		for(j = 0;j < iwidth;j ++)
		{
			if(component_area[getMatValue(labelarray,i,j)] < limit)
				setMatValue(labelarray,i,j,0);
		}
	return number_of_components;
}

/*
 * get the largest component through a label array
 * grayimg: the input gray image, in which the result will also be put
 * labelarray: the output label array, with labels for all the regions
 */
void get_largest_component(IplImage *largest_component,IplImage *grayimg,IplImage *labelarray)
{
	int component_area[MAX_COMPONENTS];
	int iheight = labelarray->height;
	int iwidth = labelarray->width;
	int i,j,max;
	
	// get the areas of all the components except the background
	for(i=0;i<MAX_COMPONENTS;i++)
		component_area[i]=0;
	for(i=0;i<iheight;i++)
		for(j=0;j<iwidth;j++)
		{
			if(getMatValue(labelarray,i,j)==0)
				continue;
			component_area[getMatValue(labelarray,i,j)]++;
		}

	// get the label of the largest component
	max=1;
	for(i=2;i<MAX_COMPONENTS;i++)
	{
		if(component_area[i]>component_area[max])
			max=i;
	}

	for(i=0;i<iheight;i++)
		for(j=0;j<iwidth;j++)
		{
			if(getMatValue(labelarray,i,j)==max)
				setPixelValue(largest_component,i,j,getPixelValue(grayimg,i,j));
			else
				setPixelValue(largest_component,i,j,0);
		}
}

/*
 * get the brightest component through a label array
 * grayimg: the input gray image, in which the result will also be put
 * labelarray: the output label array, with labels for all the regions
 */
void get_brightest_component(IplImage *brightest_component,IplImage *grayimg,IplImage *labelarray)
{
	int component_area[MAX_COMPONENTS];
	float component_avgskn[MAX_COMPONENTS];
	int iwidth = labelarray->width;
	int iheight = labelarray->height;
	int i,j,max;
	
	// get the average skinness of all the components except the background
	for(i=0;i<MAX_COMPONENTS;i++)
	{
		component_area[i]=0;
		component_avgskn[i]=0.0;
	}
	for(i=0;i<iheight;i++)
		for(j=0;j<iwidth;j++)
		{
			if(getMatValue(labelarray,i,j)==0)
				continue;
			component_area[getMatValue(labelarray,i,j)]++;
			component_avgskn[getMatValue(labelarray,i,j)]+=getPixelValue(grayimg,i,j);
		}
	for(i=0;i<MAX_COMPONENTS;i++)
		if(component_area[i]!=0)
			component_avgskn[i]/=component_area[i];

	// get the label of the brightest component
	max=1;
	for(i=2;i<MAX_COMPONENTS;i++)
	{
		if(component_avgskn[i]>component_avgskn[max])
			max=i;
	}

	for(i=0;i<iheight;i++)
		for(j=0;j<iwidth;j++)
		{
			if(getMatValue(labelarray,i,j)==max)
				setPixelValue(brightest_component,i,j,getPixelValue(grayimg,i,j));
			else
				setPixelValue(brightest_component,i,j,0);
		}
}

/*
 * get the component with the most skinness through a label array
 * grayimg: the input gray image, in which the result will also be put
 * labelarray: the output label array, with labels for all the regions
 */
float get_mostskn_component(IplImage *mostskn_component,IplImage *grayimg,IplImage *labelarray)
{
	float component_skn[MAX_COMPONENTS];
	int iheight = labelarray->height;
	int iwidth = labelarray->width;
	int i,j,max;
	float area = 0.0;
	
	// get the average skinness of all the components except the background
	for(i=0;i<MAX_COMPONENTS;i++)
		component_skn[i]=0.0;
	for(i=0;i<iheight;i++)
		for(j=0;j<iwidth;j++)
		{
			if(getMatValue(labelarray,i,j)==0)
				continue;
			component_skn[getMatValue(labelarray,i,j)]+=getPixelValue(grayimg,i,j);
		}

	// get the label of the component with the most skinness
	max=1;
	for(i=2;i<MAX_COMPONENTS;i++)
	{
		if(component_skn[i]>component_skn[max])
			max=i;
	}

	for(i=0;i<iheight;i++)
		for(j=0;j<iwidth;j++)
		{
			if(getMatValue(labelarray,i,j)==max)
			{
				if (mostskn_component != NULL)
					setPixelValue(mostskn_component,i,j,getPixelValue(grayimg,i,j));
				area ++;
				//setPixelValue(grayimg,i,j,0); // delete mosk skin region from gray image
				setMatValue(labelarray,i,j,0);
			}else
				if (mostskn_component != NULL)
					setPixelValue(mostskn_component,i,j,0);
		}
	return area;
}

float get_perimeter(IplImage *mostskn_component)
{
	float perimeter = 0.0;
	int i,j,k;
	int center_pixel,side_pixel,status;
	int iheight = mostskn_component->height;
	int iwidth = mostskn_component->width;

	int ax[8] = {-1,0,1,1,1,0,-1,-1};
	int ay[8] = {-1,-1,-1,0,1,1,1,0};

	for(i=1;i<iheight-1;i++)
		for(j=1;j<iwidth-1;j++)
		{
			center_pixel = getPixelValue(mostskn_component,i,j);
			if (center_pixel > 0)
			{
				for(k = 0;k < 8;k ++)
				{
					side_pixel = getPixelValue(mostskn_component,i+ax[k],j+ay[k]);
					if (side_pixel > 0)
						status ++;
					else 
						status = 0;
				}
				if ( status >= 6 && status < 36)
					perimeter ++;
			}
		}
	return perimeter;	
}
