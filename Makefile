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

bin/ortho: src/ortho.h src/ortho.cpp src/patterns.h src/util.h src/palettes.h src/HomeBridgeListener.h src/opc/opc_client.c src/opc/opc.h src/opc/types.h
	mkdir -p bin
	g++ ${CPPFLAGS} -o $@ src/ortho.cpp src/opc/opc_client.c
