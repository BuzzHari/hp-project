all: sample

run-transform: 
	./sample -c -i $(FILE) -o out

run-reverse: 
	./sample -d -i out -o rev.txt

sample: sample.o bwtcl.o optlist.o
	gcc -O3 -g -o sample $^ -lOpenCL -lm

sample.o: sample.c bwtcl.h optlist.h
	gcc -O3 -c -g $<

bwtcl.o: bwtcl.c transform.cl
	gcc -O3 -c -g $< -lOpenCL -lm 

optlist.o: optlist.c optlist.h
	gcc -O3 -c -g $<

clean:
	rm -f *.o
