#include <iostream>
#include <signal.h>
#include <cstdio>
#include <unistd.h>

//#include "FileStreamer.h"
#include "CreateAvi.h"

using namespace std;

static CreateAvi* streamer;
bool stop = false;

void signal_handler(int sigm) {
  delete streamer;
  return;
}

int main()
{
  streamer = new CreateAvi();

  /* ignore SIGPIPE (send if transmitting to closed sockets) */
  signal(SIGPIPE, SIG_IGN);
  if (signal(SIGINT, signal_handler) == SIG_ERR) {
    fprintf(stderr, "could not register signal handler\n");
    return 1;
  }

  streamer->start("test.avi");

  sleep(4);
  
  streamer->stop();
  delete streamer;
  cout << "end..." << endl;
  return 0;
}
