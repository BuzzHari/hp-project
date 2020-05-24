#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <CL/opencl.h>
#include "bwtcl.h"


//#define BLOCKSIZE 1048576 // 1MB
#define BLOCKSIZE 4*1024
#define MINSIZE 1048576
#define MAX_SOURCE_SIZE 1024*1024 


typedef struct __attribute__((packed)) FIFO {
    int id;
    int len;
    char block[BLOCKSIZE];
} FIFO;

static void CallKernel(FIFO *infifo, FIFO *outfifo, int no_of_blocks, char* cl_filename, char* cl_kernelname);

int BwtTransform(FILE *fpIn, FILE *fpOut) {

    if( fpIn == NULL || fpOut == NULL) {
        printf("No file\n");
        exit(1);
    }

    FIFO *infifo;
    FIFO *outfifo;

    fseek(fpIn, 0, SEEK_END);
    long total_size = ftell(fpIn);
    printf("Total size: %ld\n", total_size);
    fseek(fpIn, 0, SEEK_SET);

    if(total_size <= MINSIZE) {
        printf("No use\n");
        return 0;
    }

    int bsize = BLOCKSIZE;
    int no_of_blocks = ceil((double)total_size / bsize);
    
    infifo = (FIFO*) malloc(sizeof(FIFO)*no_of_blocks);
    outfifo = (FIFO*) malloc(sizeof(FIFO)*no_of_blocks);

    for (int i = 0; i < no_of_blocks; ++i) {
        
        infifo[i].id = i;
        int result = fread(infifo[i].block, 1, bsize, fpIn);
        printf("Block %d: Size Read %d\n", i, result);
        if(result != bsize && i != no_of_blocks-1){
            printf("Reading error");
            free(infifo);
            free(outfifo);
            exit(3);
        }
        infifo[i].len = result;
    }

    CallKernel(infifo, outfifo, no_of_blocks, "transform.cl", "BwTransform"); 

    for (int i = 0; i < no_of_blocks; ++i) {
        for(int j = 0; j < outfifo[i].len; ++j)
            printf("%c", outfifo[i].block[j]); 
        printf("\nBlock End---------------------------------\n");
    }
    return 0;
}


