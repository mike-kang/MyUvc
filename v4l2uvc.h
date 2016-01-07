#include <iostream>
#include <pthread.h>

#define NB_BUFFER 3

class v4l2uvc {
public:
  class ICBFunc {
  public:
    virtual void onFrame(void*, int) = 0;
    virtual void onError(char*) = 0;

  };
  v4l2uvc(int fps, int width, int height, ICBFunc* cbFunc);
  ~v4l2uvc();

  
  bool start();
  bool stop();
private:
  bool init();
  static void* run(void* arg);

  int m_fd;
  int m_fps;
  int m_width;
  int m_height;
  bool m_bStreaming;
  void *m_mem[NB_BUFFER];
  int m_mem_length[NB_BUFFER];
  ICBFunc* m_cbfunc;
  pthread_t m_client;
  bool m_bRunning;
};
