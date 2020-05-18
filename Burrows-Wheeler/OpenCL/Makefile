all: sample

run: 
	./sample -c -i ../../testdata/world192.txt -o out.txt

sample: sample.o bwtcl.o optlist.o
	gcc -o sample $^ -lOpenCL -lm

sample.o: sample.c bwtcl.h optlist.h
	gcc -c $<

bwtcl.o: bwtcl.c transform.cl
	gcc -c $< -lOpenCL -lm 

optlist.o: optlist.c optlist.h
	gcc -c $<

clean:
	rm -f *.o
