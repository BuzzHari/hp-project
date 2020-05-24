all: sample

run: 
	./sample -c -i $(FILE) -o out.txt

gdb-run: 
	gdb ./sample -c -i ../../testdata/world192.txt 

sample: sample.o bwtcl.o optlist.o
	gcc -g -o sample $^ -lOpenCL -lm

sample.o: sample.c bwtcl.h optlist.h
	gcc -c -g $<

bwtcl.o: bwtcl.c transform.cl
	gcc -c -g $< -lOpenCL -lm 

optlist.o: optlist.c optlist.h
	gcc -c -g $<

clean:
	rm -f *.o
