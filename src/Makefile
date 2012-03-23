# Makefile for 'auoramon' program

SRCS= auroramonApp.cpp auroramonMain.cpp chart.cpp dialog.cpp graphdialog.cpp statuspanel.cpp dates.cpp sun.cpp comms.cpp alarms.cpp

OBJS=$(patsubst %.cpp,%.o,$(SRCS))

WX_LIBS = `wx-config --libs`
   

LIBS=-lstdc++

CPPFLAGS = -O2 -s  -Wall -fexceptions `wx-config --cflags` -I/usr/include/wx-2.8 -D__WXGTK__

CXXFLAGS = -O2 -s  -Wall -fexceptions `wx-config --cflags` -I/usr/include/wx-2.8 -D__WXGTK__

all: auroramon

.cpp.o:
	$(CXX) $(CXXFLAGS) -I. -c -fno-exceptions $<

auroramon: $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LIBS) $(WX_LIBS)

clean:
	rm -f *.o *~

distclean: clean
	rm -f auroramon

