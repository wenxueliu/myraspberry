/*
Copyright (c) 2012, Broadcom Europe Ltd
Copyright (c) 2012, Kalle Vahlman <zuh@iki>
                    Tuomas Kulve <tuomas@kulve.fi>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Video encode demo using OpenMAX IL though the ilcient helper library

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

static void
print_def(OMX_PARAM_PORTDEFINITIONTYPE def)
{
   printf("Port %u: %s %u/%u %u %u %s,%s,%s %ux%u %ux%u @%u %u\n",
	  def.nPortIndex,
	  def.eDir == OMX_DirInput ? "in" : "out",
	  def.nBufferCountActual,
	  def.nBufferCountMin,
	  def.nBufferSize,
	  def.nBufferAlignment,
	  def.bEnabled ? "enabled" : "disabled",
	  def.bPopulated ? "populated" : "not pop.",
	  def.bBuffersContiguous ? "contig." : "not cont.",
	  def.format.video.nFrameWidth,
	  def.format.video.nFrameHeight,
	  def.format.video.nStride,
	  def.format.video.nSliceHeight,
	  def.format.video.xFramerate, def.format.video.eColorFormat);
}

static int
openMAX_encode_fd(int yuv_fd, FILE*h264_fp)// char *outputfilename)
{

   OMX_VIDEO_PARAM_PORTFORMATTYPE format;
   OMX_PARAM_PORTDEFINITIONTYPE def;
   COMPONENT_T *video_encode = NULL;
   COMPONENT_T *list[5];
   OMX_BUFFERHEADERTYPE *buf;
   OMX_BUFFERHEADERTYPE *out;
   OMX_ERRORTYPE r;
   ILCLIENT_T *client;
   int status = 0;
   int framenumber = 0;

   memset(list, 0, sizeof(list));

   if ((client = ilclient_init()) == NULL) {
      return -3;
   }

   if (OMX_Init() != OMX_ErrorNone) {
      ilclient_destroy(client);
      return -4;
   }

   // create video_encode
   r = ilclient_create_component(client, &video_encode, "video_encode",
				 ILCLIENT_DISABLE_ALL_PORTS |
				 ILCLIENT_ENABLE_INPUT_BUFFERS |
				 ILCLIENT_ENABLE_OUTPUT_BUFFERS);
   if (r != 0) {
      printf
	 ("ilclient_create_component() for video_encode failed with %x!\n",
	  r);
      exit(1);
   }
   list[0] = video_encode;

   // get current settings of video_encode component from port 200
   memset(&def, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
   def.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
   def.nVersion.nVersion = OMX_VERSION;
   def.nPortIndex = 200;

   if (OMX_GetParameter
       (ILC_GET_HANDLE(video_encode), OMX_IndexParamPortDefinition,
	&def) != OMX_ErrorNone) {
      printf("%s:%d: OMX_GetParameter() for video_encode port 200 failed!\n",
	     __FUNCTION__, __LINE__);
      exit(1);
   }

   print_def(def);

   // Port 200: in 1/1 115200 16 enabled,not pop.,not cont. 320x240 320x240 @1966080 20
   def.format.video.nFrameWidth = WIDTH;
   def.format.video.nFrameHeight = HEIGHT;
   def.format.video.xFramerate = 30 << 16;
   def.format.video.nSliceHeight = def.format.video.nFrameHeight;
   def.format.video.nStride = def.format.video.nFrameWidth;
   def.format.video.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;

   print_def(def);

   r = OMX_SetParameter(ILC_GET_HANDLE(video_encode),
			OMX_IndexParamPortDefinition, &def);
   if (r != OMX_ErrorNone) {
      printf
	 ("%s:%d: OMX_SetParameter() for video_encode port 200 failed with %x!\n",
	  __FUNCTION__, __LINE__, r);
      exit(1);
   }

   memset(&format, 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
   format.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
   format.nVersion.nVersion = OMX_VERSION;
   format.nPortIndex = 201;
   format.eCompressionFormat = OMX_VIDEO_CodingAVC;

   printf("OMX_SetParameter for video_encode:201...\n");
   r = OMX_SetParameter(ILC_GET_HANDLE(video_encode),
			OMX_IndexParamVideoPortFormat, &format);
   if (r != OMX_ErrorNone) {
      printf
	 ("%s:%d: OMX_SetParameter() for video_encode port 201 failed with %x!\n",
	  __FUNCTION__, __LINE__, r);
      exit(1);
   }

   OMX_VIDEO_PARAM_BITRATETYPE bitrateType;
   // set current bitrate to 1Mbit
   memset(&bitrateType, 0, sizeof(OMX_VIDEO_PARAM_BITRATETYPE));
   bitrateType.nSize = sizeof(OMX_VIDEO_PARAM_BITRATETYPE);
   bitrateType.nVersion.nVersion = OMX_VERSION;
   bitrateType.eControlRate = OMX_Video_ControlRateVariable;
   bitrateType.nTargetBitrate = 1000000;
   bitrateType.nPortIndex = 201;
   r = OMX_SetParameter(ILC_GET_HANDLE(video_encode),
                       OMX_IndexParamVideoBitrate, &bitrateType);
   if (r != OMX_ErrorNone) {
      printf
        ("%s:%d: OMX_SetParameter() for bitrate for video_encode port 201 failed with %x!\n",
         __FUNCTION__, __LINE__, r);
      exit(1);
   }


   // get current bitrate
   memset(&bitrateType, 0, sizeof(OMX_VIDEO_PARAM_BITRATETYPE));
   bitrateType.nSize = sizeof(OMX_VIDEO_PARAM_BITRATETYPE);
   bitrateType.nVersion.nVersion = OMX_VERSION;
   bitrateType.nPortIndex = 201;

   if (OMX_GetParameter
       (ILC_GET_HANDLE(video_encode), OMX_IndexParamVideoBitrate,
       &bitrateType) != OMX_ErrorNone) {
      printf("%s:%d: OMX_GetParameter() for video_encode for bitrate port 201 failed!\n",
            __FUNCTION__, __LINE__);
      exit(1);
   }
   printf("Current Bitrate=%u\n",bitrateType.nTargetBitrate);



   printf("encode to idle...\n");
   if (ilclient_change_component_state(video_encode, OMX_StateIdle) == -1) {
      printf
	 ("%s:%d: ilclient_change_component_state(video_encode, OMX_StateIdle) failed",
	  __FUNCTION__, __LINE__);
   }

   printf("enabling port buffers for 200...\n");
   if (ilclient_enable_port_buffers(video_encode, 200, NULL, NULL, NULL) != 0) {
      printf("enabling port buffers for 200 failed!\n");
      exit(1);
   }

   printf("enabling port buffers for 201...\n");
   if (ilclient_enable_port_buffers(video_encode, 201, NULL, NULL, NULL) != 0) {
      printf("enabling port buffers for 201 failed!\n");
      exit(1);
   }

   printf("encode to executing...\n");
   ilclient_change_component_state(video_encode, OMX_StateExecuting);


   printf("looping for buffers...\n");


   fd_set fds;
   //int frame_len = 0;
   ssize_t frame_len = 0;
   struct timeval tm;
   int ret_code = 1;
   int  buf_size = SIZE;
   do {
      buf = ilclient_get_input_buffer(video_encode, 200, 1);
      if (buf == NULL) {
          printf("Doh, no buffers for me!\n");
      }
      else {
         char *frame = (char*)buf->pBuffer;
         for(;;){
	        FD_ZERO(&fds);
	        FD_SET(yuv_fd, &fds);
	        tm.tv_sec = 2;
	        tm.tv_usec = 0;
	        int ret = select(yuv_fd + 1, &fds, NULL, NULL, &tm);
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
	            /* fill it */
	            //generate_test_card(buf->pBuffer, &buf->nFilledLen, framenumber++);
                frame_len = read(yuv_fd, frame, buf_size);
                if(frame_len == 0){
                    framenumber +=1;
                    printf("read frame end!\n");
                    ret_code = 0;
                } else if(frame_len != buf_size) {
                    printf("want to read: %d\n", buf_size);
                    printf("frame_len: %d\n", frame_len);
                    buf_size = buf_size - frame_len;
                    frame = frame + frame_len;
                    //fwrite(pic, frame_len, 1, fp);
                } else {
                    framenumber +=1;
                    printf("read frame success!\n");
                    ret_code = 0;
                }
             }
             if (ret_code == 0){
                break; 
             }
         }
         if (ret_code == -1){
             goto finally;
         }
         buf->nFilledLen = SIZE;

         if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_encode), buf) !=
             OMX_ErrorNone) {
             printf("Error emptying buffer!\n");
             ret_code = -1;
             goto finally;
         }

         out = ilclient_get_output_buffer(video_encode, 201, 1);

         r = OMX_FillThisBuffer(ILC_GET_HANDLE(video_encode), out);
         if (r != OMX_ErrorNone) {
             printf("Error filling buffer: %x\n", r);
             ret_code = -1;
             goto finally;
         }

         if (out != NULL) {
             if (out->nFlags & OMX_BUFFERFLAG_CODECCONFIG) {
             int i;
             for (i = 0; i < out->nFilledLen; i++)
                 printf("%x ", out->pBuffer[i]);
                 printf("\n");
             }

             r = fwrite(out->pBuffer, 1, out->nFilledLen, h264_fp);
             if (r != out->nFilledLen) {
                 printf("fwrite: Error emptying buffer: %d!\n", r);
             }
             else {
                 printf("Writing frame %d/%d\n", framenumber, NUMFRAMES);
             }
             out->nFilledLen = 0;
         }
         else {
             printf("Not getting it :(\n");
             ret_code = -1;
             goto finally;
         }
      }
