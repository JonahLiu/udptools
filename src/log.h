#ifndef __LOG_H__
#define __LOG_H__

#if defined(__GNUC__)
#define logerr(fmt, ...)	fprintf(stderr,"ERROR: "fmt"\n", ##__VA_ARGS__)
#define loginfo(fmt, ...)	fprintf(stderr,fmt"\n", ##__VA_ARGS__)

#else
#define logerr(fmt, ...)	fprintf(stderr,"ERROR: "fmt"\n", __VA_ARGS__)
#define loginfo(fmt, ...)	fprintf(stderr,fmt"\n", __VA_ARGS__)
#endif

#endif /* __LOG_H__ */
