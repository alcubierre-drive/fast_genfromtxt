CFLAGS := -Ofast -march=native -mtune=native 
CFLAGS += -Wall -Wextra -pedantic -std=c11
libfast_genfromtxt.so: fast_genfromtxt.c fast_genfromtxt.h
	gcc fast_genfromtxt.c -shared $(CFLAGS) -o libfast_genfromtxt.so
