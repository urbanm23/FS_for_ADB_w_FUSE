#definition of variables

CXXFLAGS=$(shell pkg-config fuse --cflags)
LDFLAGS=$(shell pkg-config fuse --libs)
CXX=g++
LD=g++
CFLAGS=-Wall
EXECUTABLE=adbfuse
SOURCES=bt_adbfuse_urbanek_2014.cpp bt_adbfuse_urbanek_2014.h
OB=src/

#execution part

all: $(EXECUTABLE)

$(EXECUTABLE): adbfuse.o
	$(LD) $(CFLAGS) -o $(EXECUTABLE) $(OB)adbfuse.o $(LDFLAGS)
adbfuse.o: $(SOURCES)
	mkdir -p src/
	$(CXX) $(CFLAGS) -c -o $(OB)adbfuse.o bt_adbfuse_urbanek_2014.cpp $(CXXFLAGS)
clean:
	rm -rf $(OB) $(EXECUTABLE)
run: $(EXECUTABLE)
	./$(EXECUTABLE)
