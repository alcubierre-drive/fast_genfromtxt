CFLAGS := -Ofast -march=native -mtune=native -DNDEBUG -fPIC -fopenmp -DSKIP_MAIN_COMPILATION
CFLAGS += -Wall -Wextra -pedantic -std=c11
SRC := genfromtxt_parallel.c genfromtxt_serial.c
HEADERS := genfromtxt_parallel.h genfromtxt_serial.h vector.h double_conversion_wrap.h

SRC_CXX := double_conversion_wrap.cpp
# OBJ_CXX := double_conversion_wrap.cpp.o
CXXFLAGS := -Ofast -march=native -mtune=native -DNDEBUG -fPIC -fopenmp
CXXFLAGS += -Wall -Wextra -pedantic -std=c++17

-include Makefile.local

libfast_genfromtxt.so: $(SRC) $(OBJ_CXX) $(HEADERS) Makefile Makefile.local
	gcc $(OBJ_CXX) $(SRC) -shared $(CFLAGS) -o libfast_genfromtxt.so $(LDFLAGS)

%.cpp.o: %.cpp %.h Makefile Makefile.local
	g++ -c $< -o $@ $(CXXFLAGS)
