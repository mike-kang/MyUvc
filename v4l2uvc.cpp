/*
from https://svn.code.sf.net/p/mjpg-streamer/code
Thanks
*/

#include <stdlib.h>
#include <linux/kernel.h>
#include <linux/videodev2.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>

#include "v4l2uvc.h"
#define VIDEO_DEVICE "/dev/video0"

static int debug = 1;

v4l2uvc::v4l2uvc(int fps, int width, int height, ICBFunc* cbFunc)
  :m_fps(fps), m_width(width), m_height(height), m_cbfunc(cbFunc), m_bStreaming(false), m_bRunning(false)
{
  if(!init())
    throw 1;
}

//static
void* v4l2uvc::run(void* arg)
{
  v4l2uvc* my = (v4l2uvc*)arg;
  struct v4l2_buffer buf;
  int ret;
  
  while(my->m_bRunning){
    memset(&buf, 0, sizeof(struct v4l2_buffer));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(my->m_fd, VIDIOC_DQBUF, &buf);
    if (ret < 0) {
      my->m_cbfunc->onError("Unable to dequeue buffer .\n");
      break;
    }
    my->m_cbfunc->onFrame(my->m_mem[buf.index], buf.bytesused);

    ret = ioctl(my->m_fd, VIDIOC_QBUF, &buf);
    
    if (ret < 0) {
      my->m_cbfunc->onError("Unable to requeue buffer.\n");
      break;
    }

  }
  
  if (debug)
    printf("end run\n");

  
  return NULL;
}

v4l2uvc::~v4l2uvc()
{
  int s;
  void *res;
  
  if (debug)
    printf("~v4l2uvc start\n");

  stop();
  
  close(m_fd);
  
  if (debug)
    printf("~v4l2uvc end\n");
}


bool v4l2uvc::init()
{
  int i;
  int ret = 0;
  v4l2_capability cap;
  struct v4l2_format fmt;

  if ((m_fd = open(VIDEO_DEVICE, O_RDWR)) == -1) {
    m_cbfunc->onError("ERROR opening V4L interface \n");
    return false;
  }
  memset(&cap, 0, sizeof(struct v4l2_capability));
  ret = ioctl(m_fd, VIDIOC_QUERYCAP, &cap);
  if (ret < 0) {
    m_cbfunc->onError("Error opening device : unable to query device.\n");
    return false;
  }

  if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0) {
    m_cbfunc->onError("Error opening device : video capture not supported.\n");
    return false;
  }
  if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
    m_cbfunc->onError("device does not support streaming i/o\n");
    return false;
  }
  /*
   * set format in 
   */
  memset(&fmt, 0, sizeof(struct v4l2_format));
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = m_width;
  fmt.fmt.pix.height = m_height;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
  fmt.fmt.pix.field = V4L2_FIELD_ANY;
  ret = ioctl(m_fd, VIDIOC_S_FMT, &fmt);
  if (ret < 0) {
    m_cbfunc->onError("Unable to set format: \n");
    return false;
  }
  if ((fmt.fmt.pix.width != m_width) ||
      (fmt.fmt.pix.height != m_height)) {
    m_cbfunc->onError(" format asked unavailable get widthd height  \n");
    m_width = fmt.fmt.pix.width;
    m_height = fmt.fmt.pix.height;
  }

  /*
   * set framerate 
   */
  struct v4l2_streamparm setfps;
  memset(&setfps, 0, sizeof(struct v4l2_streamparm));
  setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  setfps.parm.capture.timeperframe.numerator = 1;
  setfps.parm.capture.timeperframe.denominator = m_fps;
  ret = ioctl(m_fd, VIDIOC_S_PARM, &setfps);

 
  return true;

}


bool v4l2uvc::start()
{
  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  int ret;

 /*  
   * request buffers 
   */
   
  struct v4l2_requestbuffers rb;
  memset(&rb, 0, sizeof(struct v4l2_requestbuffers));
  rb.count = NB_BUFFER;
  rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  rb.memory = V4L2_MEMORY_MMAP;

  ret = ioctl(m_fd, VIDIOC_REQBUFS, &rb);
  if (ret < 0) {
    m_cbfunc->onError("Unable to allocate buffers: .\n");
    return false;
  }
  if(rb.count != NB_BUFFER){
    m_cbfunc->onError("Buffer count supported\n");
    return false;
  }
  /*
   * map the buffers 
   */
  struct v4l2_buffer buf;
  for (int i = 0; i < NB_BUFFER; i++) {
    memset(&buf, 0, sizeof(struct v4l2_buffer));
    buf.index = i;
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    ret = ioctl(m_fd, VIDIOC_QUERYBUF, &buf);
    if (ret < 0) {
      m_cbfunc->onError("Unable to query buffer.\n");
      return false;
    }
    if (debug)
      printf("length: %u offset: %u\n", buf.length, buf.m.offset);
    m_mem_length[i] = buf.length;
    m_mem[i] = mmap(0 /* start anywhere */ ,
                      buf.length, PROT_READ, MAP_SHARED, m_fd,
                      buf.m.offset);
    if (m_mem[i] == MAP_FAILED) {
      m_cbfunc->onError("Unable to map buffer (%d)\n" );
      return false;
    }
    if (debug)
      printf("Buffer mapped at address %p.\n", m_mem[i]);
  }
  
  /*
   * Queue the buffers. 
   */
  for (int i = 0; i < NB_BUFFER; ++i) {
    memset(&buf, 0, sizeof(struct v4l2_buffer));
    buf.index = i;
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    ret = ioctl(m_fd, VIDIOC_QBUF, &buf);
    if (ret < 0) {
      //m_cbfunc->onError("Unable to queue buffer (%d).\n", errno);
      return false;
    }
  }

  ret = ioctl(m_fd, VIDIOC_STREAMON, &type);
  if (ret < 0) {
    //m_cbfunc->onError("Unable to %s capture: %d.\n", "start", errno);
    m_cbfunc->onError("Unable to capture:\n");
    return false;
  }
  
  m_bStreaming = true;
  m_bRunning = true;
  //start
  
  if (debug)
    printf("v4l2uvc::start\n");
  
  ret = pthread_create(&m_client, NULL, run, this);
  if (ret != 0)
  {
      m_cbfunc->onError("error create thread\n");
      return false;
  }
  return true;
}

bool v4l2uvc::stop()
{
  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  int ret;
  void *res;

  if(m_bRunning){
    m_bRunning = false;
    pthread_join(m_client, &res);
  }
    
  for(int i = 0; i < NB_BUFFER; i++){
    munmap(m_mem[i], m_mem_length[i]);
  }

  if(m_bStreaming){
    ret = ioctl(m_fd, VIDIOC_STREAMOFF, &type);
    if (ret < 0) {
      //m_cbfunc->onError("Unable to %s capture: %d.\n", "stop", errno);
      return false;
    }
    m_bStreaming = false;
  }

  return true;
}




