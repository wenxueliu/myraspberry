myraspberry
===========

I have try to use raspberry to capture video and encode with h264. It took me a lot of time to get it. some experience may
help you.

* the blog on internet about video captured with v4l2 and encoded with x264 almost cannot be compile pass. you should 
reference the official of [v4l2]() and [libx264]()
* if you want to encode video as h264 with libx264, you will discourage to find that it so bad because the libx264 library
can not support hardware accelerate.
* I'm appreciate that the raspberry have OpenMAX make it so powerfull. 

Usage
============
cd encode;make
./hello_encode.bin

TODO:

h2642mp4 
============
sudo apt-get install libmp4v2-2 libmp4v2-dev


Append
============
mplayer -demuxer rawvideo -rawvideo w=640:h=480:format=i420  test.yuv
mplayer -fps 15 test.h264
