//#pragma OPENCL EXTENSION cl_khr_initialize_memory : enable


//#define BLOCK_SIZE 1048576
//#define BLOCK_SIZE 4*1024 
#define BLOCK_SIZE 16 // 16 bytes for testing purpose. 
#define UCHAR_MAX 255
#define Wrap(value, limit) (((value) < (limit)) ? (value) : ((value) - (limit)))

struct __attribute__((packed)) FIFO {
    int id;
    int len;
    unsigned int rotationIdx[BLOCK_SIZE];
    unsigned int v[BLOCK_SIZE];
    char block[BLOCK_SIZE];
};


struct __attribute__((packed)) LAST {
    int s0Idx;
    unsigned char last[BLOCK_SIZE];
};

__kernel void BwTransform(__global struct FIFO *inf,
                          __global struct FIFO *outf,
                          const unsigned int num_of_blocks)
{
    //Note to self: Kernel shall fail if the input file exceds over 100MB.
    
    //Getting ID of one of the data blocks. 
    int gid = get_global_id(0);
    //int oset = id * BLOCK_SIZE;
    //Idea is to transform this block and store it back.
    if(gid < num_of_blocks) {
        printf("ID: %d\n", gid); 
        //outf[gid].block[0] =inf[gid].block[0];
        //printf("%c\n", inf[gid].block[0]);
        //printf("%c\n", outf[gid].block[0]);
        
        __private unsigned int i, j, k; 
        int s0Idx;
        unsigned int counters[256] = { 0 };
        unsigned int offsetTable[256] = { 0 };

        int blockSize = inf[gid].len;
        printf("blockSize %d\n", blockSize);


        //...Causing junk values.

        //#pragma unroll 4  
        for(i=0; i<256; i++) {
            counters[i] = 0;
            offsetTable[i] = 0;
        }
        
        for(i=0; i<blockSize; ++i) {
            counters[inf[gid].block[i]]++;
        }
        
        offsetTable[0] = 0;
    

        // Sort on 2nd character.
        //#pragma unroll 4  
        for(i=1; i<256; ++i) {
            offsetTable[i] = offsetTable[i-1] + counters[i-1];
        }
        
        //#pragma unroll 4  
        for(i=0; i<blockSize-1; ++i) {
            printf("%c\n", inf[gid].block[1]);
            j = inf[gid].block[i+1];
            printf("offsetTable[j]: %d\n", offsetTable[j]);
            inf[gid].v[offsetTable[j]] = i;
            printf("v[offsetTable[j]]: %d\n",inf[gid].v[offsetTable[j]]);
            offsetTable[j] = offsetTable[j] + 1;
        }

        // Handle wrap for string starting at end of block.
        j = inf[gid].block[0];
        inf[gid].v[offsetTable[j]] = i;
        offsetTable[0] = 0;

        // Radix sort on first chracter.
        //#pragma unroll 4
        for(i=1; i<256; ++i) {
            offsetTable[i] = offsetTable[i-1] + counters[i-1];
        }

        //#pragma unroll 4
        for(i = 0; i < blockSize; ++i) {
            j = inf[gid].v[i];
            j = inf[gid].block[j];
            printf("offsetTable[j]: %d\n", offsetTable[j]);
            inf[gid].rotationIdx[offsetTable[j]] = inf[gid].v[i];
            printf("rotationIdx[offsetTable[j]]: %d\n",inf[gid].rotationIdx[offsetTable[j]]);
            offsetTable[j] = offsetTable[j] + 1;
        }
    }
}
    
__kernel void BwLast(__global struct FIFO *inf,
                     __global struct LAST *last
                    const unsigned int num_of_blocks) 
{
    
    int gid = get_global_id(0);

    if (gid < num_of_blocks) {
        unsigned int i, j;
        unsigned int blockSize = inf[gid].len; 
        last[gid].s0Idx = 0;

        for (i = 0; i < blockSize; ++i) {
            j = inf[gid].rotationIdx[i]; 
            if (j != 0)
                last[gid].last[i] = inf[gid].block[j-1];
            else {
                s0Idx = i;
                last[i] = inf[gid].block[blockSize-1];
            }
        }
    }
}

