#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>

#ifdef __VXWORKS__
#include <sockLib.h>
#endif

static struct sockaddr_in sa;
static int sa_len;

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

void SendMsg(int fd, int i, double t)
{
	size_t n;
	static char buffer[128];
	n=sprintf(buffer,"%d:%lf\n",i,t);
	sendto(fd, buffer, n, 0, (struct sockaddr*)&sa, sa_len);
	//puts(buffer);
}

int main(int argc, char **argv)
{
	int fd;
	short dst_port=9999;
	char *dst_ip="192.168.0.255";
	unsigned long idx=1;
	double last, now, span;

	if(argc > 3)
	{
		fprintf(stderr,"Incorrect arguments\n");
		usage();
		return -1;
	}
	else if(argc > 2)
	{
		if(sscanf(argv[2],"%d",&dst_port)!=1)
		{
			fprintf(stderr,"Incorrect port number\n");
			usage();
			return -1;
		}
		dst_ip=argv[1];
	}
	else if(argc > 1)
	{
		dst_ip=argv[1];
	}

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(fd<0)
		return -1;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons(0);
	sa_len = sizeof(sa);

	if(bind(fd, (struct sockaddr *)&sa, sa_len) < 0)
		return -1;

	sa.sin_addr.s_addr = inet_addr(dst_ip);
	sa.sin_port = htons(dst_port);

	last=getTime();
	while(1)
	{
		now=getTime();
		span=now-last;
		if(span>0.001)
		{
			SendMsg(fd, idx++,now);
			last=now;
		}
	}
	return 0;
}
