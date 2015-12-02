#include <iostream>

#include "v4l2uvc.h"

class FileStreamer: public v4l2uvc::ICBFunc
{
public:
  enum Exception {
    EXCEPTION_CREATE_DIRECTORY,

  };
  FileStreamer();
  ~FileStreamer();
  virtual void onFrame(void*, int);
  virtual void onError(char*);
  void start(const char* dirpath);
  void stop();

private:
  v4l2uvc* m_v4l2uvc;
  int m_index;  //for file
  char m_path[255];
  char* m_ppath;


};
