all: sample

sample: sample.o lzss.o optlist.o
	gcc -o sample $^ -lOpenCL -lm 

sample.o:  sample.c lzss.h optlist.h
	gcc -c $<

lzss.o: lzss.c
	gcc -c $< -lOpenCL -lm 

optlist.o: optlist.c optlist.h
	gcc -c $<

clean:
	- rm -f *.o sample 