/* from  http://sourceforge.net/projects/jpegtoavi/
thanks
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "CreateAvi.h"
#include "huffman.h"
#include "avifmt.h"

#define WIDTH 640
#define HEIGHT 480

CreateAvi::CreateAvi()
{
  m_v4l2uvc = new v4l2uvc(30, 640, 480, this);

}

CreateAvi::~CreateAvi()
{
  delete m_v4l2uvc;
}

void CreateAvi::onFrame(void* buf, int len)
{
  unsigned char *ptcur = (unsigned char*)buf;
  int sizein, aligned_size;
  off_t remnant;

  fwrite("00db", 4, 1, m_fp);

  aligned_size = len + DHT_SIZE;
  remnant = (4-(aligned_size%4)) % 4;
  aligned_size += remnant;
  m_arrSize.push_back(aligned_size);
  
  fwrite(&aligned_size, 4, 1, m_fp);

  if(!m_frames){
    gettimeofday(&m_firstframetime, NULL);
  }
  else{
    gettimeofday(&m_lastframetime, NULL);
  }
    
  m_frames++;
  

  while (((ptcur[0] << 8) | ptcur[1]) != 0xffc0)
      ptcur++;
  sizein = ptcur - (unsigned char*)buf;

  if( fwrite(buf, sizein, 1, m_fp) <= 0) {
    printf("error 1\n");
    return ;
  }
  if( fwrite(dht_data, DHT_SIZE, 1, m_fp) <= 0){
    printf("error 2\n");
    return ;
  }
  if( fwrite(ptcur, len - sizein, 1, m_fp) <= 0){
    printf("error 3\n");
    return ;
  }
  
  if(remnant)
    fwrite(buf, remnant, 1, m_fp);
}

void CreateAvi::onError(char* strErr)
{
  fprintf(stderr, strErr);

}

void CreateAvi::start(const char* path)
{
  if((m_fp = fopen(path, "w")) == NULL){
    perror("open failed:");
    throw EXCEPTION_CREATE_FILE;
  }
  
  m_frames = 0;
  //bFirst = true;
  fseek(m_fp, 12 + sizeof(AVI_list_hdrl) + 12, SEEK_SET);

  
  m_v4l2uvc->start();
  
}

#define LILEND4(x) (x)

void CreateAvi::stop()
{
  DWORD per_usec = 1;
  DWORD width = WIDTH;
  DWORD height = HEIGHT;
  DWORD frames = m_frames;
  unsigned int fps;
  unsigned short img0;
  off64_t jpg_sz_64, riff_sz_64;
  long jpg_sz = 0;
  const off64_t MAX_RIFF_SZ = 2147483648LL; /* 2 GB limit */
  DWORD riff_sz;
  DWORD len;

  struct AVI_list_hdrl hdrl = {
    /* header */
    {
      {'L', 'I', 'S', 'T'},
      LILEND4(sizeof(struct AVI_list_hdrl) - 8),
      {'h', 'd', 'r', 'l'}
    },

    /* chunk avih */
    {'a', 'v', 'i', 'h'},
    LILEND4(sizeof(struct AVI_avih)),
    {
      LILEND4(per_usec),
      LILEND4(1000000 * (jpg_sz/frames) / per_usec),
      LILEND4(0),
      LILEND4(AVIF_HASINDEX),
      LILEND4(frames),
      LILEND4(0),
      LILEND4(1),
      LILEND4(0),
      LILEND4(width),
      LILEND4(height),
      {LILEND4(0), LILEND4(0), LILEND4(0), LILEND4(0)}
    },

    /* list strl */
    {
      {
	{'L', 'I', 'S', 'T'},
	LILEND4(sizeof(struct AVI_list_strl) - 8),
	{'s', 't', 'r', 'l'}
      },

      /* chunk strh */
      {'s', 't', 'r', 'h'},
      LILEND4(sizeof(struct AVI_strh)),
      {
	{'v', 'i', 'd', 's'},
	{'M', 'J', 'P', 'G'},
	LILEND4(0),
	LILEND4(0),
	LILEND4(0),
	LILEND4(per_usec),
	LILEND4(1000000),
	LILEND4(0),
	LILEND4(frames),
	LILEND4(0),
	LILEND4(0),
	LILEND4(0)
      },
      
      /* chunk strf */
      {'s', 't', 'r', 'f'},
      sizeof(struct AVI_strf),
      {      
	LILEND4(sizeof(struct AVI_strf)),
	LILEND4(width),
	LILEND4(height),
	LILEND4(1 + 24*256*256),
	{'M', 'J', 'P', 'G'},
	LILEND4(width * height * 3),
	LILEND4(0),
	LILEND4(0),
	LILEND4(0),
	LILEND4(0)
      },

      /* list odml */
      {
	{
	  {'L', 'I', 'S', 'T'},
	  LILEND4(16),
	  {'o', 'd', 'm', 'l'}
	},
	{'d', 'm', 'l', 'h'},
	LILEND4(4),
	LILEND4(frames)
      }
    }
  };

  m_v4l2uvc->stop();

  DWORD streammingTime = m_lastframetime.tv_sec - m_firstframetime.tv_sec;
  per_usec = streammingTime * 1000000 / m_frames;

  vector<int>::iterator iter;
  for(iter = m_arrSize.begin(); iter != m_arrSize.end(); iter++)
    jpg_sz += *iter;

  //indices
  fwrite("idx1", 4, 1, m_fp);
  len = 16 * frames;
  fwrite(&len, 4, 1, m_fp);
  len = 18;
  DWORD offset = 4;
  for(iter = m_arrSize.begin(); iter != m_arrSize.end(); iter++){
    fwrite("00db", 4, 1, m_fp);
    fwrite(&len, 4, 1, m_fp);
    fwrite(&offset, 4, 1, m_fp);
    len = *iter;
    fwrite(&len, 4, 1, m_fp);
    offset += (len + 8);
  }

  riff_sz = sizeof(struct AVI_list_hdrl) + 4 + 4 + jpg_sz
    + 8*frames + 8 + 8 + 16*frames;

  /* list hdrl */
  fseek(m_fp, 0, SEEK_SET);
  fwrite("RIFF", 4, 1, m_fp);
  //len = riff_sz;
  fwrite(&riff_sz, 4, 1, m_fp);
  fwrite("AVI ", 4, 1, m_fp);


  hdrl.avih.us_per_frame = LILEND4(per_usec);
  hdrl.avih.max_bytes_per_sec = LILEND4(1000000 * (jpg_sz/frames)
				      / per_usec);
  hdrl.avih.tot_frames = LILEND4(frames);
  hdrl.avih.width = LILEND4(width);
  hdrl.avih.height = LILEND4(height);
  hdrl.strl.strh.scale = LILEND4(per_usec);
  hdrl.strl.strh.rate = LILEND4(1000000);
  hdrl.strl.strh.length = LILEND4(frames);
  hdrl.strl.strf.width = LILEND4(width);
  hdrl.strl.strf.height = LILEND4(height);
  hdrl.strl.strf.image_sz = LILEND4(width * height * 3);
  hdrl.strl.list_odml.frames = LILEND4(frames);	/*  */
    

  fwrite(&hdrl, sizeof(hdrl), 1, m_fp);

  fwrite("LIST", 4, 1, m_fp);
  len = jpg_sz + 8*frames + 4;
  fwrite(&len, 4, 1, m_fp);
  fwrite("movi", 4, 1, m_fp);

  fclose(m_fp);
}



