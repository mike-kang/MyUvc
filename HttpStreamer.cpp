/*
from https://svn.code.sf.net/p/mjpg-streamer/code
Thanks
*/


#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "HttpStreamer.h"
#include "huffman.h"

#define PORT 8090
#define BOUNDARY "arflebarfle"

HttpStreamer::HttpStreamer(): m_bRunning(false), m_v4l2uvc(NULL)
{
  int on;
  int s;
  struct sockaddr_in addr;

  m_v4l2uvc = new v4l2uvc(30, 640, 480, this);

  /* open socket for server */
  m_listenSock = socket(PF_INET, SOCK_STREAM, 0);
  if ( m_listenSock < 0 ) {
    fprintf(stderr, "socket failed\n");
    throw 1;
  }

  /* ignore "socket already in use" errors */
  if (setsockopt(m_listenSock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
    perror("setsockopt(SO_REUSEADDR) failed");
    throw 2;
  }

  /* configure server address to listen to all local IPs */
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if ( bind(m_listenSock, (struct sockaddr*)&addr, sizeof(addr)) != 0 ) {
    fprintf(stderr, "bind failed\n");
    perror("Bind");
    throw 3;
  }

  /* start listening on socket */
  if ( listen(m_listenSock, 10) != 0 ) {
    fprintf(stderr, "listen failed\n");
    throw 4;
  }

  //start
  s = pthread_create(&m_listen, NULL, run, this);
  if (s != 0)
  {
      fprintf(stderr, "error create thread\n");
      throw 3;
  }


}

HttpStreamer::~HttpStreamer()
{
  m_bRunning = false;

  delete m_v4l2uvc;
  close(m_listenSock);
}

void HttpStreamer::join()
{
  int s;
  void *res;

  s = pthread_join(m_listen, &res);
}


struct ClientData{
  HttpStreamer* m_hs;
  int m_sock;

  ClientData(HttpStreamer* hs, int sock):m_hs(hs), m_sock(sock){};
};

//static
void* HttpStreamer::run(void* arg)
{
  HttpStreamer* my = (HttpStreamer*)arg;
  int sock;
  pthread_t pid;

  my->m_bRunning = true;

  while(my->m_bRunning){
    
    printf("wiat accept\n");
    sock = accept(my->m_listenSock, 0, 0);
    printf("client socket: %d\n", sock);
    ClientData* cd = new ClientData(my, sock);
    pthread_create(&pid, NULL, run_comm, cd);
    //my->m_mapClientSocket[pid] =  sock;
    pthread_detach(pid);
  }
  
  return NULL;
}

//static
void* HttpStreamer::run_comm(void* arg)
{
  ClientData* cd = (ClientData*)arg;
  cd->m_hs->communicate(cd->m_sock);
  delete cd;
}

void HttpStreamer::communicate(int sock)
{
  char buffer[1024] = {0};
  int ok = 1;
  fd_set fds;
  struct timeval to;
  bool bAcceptImage = false;
  printf("communicate sock %d\n", sock);

  /* set timeout to 5 seconds */
  to.tv_sec  = 5;
  to.tv_usec = 0;
  FD_ZERO(&fds);
  FD_SET(sock, &fds);
  if( select(sock+1, &fds, NULL, NULL, &to) <= 0) {
    close(sock);
    printf("sock close\n");
    return ;
  }
  
  printf("communicate\n");
  printf("sock =%d\n", sock);
  read(sock, buffer, sizeof(buffer));
  
  printf("-- buffer:%s\n", buffer);

  char* start = strstr(buffer, "Accept:");
  if(!start)
    return;
  start+=7;
  char* end = strstr(start, "\n");
  *end = '\0';
  if(strstr(start, "image/*")){
    printf("communicate bAcceptImage\n");
    bAcceptImage = true;
  }

  printf("communicate server send\n");
  sprintf(buffer, "HTTP/1.0 200 OK\r\n" \
                  "Server: UVC Streamer\r\n" \
                  "Content-Type: multipart/x-mixed-replace;boundary=" BOUNDARY "\r\n" \
                  "Cache-Control: no-cache\r\n" \
                  "Cache-Control: private\r\n" \
                  "Pragma: no-cache\r\n" \
                  "\r\n" \
                  "--" BOUNDARY "\n");

  ok = ( write(sock, buffer, strlen(buffer)) >= 0)?1:0;

  m_clientSock = sock;
  m_v4l2uvc->start();
}

#if 0
void HttpStreamer::onFrame(void* buf, int len)
{
  printf("onFrame %x %d\n", buf, len);
}
#else
void HttpStreamer::onFrame(void* buf, int len)
{
  char buffer[1024] = {0};
  unsigned char *ptcur = (unsigned char*)buf;
  int sizein;
  int ok = 1;

  //printf("onFrame %x %d\n", buf, len);
  
  sprintf(buffer, "Content-type: image/jpeg\n\n");
  ok = ( write(m_clientSock, buffer, strlen(buffer)) >= 0)?1:0;
  
  while (((ptcur[0] << 8) | ptcur[1]) != 0xffc0)
      ptcur++;
  sizein = ptcur - (unsigned char*)buf;
  if( write(m_clientSock, buf, sizein) <= 0){
    printf("error 1\n");
    return ;
  }
  if( write(m_clientSock, dht_data, DHT_SIZE) <= 0){
    printf("error 2\n");
    return ;
  }
  if( write(m_clientSock, ptcur, len - sizein) <= 0){
    printf("error 3\n");
    return ;
  }
  sprintf(buffer, "\n--" BOUNDARY "\n");

  int r = 0;
  r = write(m_clientSock, buffer, strlen(buffer));
  if(r< -1){
    perror("write failed");

  }
}
#endif
void HttpStreamer::onError(char* strErr)
{
  fprintf(stderr, strErr);
}

