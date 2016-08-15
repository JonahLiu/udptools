#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <string.h>

#ifdef __VXWORKS__
#include <sockLib.h>
#endif

static struct sockaddr_in sa;
static int sa_len;
static char *buffer;

void usage()
{
	fprintf(stderr,"Usage: udpsend <ip address> <port>\n");
}

#ifdef USE_GETTIMEOFDAY
double getTime()
{
	struct timeval tv;
	double second;
	gettimeofday(&tv,NULL);
	second = tv.tv_sec + 
		tv.tv_usec/1000000.0;
	return second;
}

#else

double getTime()
{
	double second;
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	second = tp.tv_sec + tp.tv_nsec/1000000000.0;
	return second;
}

#endif /* USE_GETTIMEOFDAY */

int main(int argc, char **argv)
{
	int next_opt;
	int fd;
	short dst_port=9999;
	char *dst_ip="192.168.0.255";
	unsigned short port=0;
	unsigned long idx=1;
	double last, now, span;
	size_t size=1024;
	char *ipstr=NULL;
	double interval=0.001;

	do{
		next_opt=getopt(argc,argv,"b:a:p:i:h");
		switch(next_opt){
			case 'b': size=atol(optarg); break;
			case 'a': ipstr=optarg; break;
			case 'p': port=atoi(optarg); break;
			case 'i': interval=atof(optarg); break;
			case 'h': usage(); break;
			case -1: break;
			default: fprintf(stderr, "Unknown option -%c %s\n",
						 next_opt, optarg);
				 fprintf(stderr, "Use -h for help\n");
				 return -1;
		}
	}while(next_opt != -1);

	if(argc-optind > 2)
	{
		fprintf(stderr,"Incorrect arguments\n");
		usage();
		return -1;
	}
	else if(argc-optind > 1)
	{
		if(sscanf(argv[optind+1],"%d",&dst_port)!=1)
		{
			fprintf(stderr,"Incorrect port number\n");
			usage();
			return -1;
		}
		dst_ip=argv[optind];
	}
	else if(argc-optind > 0)
	{
		dst_ip=argv[optind];
	}

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(fd<0)
		return -1;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons(port);
	if(ipstr){
		struct hostent *he;
		he=gethostbyaddr(ipstr,strlen(ipstr),AF_INET);
		if(he==NULL){
			herror("Invalid address\n");
			return -1;
		}
		memcpy(&(sa.sin_addr.s_addr),he->h_addr_list[0],sizeof(in_addr_t));
	}
	sa_len = sizeof(sa);

	if(bind(fd, (struct sockaddr *)&sa, sa_len) < 0)
		return -1;

	sa.sin_addr.s_addr = inet_addr(dst_ip);
	sa.sin_port = htons(dst_port);

	if(size < 32)
		size = 32;

	buffer = malloc(size);

	last=getTime();
	while(1)
	{
		now=getTime();
		span=now-last;
		if(span > interval)
		{
			sprintf(buffer,"%08d:%022.6lf\n",idx++,now);
			sendto(fd, buffer, size, 0, (struct sockaddr*)&sa, sa_len);
			last=now;
		}
	}
	return 0;
}