static void CallKernel(FIFO *infifo, FIFO *outfifo, int no_of_blocks, char* cl_filename, char* cl_kernelname) {

    FILE *fp;
    char *kernel_source;
    size_t kernel_size;
    
    fp = fopen(cl_filename, "r");
    if(!fp) {
        fprintf(stderr, "Failed to load kernel.\n");
        exit(1);
    }

    kernel_source = (char *) malloc(sizeof(char) * MAX_SOURCE_SIZE);
    
    kernel_size = fread(kernel_source, sizeof(char), MAX_SOURCE_SIZE, fp);
    fclose(fp);

    cl_mem d_inf;
    cl_mem d_outf;

    cl_mem rotationIdx; // Index of first char in rotation.
    cl_mem v;           // Index of radix sorted characters.
    cl_mem last;        // last characters from sorted rotations.

    cl_platform_id cpPlatform;
    cl_device_id device_id;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;
    cl_int err;

    size_t bytes = sizeof(FIFO) * no_of_blocks;
    size_t global_size;
    size_t local_size = 128;
    printf("NO_of_blocks: %d\n", no_of_blocks);
    global_size = no_of_blocks * local_size;
    printf("Global Size: %lu\n", global_size);
    err = clGetPlatformIDs(1, &cpPlatform, NULL);

    if(err != CL_SUCCESS){
        perror("Couldn't get platforms\n");
        return;
    }
    
    err = clGetDeviceIDs(cpPlatform, CL_DEVICE_TYPE_GPU, 1, &device_id, NULL);
    if(err != CL_SUCCESS){
        perror("Couldn't get DeviceIDs \n");
        return;
    }

    context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &err);
    if(err != CL_SUCCESS) {
        perror("Problem creating context\n");
        printf("Error Code: %d\n", err);
        return;
    }
    
    queue = clCreateCommandQueueWithProperties(context, device_id, 0, &err);
    if(err != CL_SUCCESS) {
        perror("Problem creating command queue\n");
        printf("Error Code: %d\n", err);
        return;
    }

    program = clCreateProgramWithSource(context, 1, (const char **) & kernel_source,
            (const size_t *)& kernel_size, &err);
    if(err != CL_SUCCESS) {
        perror("Problem creating program\n");
        printf("Error Code: %d\n", err);
        return;
    }
    
    //char *options = (char*)malloc(sizeof(char)*10);
    //strcpy( options, "-g");
    
    err = clBuildProgram(program, 0, NULL,NULL, NULL, NULL);
     if(err != CL_SUCCESS) {
        perror("Problem building program executable.\n");
        printf("Error Code: %d\n", err);
        if(err == CL_BUILD_PROGRAM_FAILURE) {
            size_t log_siz;
            clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_siz );
            char *log = (char *) malloc(log_siz);
            clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, log_siz, log, NULL);
            printf("%s\n", log);
        }
        return;
     }


    kernel = clCreateKernel(program, cl_kernelname, &err);
    if(err != CL_SUCCESS) {
        perror("Problem creating kernel.\n");
        printf("Error Code: %d\n", err);
        return;
    }

    d_inf = clCreateBuffer(context, CL_MEM_READ_ONLY, bytes, NULL, &err);
    if(err != CL_SUCCESS) {
        perror("Problem creating buffer d_inf.\n");
        printf("Error Code: %d\n", err);
        return;
    }
    
    d_outf = clCreateBuffer(context, CL_MEM_READ_WRITE, bytes, NULL, &err);
    if(err != CL_SUCCESS) {
        perror("Problem creating buffer d_outf.\n");
        printf("Error Code: %d\n", err);
        return;
    }

    rotationIdx = clCreateBuffer(context, CL_MEM_READ_WRITE,
            BLOCKSIZE*sizeof(unsigned int)*no_of_blocks, NULL, &err);
    if(err != CL_SUCCESS) {
        perror("Problem creating buffer rotationIdx\n");
        printf("Error Code: %d\n", err);
        return;
    }
    

    v = clCreateBuffer(context, CL_MEM_READ_WRITE,
            BLOCKSIZE*sizeof(unsigned int)*no_of_blocks, NULL, &err);
    if(err != CL_SUCCESS) {
        perror("Problem creating buffer v\n");
        printf("Error Code: %d\n", err);
        return;
    }

    last = clCreateBuffer(context, CL_MEM_READ_WRITE,
            BLOCKSIZE*sizeof(unsigned char)*no_of_blocks, NULL, &err);
    if(err != CL_SUCCESS) {
        perror("Problem creating buffer last\n");
        printf("Error Code: %d\n", err);
        return;
    }

    err = clEnqueueWriteBuffer(queue, d_inf, CL_TRUE, 0, bytes, infifo, 0, NULL, NULL);
    if(err != CL_SUCCESS) {
        perror("Problem enqueing writes.\n");
        return;
    }


    err  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_inf);
    err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_outf);
    err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &rotationIdx);
    err |= clSetKernelArg(kernel, 3, sizeof(cl_mem), &v);
    err |= clSetKernelArg(kernel, 4, sizeof(cl_mem), &last);
    err |= clSetKernelArg(kernel, 5, sizeof(int), &no_of_blocks);
    if(err != CL_SUCCESS) {
        perror("Problem setting arguments.\n");
        printf("Error Code: %d\n", err);
        return;
    }
    
    err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size,
            &local_size, 0, NULL, NULL);
    if(err != CL_SUCCESS) {
        perror("Problem enqueing kernel.\n");
        printf("Error Code: %d\n", err);
        return;
    }

    err = clFinish(queue);
    if(err != CL_SUCCESS) {
        perror("Problem with CL Finish.\n");
        printf("Error Code: %d\n", err);
        return;
    }
    
    //FIFO *out = (FIFO*) malloc(sizeof(FIFO)*no_of_blocks);
    err = clEnqueueReadBuffer(queue, d_outf, CL_TRUE, 0, bytes, outfifo, 0, NULL,NULL);
    printf("Err: %d\n",err);
    if(err != CL_SUCCESS) {
        perror("Problem reading from buffer.\n");
        printf("Error Code: %d\n", err);
        return;
    }
    
/*
 *    printf("Outfifo.len: %d\n", out[0].len);
 *    for (int i = 0; i < no_of_blocks; ++i) {
 *        printf("%c", out[i].block[0]); 
 *        printf("\nBlock End---------------------------------\n");
 *    }
 *
 */
    clReleaseMemObject(d_inf);
    clReleaseMemObject(d_outf);
    clReleaseProgram(program);
    clReleaseKernel(kernel);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
}
