#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <CL/opencl.h>
#include <math.h>

#define MAX_KERNEL_SIZE 1024*1024

int* g_symbolsOut;
int* g_countsOut;
int* g_in;
int* g_decompressed;
int* g_totalRuns;
int *h_backwardMask;
int *h_scannedBackwardMask;


cl_kernel maskKernel, compactKernel, scatterKernel;

const int NUM_TESTS = 4;
const unsigned int Tests[NUM_TESTS] = {
    8388608, // 8MB
    16777216, // 16MB
    33554432, // 32MB
    67108864 // 64MB
};

const int PROFILING_TESTS = 1;
const int MAX_N = 1 << 26;

int rleCpu(int *in, int n,
    int* symbolsOut,
    int* countsOut);

int* generateCompressibleRandomData(int n){
    int val = rand() % 10;

    for (int i = 0; i < n; ++i) {
        g_in[i] = val;

        if (rand() % 6 == 0){
            val = rand() % 10;
        }
    }
    return g_in;
}
char errorString[256];
bool verifyCompression(
    int* original, int n,
    int* compressedSymbols, int* compressedCounts, int totalRuns){

    // decompress.
    int j = 0;

    int sum = 0;
    for (int i = 0; i < totalRuns; ++i) {
        sum += compressedCounts[i];
    }

    if (sum != n) {
        sprintf(errorString, "Decompressed and original size not equal %d != %d\n", n, sum);
        return false;
    }

    for (int i = 0; i < totalRuns; ++i){
        int symbol = compressedSymbols[i];
        int count = compressedCounts[i];

        for (int k = 0; k < count; ++k){
            g_decompressed[j++] = symbol;
        }
    }

    // verify the compression.
    for (int i = 0; i < n; ++i) {
        if (original[i] != g_decompressed[i]){

            sprintf(errorString, "Decompressed and original not equal at %d, %d != %d\n", i, original[i], g_decompressed[i]);
            return false;
        }
    }

    return true;
}


void unitTest(int* in, int n, int totalRuns, bool verbose)
{
    if (verbose) {
        printf("n = %d\n", n);
        printf("Original Size  : %d\n", n);
        printf("Compressed Size: %d\n\n", totalRuns * 2);
    }

    if(!verifyCompression(in, n, g_symbolsOut, g_countsOut, totalRuns)){
        printf("Failed test %s\n", errorString);
        exit(1);
    }else{
        if (verbose)
            printf("passed test!\n\n");
    }
}


template<typename F, typename G>
void profileCpu(F rle, G dataGen) {
    for (int i = 0; i < NUM_TESTS; ++i) {
        int n = Tests[i];
        int* in = dataGen[i];

	clock_t t;

        for (int i = 0; i < PROFILING_TESTS; ++i) {
            t = clock();
            int totalRuns = rle(in, n, g_symbolsOut, g_countsOut);
            t = clock() - t;

            std::cout << n << " BYTES compressed in " << t << " microseconds\n";
            std::cout << "Original Size: " << n << "\n";
            std::cout << "Compressed Size: " <<  totalRuns * 2 << "\n\n";
        }


    }
}

void fillPrefixSum(int *arr, int n, int *prefixSum)
{
    prefixSum[0] = arr[0];

    for (int i = 1; i < n; i++)
        prefixSum[i] = prefixSum[i-1] + arr[i];

}

size_t local_size, global_size;


int success(cl_int err, const char *msg) {
    if (err != CL_SUCCESS) {
        printf("%s: %s\n", msg, err);
        return 0;
    }
    return 1;
}


