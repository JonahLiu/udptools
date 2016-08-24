#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#ifdef __VXWORKS__
#include <sockLib.h>
#endif

static struct sockaddr_in sa;
static int sa_len;
static unsigned int idx=0;
static unsigned int total=0;
static unsigned int lost=0;
static double lastTime=0;
static double lastReport=0;
static char *buffer;

void usage()
{
	fprintf(stderr,
		"Usage: udpcheck [options] <port>\n"
		"Arguments: \n"
		"    <port> SHORT  port to listen\n"
		"Options:\n"
		"    -b SIZE       Data package size\n"        
		"    -a ADDRESS    IP address. (Default: Any).\n"
		"    -h            Print this message.\n"
		);
}

size_t RecvMsg(int fd, char *buffer, size_t size)
{
	size_t n;
	n=recvfrom(fd,buffer,size,0,
		(struct sockaddr*)&sa, &sa_len);
	if(n>0)
	{
		buffer[n]=0;
		//fprintf(stderr,buffer);
		//puts(buffer);
	}
	return n;
}

void CheckMsg(char *buffer, size_t size)
{
	unsigned int i;
	double t;
	if(sscanf(buffer,"%d:%lf",&i,&t)!=2)
		fprintf(stderr,"Wrong message format\n");
	if(i>idx+1)
	{
		fprintf(stderr,"Lost %d in %lf sec\n", i-idx-1, t-lastTime);
		lost+=i-idx-1;
	}
	total++;
	if(t-lastReport>1)
	{
		fprintf(stderr,"RX: %8d, LOST: %8d\n",total, lost);
		lastReport=t;
	}
	idx=i;
	lastTime=t;
}


int main(int argc, char **argv)
{
	int next_opt;
	int fd;
	short port=9999;
	size_t size=1024;
	char *ipstr=NULL;

	do{
		next_opt=getopt(argc,argv,"b:a:h");
		switch(next_opt){
			case 'b': size=atol(optarg); break;
			case 'a': ipstr=optarg; break;
			case 'h': usage(); return 0;
			case -1: break;
			default: fprintf(stderr, "Unknown option -%c %s\n",
						 next_opt, optarg);
				 fprintf(stderr, "Use -h for help\n");
				 return -1;
		}
	}while(next_opt != -1);

	if(argc-optind > 1)
	{
		fprintf(stderr,"Incorrect arguments\n");
		usage();
		return -1;
	}
	else if(argc-optind > 0)
	{
		if(sscanf(argv[optind],"%d",&port)!=1)
		{
			fprintf(stderr,"Incorrect port number\n");
			usage();
			return -1;
		}
	}

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(fd<0)
	{
		fprintf(stderr,"Error open socket\n");
		return -1;
	}
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

	buffer = malloc(size);

	while(1)
	{
		size_t n;
		n=RecvMsg(fd, buffer, size);
		CheckMsg(buffer, n);
	}

	return 0;
}

