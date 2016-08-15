#include <opencv2/opencv.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>

#include <getopt.h>

#include "log.h"

using namespace cv;

void usage()
{
	loginfo("Useage: view32 <options> or view32 file");
	loginfo("Options:");
	loginfo("	-i filename	Input file name(default=stdin)");
	loginfo("	-h int		frame height(default=512)");
	loginfo("	-w int		frame width(default=512)");
	//loginfo("	-B 		Input data is big-endian");
	loginfo("	-1 		Display in original size");
}

static int height=512;
static int width=512;
static FILE *fin=NULL;
static char *in_fn=NULL;
static int big_endian=0;
static int allow_resize=1;
static int draw_info=0;
static int skip=0;

int main(int argc, char** argv)
{
	int next_opt;
	int rc;
	do{
		next_opt=getopt(argc, argv, "i:h:w:B1vj:");
		switch(next_opt){
			case 'i': in_fn=optarg; break;
			case 'h': height=atoi(optarg); break;
			case 'w': width=atoi(optarg); break;
			case 'B': big_endian=1; break;
			case '1': allow_resize=0;break;
			case 'v': draw_info=1;break;
			case 'j': skip=atoi(optarg); break;
			case -1: break;
			default:
				  logerr("unknown option - %c %s", next_opt,optarg);
				  usage();
				  return -1;
		}
	}while(next_opt!=-1);

	if(optind<argc)
		in_fn = argv[optind];

	if(in_fn){
		fin=fopen(in_fn,"rb");
		if(fin==NULL){
			logerr("can not open input file %s",in_fn);
			return -1;
		}
	}else{
#ifdef WINDOWS
		if(setmode(fileno(stdin),O_BINARY)==-1){
			logerr("fail to set input stream to binary mode");
			return -1;
		}
#endif
		fin=stdin;
	}

	char strFrame[128];

	uint32_t* data=new uint32_t[height*width];
	Mat image(height,width,CV_8UC4,data);
	namedWindow("Display", allow_resize?0:WINDOW_AUTOSIZE);
	imshow("Display",image);
	waitKey(1);
	int key=0;
	unsigned int i=0;
	int size=height*width;
	int skip_cnt=0;
	do{
		rc=fread(data,sizeof(uint32_t),size,fin);
		if(rc!=size){
			waitKey();
			break;
		}

		if(skip_cnt<skip){
			++skip_cnt;
			continue;
		}else{
			skip_cnt=0;
		}

		if(draw_info){
			sprintf(strFrame,"#%08d", i++);
			putText(image,strFrame,cvPoint(0,20),2,1,CV_RGB(0,256,0));
		}
		imshow("Display", image);
		key=waitKey(1);
	}while(key!=27);
	return 0;
}