finally:      
      if (ret_code == -1){
          printf("select error\n");
          break;
      } else {
          printf("read yuv file end successfully\n");
      }
   }
   while (framenumber < NUMFRAMES);


   printf("Teardown.\n");

   printf("disabling port buffers for 200 and 201...\n");
   ilclient_disable_port_buffers(video_encode, 200, NULL, NULL, NULL);
   ilclient_disable_port_buffers(video_encode, 201, NULL, NULL, NULL);

   ilclient_state_transition(list, OMX_StateIdle);
   ilclient_state_transition(list, OMX_StateLoaded);

   ilclient_cleanup_components(list);

   OMX_Deinit();

   ilclient_destroy(client);
   return status;
}

int
main(int argc, char **argv)
{
   if (argc != 3) {
      printf("Usage: %s <filename>\n", argv[0]);
      exit(1);
   }

   int yuv_fd = open(argv[1], O_RDONLY, 0);
   if (yuv_fd == -1) {
      printf("open %s error : %s", argv[1], strerror(errno));
      return -1;
   }

   FILE *h264_fp;
   h264_fp = fopen(argv[2], "w");
   if (h264_fp == NULL) {
      printf("Failed to open '%s' for writing video\n", argv[2]);
      exit(1);
   }

   bcm_host_init();
   printf("file : %s %s\n", argv[1], argv[2]);

   int ret = 0; 
   ret = openMAX_encode_fd(yuv_fd, h264_fp);

   close(yuv_fd);
   fclose(h264_fp);
   return ret;
}
