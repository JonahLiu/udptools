#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#define DEFAULT_BUFFER_SIZE  (1024)
#define DEFAULT_BLOCK_SIZE   (8*1024)

static pthread_t t1,t2;
static pthread_mutex_t bq_lock;
static pthread_cond_t bq_not_empty;
static char **bq_buf_list;
static size_t *bq_size_list;
static size_t bq_head, bq_tail;
static char *tmpbuf;
static int pending;

static FILE *of;
static int fs;

static int trim;

static int terminate;

static unsigned long long limit;

static unsigned long long read_total;
static unsigned long long write_total;
static unsigned long long drop_total;

static size_t buffer_size;
static size_t block_size;
static struct sockaddr_in addr;
static socklen_t addr_len;

static void pretty_print(FILE *fout, unsigned long long data)
{
	unsigned long long rem;
	unsigned long long div;
	unsigned long long divend;
	char str[128];
	size_t n=0;
	int preceeding_zero=0;
	for(div=1000000000000;div>1;div/=1000){
		divend=data/div;
		rem=data%div;
		if(divend){
			if(preceeding_zero)
				n+=sprintf(str+n,"%03llu",divend);
			else
				n+=sprintf(str+n,"%llu",divend);
			n+=sprintf(str+n,",");
			preceeding_zero=1;
		}
		data=rem;
	}
	if(preceeding_zero)
		n+=sprintf(str+n,"%03llu",rem);
	else
		n+=sprintf(str+n,"%llu",rem);
	fprintf(fout,"%20s",str);
}

static void usage(void)
{
	fprintf(stderr,
		"Usage: udpdump [options] <port>\n"
		"Arguments: \n"
		"    <port> SHORT  port to listen\n"
		"Options:\n"
		"    -n SIZE       Maxmium data bytes to dump. (Default: unlimited)\n"        
		"    -a ADDRESS    IP address. (Default: Any).\n"
		"    -o FILE       Output file name. (Default: stdout)\n"
		"    -d            Dumb mode, no output\n"
                "    -b SIZE       Max data package size\n"
                "    -t SIZE       Trim package header with length of size\n"
                "    -q LENGTH     Buffer queue length\n"
		"    -1            Message on single line\n"
		"    -v            Verbose output.\n"
		"    -h            Print this message.\n"
	       );
}

void* read_and_enqueue(void *arg)
{
	ssize_t rc;
	size_t next_head;
	char *swap;
	while(limit==0 || read_total < limit){
		if(terminate)
			break;
		do{
			rc=recvfrom(fs,tmpbuf,block_size,0,
				(struct sockaddr*)&addr, &addr_len);
			if(rc==-1){
				terminate=1;
				return (void*)-1;
			}
		}while(rc==0);

		pthread_mutex_lock(&bq_lock);
		next_head=(bq_head+1)%buffer_size;

		if(next_head == bq_tail){
			drop_total+=rc;
		}else{
			swap=bq_buf_list[bq_head];
			bq_buf_list[bq_head] = tmpbuf;
			tmpbuf = swap;
			bq_size_list[bq_head] = rc;
			read_total+=rc;
			bq_head=next_head;
			if(pending)
				pthread_cond_signal(&bq_not_empty);
		}
		pthread_mutex_unlock(&bq_lock);
	}
	terminate=1;
	//free(tmpbuf);
	return 0;
}

void* dequeue_and_write(void *arg)
{
	ssize_t rc;
	while(limit==0 || write_total<limit){
		pthread_mutex_lock(&bq_lock);
		while(bq_tail==bq_head){
			if(terminate){
				pthread_mutex_unlock(&bq_lock);
				return 0;
			}
			pending=1;
			pthread_cond_wait(&bq_not_empty,&bq_lock);
			pending=0;
		}
		pthread_mutex_unlock(&bq_lock);

		if(limit>0 && bq_size_list[bq_tail]>(limit-write_total)){
			bq_size_list[bq_tail]=(limit-write_total);
		}

		if(of){
			rc=fwrite(bq_buf_list[bq_tail]+trim,bq_size_list[bq_tail]-trim,1,of);
			if(rc!=1){
				terminate=1;
				return (void*)-1;
			}
		}
		write_total+=bq_size_list[bq_tail];

		pthread_mutex_lock(&bq_lock);
		bq_tail=(bq_tail+1)%buffer_size;
		pthread_mutex_unlock(&bq_lock);
	}
	terminate=1;
	return 0;
}

