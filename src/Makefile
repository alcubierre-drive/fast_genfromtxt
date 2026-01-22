CFLAGS := -Ofast -march=native -mtune=native -DNDEBUG -fPIC
CFLAGS += -Wall -Wextra -pedantic -std=c11 -fopenmp
SRC := genfromtxt_parallel.c genfromtxt_serial.c
HEADERS := genfromtxt_parallel.h genfromtxt_serial.h vector.h
libfast_genfromtxt.so: $(SRC) $(HEADERS)
	gcc $(SRC) -shared $(CFLAGS) -o libfast_genfromtxt.so
