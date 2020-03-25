main: main.c culzss  gpu_compress deculzss  gpu_decompress decompression
	gcc -g -o main main.c culzss.o gpu_compress.o deculzss.o gpu_decompress.o decompression.o -lpthread -L /usr/lib/x86_64-linux-gnu/ -lcudart -lstdc++

decompression: 	decompression.c decompression.h
	gcc -g -c -o decompression.o decompression.c -lpthread 
	
culzss:  culzss.c culzss.h 
	gcc -g -c -o culzss.o culzss.c -lpthread

gpu_compress: gpu_compress.cu gpu_compress.h
	nvcc -O3 -g -c -arch sm_30  -o gpu_compress.o gpu_compress.cu -lpthread      

deculzss:  deculzss.c deculzss.h 
	gcc -g -c -o deculzss.o deculzss.c -lpthread

gpu_decompress: gpu_decompress.cu gpu_decompress.h
	nvcc -c -g -o gpu_decompress.o gpu_decompress.cu -lpthread  
	
clean:
	rm *.o