int parleDevice(cl_context context, cl_program program, cl_command_queue queue, int *in, int n){

    cl_mem d_symbolsOut;
    cl_mem d_countsOut;
    cl_mem d_in;
    cl_mem d_totalRuns;
    cl_mem d_backwardMask;
    cl_mem d_scannedBackwardMask;
    cl_mem d_compactedBackwardMask;

    clock_t t;

    h_backwardMask = (int*)malloc(sizeof(int)*n);
    h_scannedBackwardMask = (int*)malloc(sizeof(int)*n);
    cl_int err;
    local_size = 1024;
    global_size =  (ceil(n/local_size))*local_size;

    d_in = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int) * n, NULL, &err);
    if (!success(err, "clCreateBuffer")) { return 0; }

    d_backwardMask = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int) * n, NULL, &err);
    if (!success(err, "clCreateBuffer")) { return 0; }

    maskKernel = clCreateKernel(program, "maskKernel", &err);
    if (!success(err, "clCreateKernel")) { return 0; }

    err = clSetKernelArg(maskKernel, 0, sizeof(cl_mem), &d_in);
    err |= clSetKernelArg(maskKernel, 1, sizeof(cl_mem), &d_backwardMask);
    err |= clSetKernelArg(maskKernel, 2, sizeof(unsigned int), &n);
    if (!success(err, "clSetKernelArg")) { return 0; }

    err = clEnqueueWriteBuffer(queue, d_in, CL_TRUE, 0, sizeof(int) * n, in, 0, NULL, NULL);
    if (!success(err, "clEnqueueWriteBuffer")) { return 0; }

    t = clock();

    err = clEnqueueNDRangeKernel(queue, maskKernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
    /* if (!success(err, "clEnqueueNDRangeKernel")) { return 0; } */
    /* err = clFinish(queue); */
    /* if (!success(err, "clFinish")) { return 0; } */
    err = clEnqueueReadBuffer(queue, d_backwardMask, CL_TRUE, 0, sizeof(int) * n, h_backwardMask, 0, NULL, NULL);
    /* if (!success(err, "clEnqueueReadBuffer")) { return 0; } */
    /* err = clFinish(queue); */
    /* if (!success(err, "clFinish")) { return 0; } */

    /* for(int i = 65520; i < 65536; ++i) */
    /*     printf("in: %d, mask: %d\n", in[i], h_backwardMask[i]); */

    /* !----------BACKWARD MASK COMPLETED----------! */

    fillPrefixSum(h_backwardMask, n, h_scannedBackwardMask);

    /* int inds[] = {0, 8192, 16384, 24576, 32768, 40960, 49152, 57344, 65535}; */
    /* printf("Start\n"); */
    /* for(int i = 0; i < 8; ++i) */
    /*     printf("%d\n", h_scannedBackwardMask[inds[i]]); */
    /* printf("Stop\n"); */


    /* !----------SCANNED BACKWARD MASK COMPLETED----------! */

    compactKernel = clCreateKernel(program, "compactKernel", &err);
    /* if (!success(err, "clCreateKernel")) { return 0; } */

    d_scannedBackwardMask = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int)*n, NULL, &err);
    /* if (!success(err, "clCreateBuffer")) { return 0; } */

    d_compactedBackwardMask = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int)*n, NULL, &err);
    /* if (!success(err, "clCreateBuffer")) { return 0; } */

    err = clEnqueueWriteBuffer(queue, d_scannedBackwardMask, CL_TRUE, 0, sizeof(int)*n, h_scannedBackwardMask, 0, NULL, NULL);
    /* if (!success(err, "clEnqueueWriteBuffer")) { return 0; } */

    d_totalRuns = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int), NULL, &err);
    /* if (!success(err, "clCreateBuffer")) { return 0; } */

    err = clSetKernelArg(compactKernel, 0, sizeof(cl_mem), &d_in);
    err |= clSetKernelArg(compactKernel, 1, sizeof(cl_mem), &d_scannedBackwardMask);
    err |= clSetKernelArg(compactKernel, 2, sizeof(cl_mem), &d_compactedBackwardMask);
    err |= clSetKernelArg(compactKernel, 3, sizeof(cl_mem), &d_totalRuns);
    err |= clSetKernelArg(compactKernel, 4, sizeof(unsigned int), &n);
    /* if (!success(err, "clSetKernelArg")) { return 0; } */

    err = clEnqueueNDRangeKernel(queue, compactKernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
    /* if (!success(err, "clEnqueueNDRangeKernel")) { return 0; } */

    /* err = clFinish(queue); */
    /* if (!success(err, "clFinish")) { return 0; } */

    /* int *h = (int*)malloc(sizeof(int) * n); */
    /* err = clEnqueueReadBuffer(queue, d_compactedBackwardMask, CL_TRUE, 0, sizeof(int) * n, h, 0, NULL, NULL); */
    /* if (!success(err, "clEnqueueReadBuffer")) { return 0; } */
    /* int inds[] = {0, 8192, 16384, 24576, 32768, 40960, 49152, 57344, 65535}; */
    /* printf("Start\n"); */
    /* for(int i = 0; i < 8; ++i) */
    /*     printf("%d\n", h[inds[i]]); */
    /* printf("Stop\n"); */

    g_totalRuns = (int*)malloc(sizeof(int));
    err = clEnqueueReadBuffer(queue, d_totalRuns, CL_TRUE, 0, sizeof(int), g_totalRuns, 0, NULL, NULL);
    /* if (!success(err, "clEnqueueReadBuffer")) { return 0; } */
    /* printf("runs: %d\n", g_totalRuns[0]); */


    /* !----------COMPACT BACKWARD MASK COMPLETED----------! */


    d_symbolsOut = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int) * g_totalRuns[0], NULL, &err);
    /* if (!success(err, "clCreateBuffer")) { return 0; } */

    d_countsOut = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int) * g_totalRuns[0], NULL, &err);
    /* if (!success(err, "clCreateBuffer")) { return 0; } */

    scatterKernel = clCreateKernel(program, "scatterKernel", &err);
    /* if (!success(err, "clCreateKernel")) { return 0; } */

    err = clSetKernelArg(scatterKernel, 0, sizeof(cl_mem), &d_compactedBackwardMask);
    err |= clSetKernelArg(scatterKernel, 1, sizeof(cl_mem), &d_totalRuns);
    err |= clSetKernelArg(scatterKernel, 2, sizeof(cl_mem), &d_in);
    err |= clSetKernelArg(scatterKernel, 3, sizeof(cl_mem), &d_symbolsOut);
    err |= clSetKernelArg(scatterKernel, 4, sizeof(cl_mem), &d_countsOut);
    /* if (!success(err, "clSetKernelArg")) { return 0; } */

    err = clEnqueueNDRangeKernel(queue, scatterKernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
    /* if (!success(err, "clEnqueueNDRangeKernel")) { return 0; } */

    /* err = clFinish(queue); */
    /* if (!success(err, "clFinish")) { return 0; } */

    t = clock() - t;

    g_symbolsOut = (int*) malloc (sizeof(int) * g_totalRuns[0]);
    g_countsOut = (int*) malloc (sizeof(int) * n);
    /* g_totalRuns = (int*) malloc (sizeof(int)); */


    err = clEnqueueReadBuffer(queue, d_symbolsOut, CL_TRUE, 0, sizeof(int) * g_totalRuns[0], g_symbolsOut, 0, NULL, NULL);
    /* if (!success(err, "clEnqueueReadBuffer")) { return 0; } */
    err = clFinish(queue);
    /* if (!success(err, "clFinish")) { return 0; } */

    /* int inds[] = {0, 12, 312, 1324, 3453, 4564, 6786, 10098}; */
    /* printf("Start\n"); */
    /* for(int i = 0; i < 8; ++i) */
    /*     printf("%d\n", g_symbolsOut[inds[i]]); */
    /* printf("Stop\n"); */

    err = clEnqueueReadBuffer(queue, d_countsOut, CL_TRUE, 0, sizeof(int) * g_totalRuns[0], g_countsOut, 0, NULL, NULL);
    /* if (!success(err, "clEnqueueReadBuffer")) { return 0; } */
    err = clFinish(queue);
    /* if (!success(err, "clFinish")) { return 0; } */

    clReleaseMemObject(d_in);
    clReleaseMemObject(d_countsOut);
    clReleaseMemObject(d_backwardMask);
    clReleaseMemObject(d_compactedBackwardMask);
    clReleaseMemObject(d_scannedBackwardMask);
    clReleaseMemObject(d_symbolsOut);
    clReleaseMemObject(d_totalRuns);
    return t;
}