int main(int argc,char *argv[])
{
	int next_opt;
	char *ipstr=NULL;
	char *ofstr=NULL;
	unsigned short port;
	char verbose=0;
	char dumb=0;
	char single_line=0;
	int rc=0;

	limit = 0;
	of=stdout;
	buffer_size=DEFAULT_BUFFER_SIZE;
	block_size=DEFAULT_BLOCK_SIZE;
	addr_len=sizeof(addr);
	trim=0;
	setvbuf(stderr, (char *)NULL, _IONBF, 0);
	do{
		next_opt=getopt(argc,argv,"n:a:o:b:q:t:d1vh");
		switch(next_opt){
			case 'n': limit=atoll(optarg); break;
			case 'a': ipstr=optarg; break;
			case 'o': ofstr=optarg; break;
			case 'd': dumb=1; break;
			case 'b': block_size=atol(optarg); break;
			case 'q': buffer_size=atol(optarg); break;
			case 't': trim=atol(optarg); break;
			case 'v': verbose=1; break;
			case '1': single_line=1; break;
			case 'h': usage(); break;
			case -1: break;
			default: fprintf(stderr, "Unknown option -%c %s\n",
						 next_opt, optarg);
				 fprintf(stderr, "Use -h for help\n");
				 rc=-1;
				 goto EXIT0;
		}
	}while(next_opt != -1);

	if(argc-optind<1){
		fprintf(stderr, "Please specify port to listen\n");
		fprintf(stderr, "Use -h for help\n");
		rc=-1;
		goto EXIT0;
	}

	port = atoi(argv[optind]);

	bq_size_list=malloc(sizeof(size_t)*buffer_size);
	if(bq_size_list==NULL){
		fprintf(stderr, "Memory error\n");
		rc=-1;
		goto EXIT0;
	}
	bq_buf_list=malloc(sizeof(char*)*buffer_size);
	if(bq_buf_list==NULL){
		fprintf(stderr, "Memory error\n");
		rc=-1;
		goto EXIT0;
	}
	/*
	bq_buf_list[0]=malloc(block_size*(buffer_size+1));
	if(bq_buf_list[0]==NULL){
		fprintf(stderr, "Memory error\n");
		rc=-1;
		goto EXIT0;
	}
	int i;
	for(i=1;i<buffer_size;++i){
		bq_buf_list[i]=bq_buf_list[0]+block_size*i;
	}
	tmpbuf=bq_buf_list[0]+block_size;
	*/
	int i;
	for(i=0;i<buffer_size;++i){
		bq_buf_list[i]=malloc(block_size);
		if(bq_buf_list[i]==NULL){
			fprintf(stderr, "Memory error\n");
			rc=-1;
			goto EXIT0;
		}
	}
	tmpbuf=malloc(block_size);
	if(tmpbuf==NULL){
		fprintf(stderr, "Memory error\n");
		rc=-1;
		goto EXIT0;
	}

	pending=0;

	if(dumb){
		of=NULL;
	}else if(ofstr){
		of=fopen(ofstr,"wb");
		if(of==NULL){
			perror("Can not create file");
			rc=-1;
			goto EXIT0;
		}
		if(verbose)
			fprintf(stderr,"Writing data to %s\n", ofstr);
	}

	fs=socket(AF_INET, SOCK_DGRAM, 0);
	if(fs==-1){
		perror("Can not open socket");
		rc=-1;
		goto EXIT1;
	}

	memset(&addr,0,sizeof(addr));
	addr.sin_family=AF_INET;
	addr.sin_port=htons(port);
	addr.sin_addr.s_addr=htonl(INADDR_ANY);
	if(ipstr){
		struct hostent *he;
		he=gethostbyaddr(ipstr,strlen(ipstr),AF_INET);
		if(he==NULL){
			herror("Invalid address\n");
			rc=-1;
			goto EXIT0;
		}
		memcpy(&(addr.sin_addr.s_addr),he->h_addr_list[0],sizeof(in_addr_t));
	}
	if(verbose)
		fprintf(stderr,"Listening on port %u\n",port);

	if(bind(fs, (struct sockaddr *)&addr,sizeof(addr))){
		perror("Can not bind address");
		rc=-1;
		goto EXIT2;
	}

	{
		int nRecvBuf=64*1024*1024;
		setsockopt(fs,SOL_SOCKET,SO_RCVBUF,(const char*)&nRecvBuf,sizeof(int));
	}

	terminate=0;

	bq_head=bq_tail=0;
	pthread_mutex_init(&bq_lock,NULL);
	pthread_cond_init(&bq_not_empty,NULL);

	pthread_create(&t1, NULL, read_and_enqueue, NULL);
	pthread_create(&t2, NULL, dequeue_and_write, NULL);

	long long last_toal=0;
	if(verbose){
		fprintf(stderr,"%20s%20s%20s\r\n","Received","Speed","Dropped");
	}
	while(!terminate){
		sleep(1);
		if(verbose){
			pretty_print(stderr, write_total);
			pretty_print(stderr, write_total-last_toal);
			pretty_print(stderr, drop_total);
			if(single_line)
				fprintf(stderr,"\r");
			else
				fprintf(stderr,"\r\n");
			last_toal=write_total;
		}
	}

	if(pending)
		pthread_cond_signal(&bq_not_empty);
	pthread_join(t1,NULL);
	pthread_join(t2,NULL);

	pthread_mutex_destroy(&bq_lock);
	pthread_cond_destroy(&bq_not_empty);
EXIT2:
	close(fs);
EXIT1:
	if(of) fclose(of);
EXIT0:
	return rc;
}
