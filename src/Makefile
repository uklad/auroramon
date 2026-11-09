# Makefile for 'auroramon' program
# Requires wxWidgets 3.1.5 or later (for wxWebRequest), eg. package libwxgtk3.2-dev

SRCS= auroramonApp.cpp auroramonMain.cpp chart.cpp dialog.cpp graphdialog.cpp statuspanel.cpp dates.cpp sun.cpp comms.cpp alarms.cpp pvoutput.cpp mqtt.cpp

OBJS=$(patsubst %.cpp,%.o,$(SRCS))
ifeq ($(OS),Windows_NT)
OBJS += auroramon_rc.o
endif
DEPS=$(patsubst %.cpp,%.d,$(SRCS))

WX_CONFIG ?= wx-config

CXXFLAGS ?= -O2 -Wall
ALL_CXXFLAGS = $(CXXFLAGS) `$(WX_CONFIG) --cxxflags` -I. -MMD -MP

all: auroramon

%.o: %.cpp
	$(CXX) $(ALL_CXXFLAGS) -c $< -o $@

auroramon_rc.o: auroramon.rc ../icons/auroramon.ico
	`$(WX_CONFIG) --rescomp` -I. $< -o $@

auroramon: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) `$(WX_CONFIG) --libs`

clean:
	rm -f *.o *.d *~

distclean: clean
	rm -f auroramon auroramon.exe

.PHONY: all clean distclean

-include $(DEPS)
