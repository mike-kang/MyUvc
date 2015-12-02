#include <iostream>

#include "v4l2uvc.h"
#include <vector>
#include <sys/time.h>

using namespace std;

class CreateAvi: public v4l2uvc::ICBFunc
{
public:
  enum Exception {
    EXCEPTION_CREATE_FILE,

  };
  
  CreateAvi();
  ~CreateAvi();
  virtual void onFrame(void*, int);
  virtual void onError(char*);
  void start(const char* dirpath);
  void stop();

private:
  v4l2uvc* m_v4l2uvc;

  vector<int> m_arrSize;
  int m_frames; 
  FILE* m_fp;

  struct timeval m_firstframetime;
  struct timeval m_lastframetime;
  
  

};

