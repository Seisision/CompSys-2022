#GCC=gcc -g -Wall -Wextra -pedantic -std=gnu11
GCC=gcc -g -Wall -Wextra -pedantic -std=gnu11
LD_FLAGS= -lpthread -lm

all: sim

rebuild: clean all

assembly.o: assembly.c assembly.h
	$(GCC) -c $< -o $@

memory.o: memory.c memory.h
	$(GCC) -c $< -o $@

read_exec.o: read_exec.c read_exec.h
	$(GCC) -c $< -o $@
	
simulate.o: simulate.c simulate.h
	$(GCC) -c $< -o $@

sim: main.c simulate.o memory.o assembly.o read_exec.o
	$(GCC) $< *.o -o $@ $(LD_FLAGS)

zip: ../src.zip

../src.zip: clean
	cd .. && zip -r src.zip src/Makefile src/*.c src/*.h

clean:
	rm -rf *.o sim 

#vgcore*
