CC			= 	gcc
AR 			= 	ar
CFLAGS 		+= 	-std=c99 -Wall -Werror -g
ARFLAGS 	= 	rvs
INCLUDES	= 	-I.
LDFLAGS 	= 	-L.
OPTFLAGS 	= 	

TARGETS		=	supervisor \
				server \
				client

.PHONY: all test clean cleanTestFiles cleanall
.SUFFIXES: .c .h

%: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

all: $(TARGETS)

cleanTestFiles:
	#checking if there are previous test files...
	#if errors are displayed, it means no previously created files were found
	#else it just deletes them to make room for new files and measurements
	-rm *.log

test: cleanTestFiles
	bash ./test.sh

clean:
	-rm -f $(TARGETS)

cleanall: clean cleanTestFiles
