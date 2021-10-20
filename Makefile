platform=$(shell uname)

ifeq ($(wildcard /usr/include/wiringPi.h),) 
    PI_FLAG = 
else 
    PI_FLAG = -DRASPBERRY_PI -lwiringPi
endif 

CPPFLAGS=-O2 -g -std=c++17 ${PI_FLAG}
ifeq ($(platform),Darwin)
  ALL=bin/ortho
else ifeq ($(platform),Linux)
  ALL=bin/ortho
endif

all: $(ALL)

clean:
	rm -rf bin/*

bin/ortho: src/ortho.cpp src/opc/opc_client.c
	mkdir -p bin
	g++ ${CPPFLAGS} -o $@ src/ortho.cpp src/opc/opc_client.c
