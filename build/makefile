OPENCV_INC=`pkg-config opencv --cflags`
OPENCV_LIB=`pkg-config opencv --libs`
CC=gcc
CXX=g++
CFLAGS=-Wall -O2
SRC_PATH=../src
LDFLAGS=
BIN=extract32 udpdump udptest check udpcat udpsend udpcheck

all: $(BIN)

view32: $(SRC_PATH)/view32.c
	$(CXX) -o $@ $^ $(CFLAGS) $(LDFLAGS) $(OPENCV_INC) $(OPENCV_LIB)

extract32: $(SRC_PATH)/extract32.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

udpdump: $(SRC_PATH)/udpdump.c
	$(CC) $(CFLAGS) $(LDFLAGS) -pthread -o $@ $^

udptest: $(SRC_PATH)/udptest.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

udpcat: $(SRC_PATH)/udpcat.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

check: $(SRC_PATH)/check.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

udpsend: $(SRC_PATH)/udpsend.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

udpcheck: $(SRC_PATH)/udpcheck.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm $(BIN)


