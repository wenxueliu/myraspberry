#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include "../include/sdf.h"
#include "../include/video_capture.h"

const char *filename = "test.yuv\0";
FILE *fp = NULL;

static int maim_loop(struct camera *cam)
{
	int count = 1;
	fd_set fds;
	struct timeval tm;
    int ret_code = 0;
	int frame_len = 0;
    uint8 *pic = NULL;
    pic = (uint8 *)malloc(cam->width * cam->height * 2);
    if (NULL == pic)
    {
        printf("can't get enough memory pic\n");
        return -1;
    }
	for(;;)
	{
        frame_len = 0;
        memset(pic, 0, cam->width * cam->height * 2);
		printf("this is the %d th frame\n", count);
		if ( count++ > 50 ) {
			printf("success exit\n");
            free(pic);
			break;
		}

		FD_ZERO(&fds);
		FD_SET(cam->camera_fd, &fds);

		tm.tv_sec = 2;
		tm.tv_usec = 0;

		int ret = select(cam->camera_fd + 1, &fds, NULL, NULL, &tm);

		switch(ret)
		{
		case -1:
            if (EINTR == errno){
                continue;
            } else {
                ret_code = -1;
            }
			break;
		case 0:
			printf("time out \n");
            ret_code = -1;
            break;
		default:
			frame_len = read_frame_from_camera(cam, pic);
            printf("read_frame_from_camera : %d", frame_len);
            if(frame_len == -1){
                ret_code = -1;
            } else {
                fwrite(pic, frame_len, 1, fp);
            }
		}
        if (ret_code == -1){
            printf("select error\n");
            free(pic);
            break;
        }
    }
}

static void usage(FILE *fp, int argc, char **argv) {
    fprintf(fp, "Usage: %s [options]\n\n"
            "Options:\n"
            "-d | --device name   Video device name [/dev/video]\n"
            "-h | --help          Print this message\n"
            "-m | --mmap          Use memory mapped buffers\n"
            "-r | --read          Use read() calls\n"
            "-u | --userp         Use application allocated buffers\n"
            "", argv[0]);
}


static const char short_options[] = "d:hmru";

static const struct option long_options[] = {
    { "device", required_argument,NULL, 'd' },
    { "help", no_argument, NULL, 'h' },
    { "mmap", no_argument, NULL, 'm' },
    { "read", no_argument, NULL, 'r' },
    { "userp", no_argument,NULL, 'u' },
    { 0, 0, 0, 0 }
};

int main(int argc, char *argv[])
{
	char *camera_name = "/dev/video0";
    for (;;) {
        int index;
        int c;
        c = getopt_long(argc, argv, short_options, long_options, &index);
        if (-1 == c)
            break;
        switch (c) {
        case 0: /* getopt_long() flag */
            break;
        case 'd':
            camera_name = optarg;
            break;
        case 'h':
            usage(stdout, argc, argv);
            exit(EXIT_SUCCESS);
        /*
        case 'm':
            io = IO_METHOD_MMAP;
            break;
        case 'r':
            io = IO_METHOD_READ;
            break;
        case 'u':
            io = IO_METHOD_USERPTR;
            break;
        */
        default:
            usage(stderr, argc, argv);
            exit(EXIT_FAILURE);
        }
    }


	struct camera ca = {0};

	ca.width = 640;
	ca.height = 480;
	ca.device_name = camera_name;

	printf("Now initialize the video for linux \n");
	int ret = v4l2_init(&ca);
    if (ret == -1) return 1;
	fp = fopen(filename, "w+");
    if (fp == NULL) {
        printf("open %s error : %s", filename, strerror(errno));
        return -1;
    }
    maim_loop(&ca);
	v4l2_close(&ca);
}