template<typename G>
void profileGpu(G dataGen, cl_context context, cl_program program, cl_command_queue queue, int i) {

    int n;
    int *in;
    int time;
    for(int i = 0; i < NUM_TESTS; ++i){
n = Tests[i];
in = dataGen[i];
        time = parleDevice(context, program, queue, dataGen[i], Tests[i]);
        unitTest(in, n, g_totalRuns[0], false);

        std::cout << n << " BYTES compressed in " << time << " microseconds\n";
        std::cout << "Original Size: " << n << "\n";
        std::cout << "Compressed Size: " <<  g_totalRuns[0] * 2 << "\n\n";
    }
}


int main() {
    srand(1000);

    g_in = new int[MAX_N];
    g_decompressed = new int[MAX_N];
    g_symbolsOut = new int[MAX_N];
    g_countsOut = new int[MAX_N];

    int* data[NUM_TESTS];
    for (int z = 0; z < NUM_TESTS; z++){
        data[z] = generateCompressibleRandomData(Tests[z]);
    }
    /* printf("0,20,3421,65535: %d\t%d\t%d\t%d\n", data[0][0], data[0][20], data[0][3421], data[0][65535]); */

    std::cout << "\033[1;31mProfile CPU\033[0m\n";
    profileCpu(rleCpu, data);

    /* std::cout << "\n\n"; */

    std::cout << "\033[1;31mProfile GPU\033[0m\n";
    char *kernel_source = (char*) malloc(sizeof(char) * MAX_KERNEL_SIZE);
    size_t kernel_size;
    FILE *fp = fopen("main.cl", "r");
    if(!fp) {
        perror("file error\n");
        return 1;
    }

    kernel_size = fread(kernel_source, sizeof(char), MAX_KERNEL_SIZE, fp);
    fclose(fp);

    cl_platform_id *platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_int err;
    cl_uint platforms;

    err = clGetPlatformIDs(0, NULL, &platforms);
    if (!success(err, "")) { return 1; }


    platform = (cl_platform_id*) malloc(sizeof(cl_platform_id)*platforms);
    err = clGetPlatformIDs(platforms, platform, NULL);
    if (!success(err, "")) { return 1; }

    err = clGetDeviceIDs(platform[0], CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    if (!success(err, "")) { return 1; }


    context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    if (!success(err, "")) { return 1; }


    queue = clCreateCommandQueueWithProperties(context, device, 0, &err);
    if (!success(err, "")) { return 1; }


    program = clCreateProgramWithSource(context, 1, (const char **)& kernel_source,
            (const size_t *)& kernel_size, &err);
    if (!success(err, "")) { return 1; }


    err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);

    if (err != CL_SUCCESS) {
        printf("Error Code: %d\n", err);
        size_t log_size;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char *log = (char*)malloc(log_size);
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        printf("%s\n", log);
        return 1;
    }

    profileGpu(data, context, program, queue, 0);

    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    free(h_backwardMask);
    free(h_scannedBackwardMask);

    return 0;
}

