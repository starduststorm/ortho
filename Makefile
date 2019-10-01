platform=$(shell uname)

CPPFLAGS=-O2 -g -std=c++11
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

bin/ortho: src/ortho.cpp src/patterns.h src/util.h src/palettes.h src/opc/opc_client.c src/opc/opc.h src/opc/types.h
	mkdir -p bin
	g++ ${CPPFLAGS} -o $@ src/ortho.cpp src/opc/opc_client.c