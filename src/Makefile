CFLAGS := -Ofast -march=native -mtune=native \
	  -fPIC -fopenmp -DSKIP_MAIN_COMPILATION
CFLAGS += -Wall -Wextra -pedantic -std=c11
SRC := fast_genfromtxt.c
HEADERS := fast_genfromtxt.h double_conversion_wrap.h

SRC_CXX := double_conversion_wrap.cpp
# OBJ_CXX := double_conversion_wrap.cpp.o
CXXFLAGS := -Ofast -march=native -mtune=native -DNDEBUG -fPIC -fopenmp
CXXFLAGS += -Wall -Wextra -pedantic -std=c++17

-include Makefile.local

libfast_genfromtxt.so: $(SRC) $(OBJ_CXX) $(HEADERS) Makefile Makefile.local
	gcc $(OBJ_CXX) $(SRC) -shared $(CFLAGS) -lm -o libfast_genfromtxt.so $(LDFLAGS)

%.cpp.o: %.cpp %.h Makefile Makefile.local
	g++ -c $< -o $@ $(CXXFLAGS)

clean:
	-@rm -f *.o *.so
