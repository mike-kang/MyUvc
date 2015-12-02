#include <iostream>
#include <pthread.h>

#include "v4l2uvc.h"

class HttpStreamer: public v4l2uvc::ICBFunc
{
public:
  HttpStreamer();
  ~HttpStreamer();
  virtual void onFrame(void*, int);
  virtual void onError(char*);
  void join();

private:
  void communicate(int sock);
  static void* run(void* arg);
  static void* run_comm(void* arg);
  
  int m_listenSock;
  int m_clientSock;
  pthread_t m_listen;
  pthread_t m_client;

  v4l2uvc* m_v4l2uvc;
  bool m_bRunning;
};
