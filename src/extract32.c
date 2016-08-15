#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "getopt.h"

#include "log.h"

#define swap32(a)	((a<<24) | ((a&0xff00)<<8) | ((a&0xff0000)>>8) | (a>>24))

void usage()
{
	loginfo("Useage: extract32 <options> or extract32 file");
	loginfo("Options:");
	loginfo("	-i filename	Input file name(default=stdin)");
	loginfo("	-o filename	Output file name(default=stdout)");
	loginfo("	-d filename	Duplicate output to given file");
	loginfo("	-h hex int	frame header(default=0xA0000000)");
	loginfo("	-m hex int	frame header mask(default=0xF0000000)");
	loginfo("	-s int		frame size. 0 for variable size frame(default=0)");
	loginfo("	-B 		Input data is big-endian");
	loginfo("	-f int		start frame number to extract(default=0)");
	loginfo("	-n int		number of frame to extract(default=0)");
	loginfo("	-t 		swap data endianess");
}

void extract_fixed_size();

void extract_variable_size();

static int size=0;
static int from=0;
static int frame_num=-1;
static FILE *fin=NULL;
static FILE *fout=NULL;
static FILE *fdup=NULL;
static char *in_fn=NULL;
static char *out_fn=NULL;
static char *dup_fn=NULL;
static int endian_swap=0;
static uint32_t header=0xA0000000;
static uint32_t mask=0xF0000000;
static int big_endian=0;
static int skip=0;

int main(int argc, char **argv)
{
	int next_opt;
	do{
		next_opt=getopt(argc, argv, "h:m:s:o:f:n:i:tBd:j:");
		switch(next_opt){
			case 'i': in_fn=optarg; break;
			case 'o': out_fn=optarg;break;
			case 'd': dup_fn=optarg; break;
			case 'h': sscanf(optarg,"%x",&header); break;
			case 'm': sscanf(optarg,"%x",&mask); break;
			case 's': size=atoi(optarg); break;
			case 'f': from=atoi(optarg); break;
			case 'n': frame_num=atoi(optarg); break;
			case 't': endian_swap=1; break;
			case 'B': big_endian=1; break;
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

	if(out_fn){
		fout=fopen(out_fn,"wb");
		if(fout==NULL){
			logerr("can not open output file %s",out_fn);
			return -1;
		}
	}else{
#ifdef WINDOWS
		if(setmode(fileno(stdout),O_BINARY)==-1){
			logerr("fail to set output stream to binary mode");
			return -1;
		}
#endif
		fout=stdout;
	}

	if(dup_fn){
		fdup=fopen(dup_fn,"wb");
		if(fdup==NULL){
			logerr("can not open output file %s",dup_fn);
			return -1;
		}
	}

	if(big_endian==1){
		header = swap32(header);
		mask = swap32(mask);
	}
		

	if(size>0){
		extract_fixed_size();
	}else{
		extract_variable_size();
	}

	return 0;
}

void extract_fixed_size()
{
	uint32_t *buffer;
	int rc;
	int frame_cnt=0;
	if(fin!=stdin && from>0){
		/* move to a predicated position */
		if(fseek(fin,size*from*sizeof(uint32_t),SEEK_SET)){
			logerr("fail to seek frame %d",from);
			return;
		}
	}

	/* buffer size is exactly frame size */
	buffer = (uint32_t*)malloc(sizeof(uint32_t)*size);

	int skip_cnt=0;

	while(frame_num<0 || frame_cnt<frame_num){
		int i;
		uint32_t *ptr;
		/* read a frame size */
		rc=fread(buffer,sizeof(uint32_t),size,fin);
		if(rc!=size) break;
		/* seek header */
		for(i=0,ptr=buffer;i<size;++i,++ptr){
			if(((*ptr)&mask) == header) 
				break;
		}
		if(i<size){
			//i-=3;
			//ptr-=3;
			/* found header */
			if(i>0){
				/* discard data before header */
				memcpy(buffer,ptr,(size-i)*sizeof(uint32_t));
				/* and fill the hole */
				rc=fread(buffer+size-i,sizeof(uint32_t),i,fin);
				if(rc!=i) break;
				//logerr("discard %u data without header",i);
			}
			/* output buffer */
			if(endian_swap){
				int i;
				uint32_t *ptr=buffer;
				for(i=0;i<size;++i){
					uint32_t tmp = *ptr;
					*ptr=swap32(tmp);
					++ptr;
				}
			}

			if(skip_cnt<skip){
				++skip_cnt;
				continue;
			}else{
				skip_cnt=0;
			}

			rc=fwrite(buffer,sizeof(uint32_t),size,fout);
			if(rc!=size)
				break;
			if(fdup){
				rc=fwrite(buffer,sizeof(uint32_t),size,fdup);
				if(rc!=size)
					break;
			}
			++frame_cnt;
		}
	};

	free(buffer);
}

void extract_variable_size()
{
	uint32_t buffer;
	int rc;
	int frame_cnt=0;
	int skip=from;

	if(frame_num==0)
		return;

	/* find first frame */
	buffer=0;
	do{
		rc=fread(&buffer,sizeof(buffer),1,fin);
		if(rc!=1)
			return;
		if((buffer&mask)==header)
			--skip;
	}while(skip>=0);


	do{
		if(endian_swap){
			buffer=swap32(buffer);
		}

		rc=fwrite(&buffer,sizeof(uint32_t),1,fout);
		if(rc!=1)
			return;

		if(fdup){
			rc=fwrite(&buffer,sizeof(uint32_t),1,fdup);
			/*
			 * ignore duplicate stream
			if(rc!=sizeof(buffer))
				return;
				*/
		}

		rc=fread(&buffer,sizeof(uint32_t),1,fin);
		if(rc!=1)
			return;

		if((buffer&mask)==header) 
			++frame_cnt;
	} while(frame_num<0 || frame_cnt<frame_num);
}
