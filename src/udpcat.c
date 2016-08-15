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
		"    -m msg        Send a char string instead of reading from stdin\n"
		"    -x hex        Send a hex string instead of reading from stdin\n"
		"    -i file       Read data from file instead of reading from stdin\n"
		"    -h            Print this message.\n"
	       );
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
	char *strmsg=NULL;
	char *hexmsg=NULL;
	char *infn=NULL;
	FILE *fin=stdin;

	addr_len=sizeof(addr);

	do{
		next_opt=getopt(argc,argv,"b:a:p:m:x:i:h");
		switch(next_opt){
			case 'b': size=atol(optarg); break;
			case 'a': ipstr=optarg; break;
			case 'p': port=atoi(optarg); break;
			case 'm': strmsg=optarg;break;
			case 'x': hexmsg=optarg; break;
			case 'i': infn=optarg; break;
			case 'h': usage(); break;
			case -1: break;
			default: fprintf(stderr, "Unknown option -%c %s\n",
						 next_opt, optarg);
				 fprintf(stderr, "Use -h for help\n");
				return -1;
		}
	}while(next_opt != -1);

	if(argc-optind<2){
		fprintf(stderr, "Please specify destination address and port\n");
		fprintf(stderr, "Use -h for help\n");
		return -1;
	}

	dest_ipstr = argv[optind];
	dest_port = atoi(argv[optind+1]);

	if(infn){
		fin=fopen(infn,"rb");
		if(fin==NULL){
			fprintf(stderr,"Can not open file %s", infn);
			return -1;
		}
	}


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

	if(strmsg){
		size=strlen(strmsg)+1;
		fprintf(stderr,"%s",strmsg);
		sendto(fs,strmsg,size,0,(struct sockaddr*)&addr,addr_len);
	}else if(hexmsg){
		int i;
		char tmp[3];
		size=strlen(hexmsg)/2;
		buffer=malloc(size);
		for(i=0;i<size;i++){
			tmp[0]=hexmsg[i*2];
			tmp[1]=hexmsg[i*2+1];
			tmp[2]=0;
			buffer[i]=strtol(tmp,NULL,16);
			fprintf(stderr,"%02x",buffer[i]);
		}
		sendto(fs,buffer,size,0,(struct sockaddr*)&addr,addr_len);
		free(buffer);
	}else{
		int i;
		buffer=malloc(size);
		do{
			i=fread(buffer,1,size,fin);
			if(i>0)
				sendto(fs,buffer,i,0,(struct sockaddr*)&addr,addr_len);
		}while(i>0);
		free(buffer);
	}

	close(fs);
	return 0;
}
