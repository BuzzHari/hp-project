#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <limits.h>
#include <CL/opencl.h>
#include "bwtcl.h"


//#define BLOCK_SIZE 1048576 // 1MB
//#define BLOCK_SIZE 4096 // 4KB 
#define BLOCK_SIZE 16 // 16 bytes. For testing. 
#define MIN_SIZE 1048576
#define MAX_SOURCE_SIZE 1024*1024 

#define Wrap(value, limit) (((value) < (limit)) ? (value) : ((value) - (limit)))



typedef struct __attribute__((packed)) FIFO {
    int id;
    int len;
    unsigned int rotationIdx[BLOCK_SIZE];
    unsigned int v[BLOCK_SIZE];
    char block[BLOCK_SIZE];
} FIFO;

typedef struct __attribute__((packed)) LAST{
    int s0Idx;
    unsigned char last[BLOCK_SIZE];
} LAST;



static int ComparePresorted(const void *s1, const void *s2, void *blck)
{
    FIFO *block;
    block = (FIFO*) blck;
    unsigned int offset1, offset2;
    unsigned int i;
    unsigned int blockSize = block->len;
    /***********************************************************************
    * Compare 1 character at a time until there's difference or the end of
    * the block is reached.  Since we're only sorting strings that already
    * match at the first two characters, start with the third character.
    ***********************************************************************/
    offset1 = *((unsigned int *)s1) + 2;
    offset2 = *((unsigned int *)s2) + 2;
    for(i = 2; i < blockSize; i++)
    {
        unsigned char c1, c2;

        /* ensure that offsets are properly bounded */
        if (offset1 >= blockSize)
        {
            offset1 -= blockSize;
        }

        if (offset2 >= blockSize)
        {
            offset2 -= blockSize;
        }

        c1 = block->block[offset1];
        c2 = block->block[offset2];

        if (c1 > c2)
        {
            return 1;
        }
        else if (c2 > c1)
        {
            return -1;
        }

        /* strings match to here, try next character */
        offset1++;
        offset2++;
    }

    /* strings are identical */
    return 0;
}

static void CallKernel(FIFO *infifo, FIFO *outfifo, int no_of_blocks, char* cl_filename, char* cl_kernelname);

