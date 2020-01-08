platform=$(shell uname)

ifeq ($(wildcard /usr/include/wiringPi.h),) 
    PI_FLAG = 
else 
    PI_FLAG = -DRASPBERRY_PI -lwiringPi
endif 

CPPFLAGS=-O2 -g -std=c++11 ${PI_FLAG}
ifeq ($(platform),Darwin)
  ALL=bin/ortho
  GL_OPTS=-framework OpenGL -framework GLUT -Wno-deprecated-declarations
else ifeq ($(platform),Linux)
  ALL=bin/ortho
  GL_OPTS=-lGL -lglut -lGLU -lm
endif

all: $(ALL)

clean:
	rm -rf bin/*

bin/ortho: src/ortho.h src/ortho.cpp src/patterns.h src/util.h src/palettes.h src/HomeBridgeListener.h src/opc/opc_client.c src/opc/opc.h src/opc/types.h
	mkdir -p bin
	g++ ${CPPFLAGS} -o $@ src/ortho.cpp src/opc/opc_client.c
