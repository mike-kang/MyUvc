#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "FileStreamer.h"
#include "huffman.h"

#define PREFIX "image_"

FileStreamer::FileStreamer()
{
  m_v4l2uvc = new v4l2uvc(30, 640, 480, this);

}

FileStreamer::~FileStreamer()
{
  delete m_v4l2uvc;
}

void FileStreamer::onFrame(void* buf, int len)
{
  unsigned char *ptcur = (unsigned char*)buf;
  int sizein;

  sprintf(m_ppath, "%06d.jpg", m_index);
  printf("write %s %06d\n",m_path, len);
  FILE* fp = fopen(m_path, "w");

  while (((ptcur[0] << 8) | ptcur[1]) != 0xffc0)
      ptcur++;
  sizein = ptcur - (unsigned char*)buf;

  
  if( fwrite(buf, sizein, 1, fp) <= 0) {
    printf("error 1\n");
    return ;
  }
  if( fwrite(dht_data, DHT_SIZE, 1, fp) <= 0){
    printf("error 2\n");
    return ;
  }
  if( fwrite(ptcur, len - sizein, 1, fp) <= 0){
    printf("error 3\n");
    return ;
  }
  fclose(fp);
  
  m_index++;
}

void FileStreamer::onError(char* strErr)
{
  fprintf(stderr, strErr);

}

void FileStreamer::start(const char* dirpath)
{
  if(mkdir(dirpath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0){
    perror("mkdir failed:");
    throw EXCEPTION_CREATE_DIRECTORY;
  }
  sprintf(m_path, "%s/%s", dirpath, PREFIX);
  m_index = 0;
  m_ppath = m_path + strlen(m_path);
  m_v4l2uvc->start();
}

void FileStreamer::stop()
{
  m_v4l2uvc->stop();
}

