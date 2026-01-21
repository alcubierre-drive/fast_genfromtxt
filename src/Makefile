CFLAGS := -Ofast -march=native -mtune=native -DNDEBUG -fPIC
CFLAGS += -Wall -Wextra -pedantic -std=c11 -fopenmp
libfast_genfromtxt.so: fast_genfromtxt.c fast_buffromtxt.c fast_genfromtxt.h
	gcc fast_genfromtxt.c fast_buffromtxt.c -shared $(CFLAGS) -o libfast_genfromtxt.so
