CXX=       	g++
CXXFLAGS= 	-g -gdwarf-2 -std=gnu++11 -Wall -Iinclude -fPIC
LDFLAGS=	-Llib
AR=		ar
ARFLAGS=	rcs

LIB_HEADERS=	$(wildcard include/*/*.h)
LIB_SOURCE=	$(wildcard include/*/*.cpp)
LIB_OBJECTS=	$(LIB_SOURCE:.cpp=.o)
LIB_STATIC=	lib/libfs.a

SHELL_SOURCE=	$(wildcard src/*.cpp)
SHELL_OBJECTS=	$(SHELL_SOURCE:.cpp=.o)
SHELL_PROGRAM=	bin/shell

all:    $(LIB_STATIC) $(SHELL_PROGRAM)

%.o:	%.cpp $(LIB_HEADERS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(LIB_STATIC):		$(LIB_OBJECTS) $(LIB_HEADERS)
	$(AR) $(ARFLAGS) $@ $(LIB_OBJECTS)

$(SHELL_PROGRAM):	$(SHELL_OBJECTS) $(LIB_STATIC)
	$(CXX) $(LDFLAGS) -o $@ $(SHELL_OBJECTS) -lfs

test:	$(SHELL_PROGRAM)
	@for test_script in tests/test_*.sh; do $${test_script}; done

clean:
	rm -f $(LIB_OBJECTS) $(LIB_STATIC) $(SHELL_OBJECTS) $(SHELL_PROGRAM)

.PHONY: all clean
