#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

int (*fill)(char* buffer, size_t size);

static void usage(void)
{
	fprintf(stderr,
		"Usage: udptest [options] <address> <port>\n"
		"Arguments: \n"
		"    <address>     Destination address\n"
		"    <port>        Destination port\n"
		"Options:\n"
		"    -b SIZE       Data package size. (Default: 1024)\n"        
		"    -a ADDRESS    Local IP address. (Default: Any).\n"
                "    -p PORT       Local port. (Default: 12345)\n"
		"    -m METHOD     Method to generate data pattern. (Default: counter32)\n"
		"    -h            Print this message.\n"
		"Currently supported method:\n"
		"    counter32\n"
	       );
}

int fill_counter32(char *buffer, size_t size)
{
	int i;
	static uint32_t next=0;
	uint32_t *dptr=(uint32_t*)buffer;
	for(i=0;i<size;i+=sizeof(uint32_t)){
		*dptr++=next++;
	}
	return 0;
}

int main(int argc,char *argv[])
{
	int next_opt;
	static struct sockaddr_in addr;
	static socklen_t addr_len;
	char *ipstr=NULL;
	char *dest_ipstr=NULL;
	unsigned short port=12345;
	unsigned short dest_port;
	size_t size=1024;
	static int fs;
	char *buffer;
	char *method="counter32";

	addr_len=sizeof(addr);

	do{
		next_opt=getopt(argc,argv,"b:a:p:h");
		switch(next_opt){
			case 'b': size=atol(optarg); break;
			case 'a': ipstr=optarg; break;
			case 'p': port=atoi(optarg); break;
			case 'm': method=optarg;break;
			case 'h': usage(); break;
			case -1: break;
			default: fprintf(stderr, "Unknown option -%c %s\n",
						 next_opt, optarg);
				 fprintf(stderr, "Use -h for help\n");
				return -1;
		}
	}while(next_opt != -1);

	if(strcmp(method,"counter32")==0){
		fill=fill_counter32;
	}else{
		fprintf(stderr,"Unsupported method\n");
		return -1;
	}

	if(argc-optind<2){
		fprintf(stderr, "Please specify destination address and port\n");
		fprintf(stderr, "Use -h for help\n");
		return -1;
	}

	dest_ipstr = argv[optind];
	dest_port = atoi(argv[optind+1]);

	buffer=malloc(size);

	fs=socket(AF_INET, SOCK_DGRAM, 0);

	memset(&addr,0,sizeof(addr));
	addr.sin_family=AF_INET;
	addr.sin_port=htons(port);
	addr.sin_addr.s_addr=htonl(INADDR_ANY);
	if(ipstr){
		struct hostent *he;
		he=gethostbyaddr(ipstr,strlen(ipstr),AF_INET);
		if(he==NULL){
			herror("Invalid address\n");
			return -1;
		}
		memcpy(&(addr.sin_addr.s_addr),he->h_addr_list[0],sizeof(in_addr_t));
	}

	bind(fs, (struct sockaddr *)&addr,sizeof(addr));

	addr.sin_port=htons(dest_port);
	if(dest_ipstr){
		struct hostent *he;
		he=gethostbyname(dest_ipstr);
		if(he==NULL){
			herror("Invalid address\n");
			return -1;
		}
		memcpy(&(addr.sin_addr.s_addr),he->h_addr_list[0],sizeof(in_addr_t));
	}

	while(1){
		fill(buffer,size);
		sendto(fs,buffer,size,0,(struct sockaddr*)&addr,addr_len);
	}

	close(fs);
	free(buffer);
	return 0;
}
