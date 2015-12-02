CPPFLAGS =  -g -I.. 
EXE = main

OBJS = main.o HttpStreamer.o v4l2uvc.o CreateAvi.o

all :  $(EXE)

$(EXE) : ${OBJS}  $(CAMERA_LIB) 
	$(CXX) -o $@ ${OBJS} -lpthread -lrt 

 



 .PHONY : clean
clean :
	-rm *.o main