# Kolby Kiesling
# 07 / 16 / 2019
# kjk15b@acu.edu

DEBUGFLAGS=-g
#DEBUGFLAGS=
CFLAGS=$(DEBUGFLAGS) -Wall -Os -I$(MIDASSYS)/include -I$(MIDASSYS)/drivers/class -I$(MIDASSYS)/drivers/bus -fpermissive
CXXFLAGS=$(CFLAGS)
LDFLAGS=$(MIDASSYS)/linux/lib/mfe.o  -L$(MIDASSYS)/linux/lib -lmidas -lpthread -lutil -lrt -lz 


all: feMCFD

rs232.o: $(MIDASSYS)/drivers/bus/rs232.cxx $(MIDASSYS)/drivers/bus/rs232.h
	g++ -c $(CFLAGS) $(MIDASSYS)/drivers/bus/rs232.cxx

multi.o: $(MIDASSYS)/drivers/class/multi.cxx $(MIDASSYS)/drivers/class/multi.h
	g++ -c $(CFLAGS) $(MIDASSYS)/drivers/class/multi.cxx
	
dd_mcfd16.o: dd_mcfd16.cxx dd_mcfd16.h
	g++ $(CXXFLAGS) -c dd_mcfd16.cxx 

feMCFD: feMCFD.cc rs232.o multi.o dd_mcfd16.o
	g++ -o $@ $(CXXFLAGS) $^ $(LDFLAGS)

clean:
	rm -f feMCFD *.o