int BwtTransform(FILE *fpIn, FILE *fpOut) {

    if( fpIn == NULL || fpOut == NULL) {
        printf("No file\n");
        exit(1);
    }

    FIFO *infifo;
    FIFO *outfifo;
    LAST *last;

    fseek(fpIn, 0, SEEK_END);
    long total_size = ftell(fpIn);
    printf("Total size: %ld\n", total_size);
    fseek(fpIn, 0, SEEK_SET);

    /*
     *if(total_size <= MINSIZE) {
     *    printf("No use\n");
     *    return 0;
     *}
     */

    int bsize = BLOCK_SIZE;
    int no_of_blocks = ceil((double)total_size / bsize);
    //int no_of_blocks = 100;
    infifo = (FIFO*) malloc(sizeof(FIFO)*no_of_blocks);
    if(!infifo) {
        printf("Infifo: Malloc Error\n");
        exit(0);
    }
        
    outfifo = (FIFO*) malloc(sizeof(FIFO)*no_of_blocks);
    if(!infifo) {
        printf("Outfifo: Malloc Error\n");
        exit(0);
    }

    last = (LAST*) malloc(sizeof(LAST)*no_of_blocks);
    if(!infifo) {
        printf("Last: Malloc Error\n");
        exit(0);
    }
   
    for (int i = 0; i < 1; ++i) {
        
        infifo[i].id = i;
        int result = fread(infifo[i].block, 1, bsize, fpIn);
        printf("Block %d: Size Read %d\n", i, result);
        printf("Text: %c\n", infifo[i].block[14]);
        if(result != bsize && i != no_of_blocks-1){
            printf("Reading error\n");
            free(infifo);
            free(outfifo);
            exit(3);
        }
        infifo[i].len = result;
    }

    CallKernel(infifo, outfifo, no_of_blocks, "transform.cl", "BwTransform"); 

    for (int i = 0; i < no_of_blocks; ++i) {
        for (int j=0; j < outfifo[i].len; ++j)
            printf("rotationIdx[%d]: %d\n", j, outfifo[i].rotationIdx[j]);
    }
    
    // Not required anymore.
    free(infifo); 

    unsigned int i,j,k,l;
    for(l = 0; l < no_of_blocks; ++l) {
        unsigned int blockSize = outfifo[l].len;
        for(i = 0, k = 0; (i <= UCHAR_MAX) && (k < (blockSize -1)); ++i) {
            for (j = 0; (j <= UCHAR_MAX) && (k < (blockSize - 1)); j++) {
                unsigned int first = k;

               /* count strings starting with ij */
                while ((i == outfifo[l].block[outfifo[l].rotationIdx[k]]) &&
                    (j == outfifo[l].block[Wrap(outfifo[l].rotationIdx[k] + 1,  blockSize)])) {
                    k++;
                    if (k == blockSize) {
                        /* we've searched the whole block */
                        break;
                    }
                }
                if (k - first > 1) {
                    /* there are at least 2 strings staring with ij, sort them */
                    qsort_r(&outfifo[l].rotationIdx[first], k - first, sizeof(int),
                        ComparePresorted, &outfifo[l]);
                }
            }
        }
    }
    
    printf("After sorting:\n");

    for (int i = 0; i < no_of_blocks; ++i) {
        for (int j=0; j < outfifo[i].len; ++j)
            printf("rotationIdx[%d]: %d\n", j, outfifo[i].rotationIdx[j]);
    }

    // Okay! It's sorted now. Time to get the last characters of the rotations.
    
    //CallKernel(outfifo,last,no_of_blocks, "transform.cl", "BwLast"); 
    return 0;
}

static cl_kernel CreateKernel(char* cl_filename, char* cl_kernelname) {
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

    cl_platform_id cpPlatform;
    cl_device_id device_id;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;
    cl_int err;

    size_t bytes = sizeof(FIFO) * no_of_blocks;
    printf("bytes: %lu\n", sizeof(FIFO) * no_of_blocks);

    printf("%s", infifo[0].block);
    size_t global_size;
    //size_t local_size = 128;
    size_t local_size = 1;
    printf("No_of_blocks: %d\n", no_of_blocks);
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

    cl_context_properties properties[] = {CL_CONTEXT_PLATFORM,(cl_context_properties)cpPlatform,0};

    context = clCreateContext(properties, 1, &device_id, NULL, NULL, &err);
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

    d_inf = clCreateBuffer(context, CL_MEM_READ_WRITE, bytes, NULL, &err);
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

    err = clEnqueueWriteBuffer(queue, d_inf, CL_TRUE, 0, bytes, infifo, 0, NULL, NULL);
    if(err != CL_SUCCESS) {
        printf("Error Code: %d\n", err);
        perror("Problem enqueing writes.\n");
        return;
    }


    err  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_inf);
    err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_outf);
    err |= clSetKernelArg(kernel, 2, sizeof(unsigned int), &no_of_blocks);
    
    if(err != CL_SUCCESS) {
        perror("Problem setting arguments\n");
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
    err = clEnqueueReadBuffer(queue, d_inf, CL_TRUE, 0, bytes, outfifo, 0, NULL,NULL);
    printf("Err: %d\n",err);
    if(err != CL_SUCCESS) {
        perror("Problem reading from buffer.\n");
        printf("Error Code: %d\n", err);
        return;
    }
    

    // Now to quick sort on this data in main. 

    clReleaseMemObject(d_inf);
    clReleaseMemObject(d_outf);
    clReleaseProgram(program);
    clReleaseKernel(kernel);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
}
