
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>

#include "bcm_host.h"
#include "ilclient.h"

#define NUMFRAMES 100
#define WIDTH     640
#define PITCH     ((WIDTH+31)&~31)
#define HEIGHT    ((WIDTH)*12/16)
#define HEIGHT16  ((HEIGHT+15)&~15)
#define SIZE      ((WIDTH * HEIGHT16 * 3)/2)

int openMAX_encode_fd(int , FILE*);
//static int openMax_encode_init(COMPONENT_T *video_encode);
//int openMAX_encode_fd(int yuv_fd, FILE *h264_fp);
//int openMAX_encode_file(char *yuv_file, char *h264_file);
//static int video_encode_test(int fd, char *outputfilename);
