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
#define BLOCK_SIZE 4096 // 4KB 
//#define BLOCK_SIZE 2048 // 4KB 
//#define BLOCK_SIZE 102400*5// 4KB 
//#define BLOCK_SIZE 16 // 16 bytes. For testing. 
#define MIN_SIZE 1048576
#define MAX_SOURCE_SIZE 1024*1024 

#define LOCAL_SIZE 64 

#define Wrap(value, limit) (((value) < (limit)) ? (value) : ((value) - (limit)))



typedef struct __attribute__((packed)) FIFO {
    unsigned long int id;
    unsigned long int len;
    unsigned int rotationIdx[BLOCK_SIZE];
    unsigned int v[BLOCK_SIZE];
    unsigned char block[BLOCK_SIZE];
} FIFO;


typedef struct __attribute__((packed)) r_FIFO {
    unsigned long int id;

    unsigned int s0Idx;
    unsigned long int len;
    unsigned int pred[BLOCK_SIZE];
    unsigned char unrotated[BLOCK_SIZE];
    unsigned char block[BLOCK_SIZE];
} r_FIFO;

typedef struct __attribute__((packed)) LAST{
    unsigned int s0Idx;
    unsigned char last[BLOCK_SIZE];
} LAST;



static int ComparePresorted(const void *s1, const void *s2, void *blck)
{
    FIFO *block;
    block = (FIFO*) blck;
    unsigned int offset1, offset2;
    unsigned long int i;
    unsigned long int blockSize = block->len;
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


static void CallTransformKernel(cl_kernel kernel, FIFO *infifo, FIFO *outfifo, unsigned long int no_of_blocks);

static cl_kernel CreateKernel(char* cl_filename, char* cl_kernelname);

static void CallLastKernel(cl_kernel kernel, FIFO *outfifo, LAST *last, unsigned long int no_of_blocks); 

static void CallReverseKernel(cl_kernel, r_FIFO *infifo, unsigned long int no_of_blocks);

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
    //printf("Total size: %ld\n", total_size);
    fseek(fpIn, 0, SEEK_SET);

    /*
     *if(total_size <= MINSIZE) {
     *    printf("No use\n");
     *    return 0;
     *}
     */

    unsigned long int bsize = BLOCK_SIZE;
    unsigned long int no_of_blocks = ceil((double)total_size / bsize);
    //int no_of_blocks = 100;
    infifo = (FIFO*) malloc(sizeof(FIFO)*no_of_blocks);
    if(!infifo) {
        printf("Infifo: Malloc Error\n");
        exit(0);
    }
        
    outfifo = (FIFO*) malloc(sizeof(FIFO)*no_of_blocks);
    if(!outfifo) {
        printf("Outfifo: Malloc Error\n");
        exit(0);
    }

    last = (LAST*) malloc(sizeof(LAST)*no_of_blocks);
    if(!last) {
        printf("Last: Malloc Error\n");
        exit(0);
    }
    
    unsigned long int i,j,k,l;
    
    for (i = 0; i < no_of_blocks; ++i) {
        
        infifo[i].id = i;
        unsigned long int result = fread(infifo[i].block, 1, bsize, fpIn);
        //printf("Block %ld: Size Read %ld\n", i, result);
        //printf("Text: %c\n", infifo[i].block[14]);
        if(result != bsize && i != no_of_blocks-1){
            printf("Reading error\n");
            free(infifo);
            free(outfifo);
            exit(3);
        }
        infifo[i].len = result;
    }

    cl_kernel kernel = CreateKernel("transform.cl", "BwTransform");

    if(!kernel) {
        printf("Something went wrong in creating the kernel\n");
        exit(0);
    }

    CallTransformKernel(kernel, infifo, outfifo, no_of_blocks); 

    /*
     *for (int i = 0; i < no_of_blocks; ++i) {
     *    for (int j=0; j < outfifo[i].len; ++j)
     *        printf("rotationIdx[%d]: %d\n", j, outfifo[i].rotationIdx[j]);
     *}
     */
    
    // Not required anymore.
    free(infifo); 

    for(l = 0; l < no_of_blocks; ++l) {
        unsigned long int blockSize = outfifo[l].len;
        for(i = 0, k = 0; (i <= UCHAR_MAX) && (k < (blockSize -1)); ++i) {
            for (j = 0; (j <= UCHAR_MAX) && (k < (blockSize - 1)); j++) {
                unsigned long int first = k;

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
    
/*
 *    printf("After sorting:\n");
 *
 *    for (int i = 0; i < no_of_blocks; ++i) {
 *        for (int j=0; j < outfifo[i].len; ++j)
 *            printf("rotationIdx[%d]: %d\n", j, outfifo[i].rotationIdx[j]);
 *    }
 */

    // Okay! It's sorted now. Time to get the last characters of the rotations.
    
    kernel = CreateKernel("transform.cl", "BwLast");

    CallLastKernel(kernel, outfifo, last, no_of_blocks); 

    for (int i = 0; i < no_of_blocks; ++i) {
        //printf("s0Idx: %d\n", last[i].s0Idx);
        //for (int j=0; j < outfifo[i].len; ++j)
            //printf("last[%d]: %c\n", j, last[i].last[j]);
        
        fwrite(&last[i].s0Idx, sizeof(int), 1, fpOut);
        fwrite(last[i].last, sizeof(unsigned char), outfifo[i].len, fpOut);
    }

    free(outfifo);
    free(last);
    return 0;
}


int BwtReverse(FILE *fpIn, FILE *fpOut) {

    if( fpIn == NULL || fpOut == NULL) {
        printf("No file\n");
        exit(1);
    }

    r_FIFO *infifo;

    fseek(fpIn, 0, SEEK_END);
    long total_size = ftell(fpIn);
    //printf("Total size: %ld\n", total_size);
    fseek(fpIn, 0, SEEK_SET);


    unsigned long int bsize = BLOCK_SIZE;
    unsigned long int no_of_blocks = ceil((double)total_size / bsize);
    //int no_of_blocks = 100;
    infifo = (r_FIFO*) malloc(sizeof(r_FIFO)*no_of_blocks);
    if(!infifo) {
        printf("Infifo: Malloc Error\n");
        exit(0);
    }

        
    
    unsigned long int i,j,k,l;
    
    for (i = 0; i < no_of_blocks; ++i) {
        
        infifo[i].id = i;
        int ret = fread(&infifo[i].s0Idx, sizeof(int), 1, fpIn);
        //printf("s0Idx: %u", infifo[i].s0Idx);
        unsigned long int result = fread(infifo[i].block, sizeof(unsigned char), bsize, fpIn);
        //printf("Block %ld: Size Read %ld\n", i, result);
        //printf("Text: %s\n", infifo[i].block);
        /*if(result != bsize && i != no_of_blocks-1){*/
            /*printf("Reading error\n");*/
            /*free(infifo);*/
            /*exit(3);*/
        /*}*/
        infifo[i].len = result;
    }

    cl_kernel kernel = CreateKernel("reverse.cl", "BwtReverse");

    if(!kernel) {
        printf("Something went wrong in creating the kernel\n");
        exit(0);
    }

    CallReverseKernel(kernel, infifo, no_of_blocks); 

    for (int i = 0; i < no_of_blocks; ++i) {
        //printf("s0Idx: %d\n", last[i].s0Idx);
        //for (int j=0; j < outfifo[i].len; ++j)
            //printf("last[%d]: %c\n", j, last[i].last[j]);
        
        fwrite(infifo[i].unrotated, sizeof(unsigned char), infifo[i].len, fpOut);
    }

    free(infifo);
    return 0;
}

static cl_kernel CreateKernel(char* cl_filename, char* cl_kernelname) {
    
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

    //cl_mem d_inf;
    //cl_mem d_outf;

    cl_platform_id cpPlatform;
    cl_device_id device_id;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;
    cl_int err;

    /*size_t global_size;*/
    /*//size_t local_size = 128;*/
    /*size_t local_size = 1;*/
    /*printf("No_of_blocks: %d\n", no_of_blocks);*/
    /*global_size = no_of_blocks * local_size;*/
    /*printf("Global Size: %lu\n", global_size);*/
    
    err = clGetPlatformIDs(1, &cpPlatform, NULL);

    if(err != CL_SUCCESS){
        perror("Couldn't get platforms\n");
        return NULL;
    }
    
    err = clGetDeviceIDs(cpPlatform, CL_DEVICE_TYPE_GPU, 1, &device_id, NULL);
    if(err != CL_SUCCESS){
        perror("Couldn't get DeviceIDs \n");
        return NULL;
    }

    cl_context_properties properties[] = {CL_CONTEXT_PLATFORM,(cl_context_properties)cpPlatform,0};

    context = clCreateContext(properties, 1, &device_id, NULL, NULL, &err);
    if(err != CL_SUCCESS) {
        perror("Problem creating context\n");
        printf("Error Code: %d\n", err);
        return NULL;
    }
    
    queue = clCreateCommandQueueWithProperties(context, device_id, 0, &err);
    if(err != CL_SUCCESS) {
        perror("Problem creating command queue\n");
        printf("Error Code: %d\n", err);
        return NULL;
    }

    program = clCreateProgramWithSource(context, 1, (const char **) & kernel_source,
            (const size_t *)& kernel_size, &err);
    if(err != CL_SUCCESS) {
        perror("Problem creating program\n");
        printf("Error Code: %d\n", err);
        return NULL;
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
        return NULL;
     }

    kernel = clCreateKernel(program, cl_kernelname, &err);
    if(err != CL_SUCCESS) {
        perror("Problem creating kernel.\n");
        printf("Error Code: %d\n", err);
        return NULL;
    }

    clReleaseCommandQueue(queue);
    return kernel;
}


static void CallTransformKernel(cl_kernel kernel, FIFO *infifo, FIFO *outfifo, unsigned long int no_of_blocks) {
    

    cl_mem d_inf;

    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_device_id device_id;
    cl_int err;

    size_t bytes = sizeof(FIFO) * no_of_blocks;
    //printf("bytes: %lu\n", sizeof(FIFO) * no_of_blocks);

    size_t global_size;
    //size_t local_size = 128;
    size_t local_size = LOCAL_SIZE;
    //printf("No_of_blocks: %ld\n", no_of_blocks);
    //global_size = no_of_blocks * local_size;
    global_size = ceil((float)no_of_blocks / local_size) * local_size;
    //printf("Global Size: %lu\n", global_size);
    

    err = clGetKernelInfo(kernel,CL_KERNEL_CONTEXT, sizeof(cl_context), &context, NULL);
    if(err != CL_SUCCESS) {
        perror("Problem getting context\n");
        printf("Error Code: %d\n", err);
        return;
    }


    err = clGetKernelInfo(kernel,CL_KERNEL_PROGRAM, sizeof(cl_program), &program, NULL);
    if(err != CL_SUCCESS) {
        perror("Problem getting program\n");
        printf("Error Code: %d\n", err);
        return;
    }
    
    err = clGetProgramInfo(program, CL_PROGRAM_DEVICES, sizeof(cl_device_id), &device_id, NULL);
    if(err != CL_SUCCESS) {
        perror("Problem getting deviceID\n");
        printf("Error Code: %d\n", err);
        return;
    }

    queue = clCreateCommandQueueWithProperties(context, device_id, 0, &err);
    if(err != CL_SUCCESS) {
        perror("Problem creating command queue\n");
        printf("Error Code: %d\n", err);
        return;
    }
    
    d_inf = clCreateBuffer(context, CL_MEM_READ_WRITE, bytes, NULL, &err);
    if(err != CL_SUCCESS) {
        perror("Problem creating buffer d_inf.\n");
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
    if(err != CL_SUCCESS) {
        perror("Problem setting arguments: 1\n");
        printf("Error Code: %d\n", err);
        return;
    }
    
    err |= clSetKernelArg(kernel, 1, sizeof(unsigned long int), &no_of_blocks);
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
    //printf("Err: %d\n",err);
    if(err != CL_SUCCESS) {
        perror("Problem reading from buffer.\n");
        printf("Error Code: %d\n", err);
        return;
    }
    

    // Now to quick sort on this data in main. 

    clReleaseMemObject(d_inf);
    clReleaseProgram(program);
    clReleaseKernel(kernel);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
}



static void CallLastKernel(cl_kernel kernel, FIFO *infifo, LAST *last, unsigned long int no_of_blocks) {
    
    cl_mem d_inf;
    cl_mem d_outf;

    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_device_id device_id;
    cl_int err;

    size_t bytes = sizeof(FIFO) * no_of_blocks;
    size_t lbytes = sizeof(LAST) * no_of_blocks;
    //printf("bytes: %lu\n", bytes);
    //printf("lbytes: %lu\n", lbytes);
    
    size_t global_size;
    size_t local_size = LOCAL_SIZE;
    //size_t local_size = 1;
    //printf("No_of_blocks: %ld\n", no_of_blocks);
    //global_size = no_of_blocks * local_size;
    global_size = ceil((float)no_of_blocks / local_size) * local_size;
    //printf("Global Size: %lu\n", global_size);
    

    err = clGetKernelInfo(kernel,CL_KERNEL_CONTEXT, sizeof(cl_context), &context, NULL);
    if(err != CL_SUCCESS) {
        perror("Problem getting context\n");
        printf("Error Code: %d\n", err);
        return;
    }


    err = clGetKernelInfo(kernel,CL_KERNEL_PROGRAM, sizeof(cl_program), &program, NULL);
    if(err != CL_SUCCESS) {
        perror("Problem getting program\n");
        printf("Error Code: %d\n", err);
        return;
    }
    
    err = clGetProgramInfo(program, CL_PROGRAM_DEVICES, sizeof(cl_device_id), &device_id, NULL);
    if(err != CL_SUCCESS) {
        perror("Problem getting deviceID\n");
        printf("Error Code: %d\n", err);
        return;
    }

    queue = clCreateCommandQueueWithProperties(context, device_id, 0, &err);
    if(err != CL_SUCCESS) {
        perror("Problem creating command queue\n");
        printf("Error Code: %d\n", err);
        return;
    }
    
    d_inf = clCreateBuffer(context, CL_MEM_READ_ONLY, bytes, NULL, &err);
    if(err != CL_SUCCESS) {
        perror("Problem creating buffer d_inf.\n");
        printf("Error Code: %d\n", err);
        return;
    }


    d_outf = clCreateBuffer(context, CL_MEM_READ_WRITE, lbytes, NULL, &err);
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
    err |= clSetKernelArg(kernel, 2, sizeof(unsigned long int), &no_of_blocks);
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
    err = clEnqueueReadBuffer(queue, d_outf, CL_TRUE, 0, lbytes, last, 0, NULL,NULL);
    //printf("Err: %d\n",err);
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


static void CallReverseKernel(cl_kernel kernel, r_FIFO *infifo, unsigned long int no_of_blocks) {
    

    cl_mem d_inf;

    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_device_id device_id;
    cl_int err;

    size_t bytes = sizeof(r_FIFO) * no_of_blocks;
    //printf("bytes: %lu\n", bytes);

    size_t global_size;
    //size_t local_size = 128;
    size_t local_size = LOCAL_SIZE;
    //printf("No_of_blocks: %ld\n", no_of_blocks);
    //global_size = no_of_blocks * local_size;
    global_size = ceil((float)no_of_blocks / local_size) * local_size;
    //printf("Global Size: %lu\n", global_size);
    

    err = clGetKernelInfo(kernel,CL_KERNEL_CONTEXT, sizeof(cl_context), &context, NULL);
    if(err != CL_SUCCESS) {
        perror("Problem getting context\n");
        printf("Error Code: %d\n", err);
        return;
    }


    err = clGetKernelInfo(kernel,CL_KERNEL_PROGRAM, sizeof(cl_program), &program, NULL);
    if(err != CL_SUCCESS) {
        perror("Problem getting program\n");
        printf("Error Code: %d\n", err);
        return;
    }
    
    err = clGetProgramInfo(program, CL_PROGRAM_DEVICES, sizeof(cl_device_id), &device_id, NULL);
    if(err != CL_SUCCESS) {
        perror("Problem getting deviceID\n");
        printf("Error Code: %d\n", err);
        return;
    }

    queue = clCreateCommandQueueWithProperties(context, device_id, 0, &err);
    if(err != CL_SUCCESS) {
        perror("Problem creating command queue\n");
        printf("Error Code: %d\n", err);
        return;
    }
    
    d_inf = clCreateBuffer(context, CL_MEM_READ_WRITE, bytes, NULL, &err);
    if(err != CL_SUCCESS) {
        perror("Problem creating buffer d_inf.\n");
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
    if(err != CL_SUCCESS) {
        perror("Problem setting arguments: 1\n");
        printf("Error Code: %d\n", err);
        return;
    }
    
    err |= clSetKernelArg(kernel, 1, sizeof(unsigned long int), &no_of_blocks);
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
    err = clEnqueueReadBuffer(queue, d_inf, CL_TRUE, 0, bytes, infifo, 0, NULL,NULL);
    //printf("Err: %d\n",err);
    if(err != CL_SUCCESS) {
        perror("Problem reading from buffer.\n");
        printf("Error Code: %d\n", err);
        return;
    }
    

    // Now to quick sort on this data in main. 

    clReleaseMemObject(d_inf);
    clReleaseProgram(program);
    clReleaseKernel(kernel);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
}
