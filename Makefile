#CXX=g++
#CC=gcc
INCLUDES=-I/usr/include\
		 -I/usr/local/include\
		 -I./3rd/include\
		 -I./3rd/include/lua-5.14

CXXFLAGS=-c -Wall -O2 -g $(INCLUDES)

LIBS=./3rd/lib/liblua.a ./3rd/lib/libjemalloc.a
	
SRCDIRS=. ./common ./common/somgr ./common/timer
SRCS=$(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.c)) 
SRCCPP=$(foreach dir,$(SRCDIRS)
OBJS=$(SRCS:.c=.o)
PROG=./mul

all: $(PROG) $(MODULE)

install:
clean:
	rm -rf $(OBJS) $(PROG) $(PROG).core

$(PROG): $(OBJS)
	gcc -g $(OBJS) -o $(PROG) $(LIBS) -lpthread -ldl -lrt -lc -rdynamic -lpthread -lm
.c.o:
	gcc $(CXXFLAGS) -fPIC $< -o $@