int rleCpu(int *in, int n, int* symbolsOut, int* countsOut){

    if (n == 0)
        return 0;

    int outIndex = 0;
    int symbol = in[0];
    int count = 1;
    int *bMask = (int*) malloc (sizeof(int) * n);
    int *sBMask = (int*) malloc (sizeof(int) * n);
    int *cBMask = (int*) malloc (sizeof(int) * n);
    int totalRuns;

    //mask
    for (int i = 0; i < n; ++i) {
        if ( i == 0 ){
            bMask[0] = 1;
        }
        else{
            bMask[i] = (in[i] != in[i-1]);
        }
    }

    //scan
    fillPrefixSum(bMask, n, sBMask);
    //compact
    for (int i = 0; i < n; ++i) {
        if (i == 0){
            cBMask[0] = 0;
        }
        else if (i == (n-1)) {
            cBMask[sBMask[i]] = i+1;
            totalRuns = sBMask[i];
        }
        else if ( sBMask[i] != sBMask[i-1]) {
            cBMask[sBMask[i] -1] = i;
        }
    }

    //scatter
    for (int i = 0; i < n; ++i) {
        symbolsOut[i] = in[cBMask[i]];
        countsOut[i] = cBMask[i+1] - cBMask[i];
    }
    return totalRuns;

}
