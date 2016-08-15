#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include "log.h"

void usage()
{
	loginfo("Useage: check <options> method or check <options> method file");
	loginfo("Options:");
	loginfo("	-i filename	Input file name(default=stdin)");
	loginfo("	-b size         Block size in bytes(default=4096)");
	loginfo("Method: specify a data check algorithm");
	loginfo("Currently supported methods are:");
	loginfo("       counter32");
}

static FILE *fin=NULL;
static char *in_fn=NULL;
static size_t bsize=4096;
static char *method=NULL;

int (*check)(char* data, size_t size);

int check_counter32(char* data, size_t size)
{
	int i;
	static uint32_t expect=0;
	uint32_t *got=(uint32_t*)data;
	for(i=0;i<size;i+=sizeof(uint32_t)){
		if(*got==expect){
			expect++;
		}else{
			fprintf(stderr,"Expect %08x, Got %08x\n",expect, *got);
			expect=*got+1;
		}
		got++;
	}
	return 0;
}

int main(int argc, char** argv)
{
	int next_opt;
	int rc;
	char *buffer;
	do{
		next_opt=getopt(argc, argv, "i:b:");
		switch(next_opt){
			case 'i': in_fn=optarg; break;
			case 'b': bsize=atoi(optarg); break;
			case -1: break;
			default:
				  logerr("unknown option - %c %s", next_opt,optarg);
				  usage();
				  return -1;
		}
	}while(next_opt!=-1);

	if(optind<argc){
		method=argv[optind];
		if(strcmp(method,"counter32")==0){
			check=check_counter32;
		}else{
			logerr("Unsupported method - %s", method);
			return -1;
		}
	}else{
		logerr("Please speficy check method");
		usage();
		return -1;
	}

	if(optind+1<argc)
		in_fn = argv[optind+1];

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

	buffer=(char*)malloc(bsize);

	while(1)
	{
		rc=fread(buffer,1,bsize,fin);
		if(rc!=bsize){
			break;
		}
		check(buffer,rc);
	}
	return 0;
}
