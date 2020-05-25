//#pragma OPENCL EXTENSION cl_khr_initialize_memory : enable


//#define BLOCK_SIZE 1048576
//#define BLOCK_SIZE 4096 
#define BLOCK_SIZE 4096 
//#define BLOCK_SIZE 102400*5 
//#define BLOCK_SIZE 16 // 16 bytes for testing purpose. 
#define Wrap(value, limit) (((value) < (limit)) ? (value) : ((value) - (limit)))

typedef struct __attribute__((packed)) r_FIFO {
    unsigned long int id;

    unsigned int s0Idx;
    unsigned long int len;
    unsigned int pred[BLOCK_SIZE];
    unsigned char unrotated[BLOCK_SIZE];
    char block[BLOCK_SIZE];
} r_FIFO;


__kernel void BwtReverse(__global struct r_FIFO *inf,
                         const unsigned long int num_of_blocks)
{
    //Note to self: Kernel shall fail if the input file exceds over 100MB.
    
    //Getting ID of one of the data blocks. 
    unsigned long int gid = get_global_id(0);
    //int oset = id * BLOCK_SIZE;
    //Idea is to transform this block and store it back.
    if(gid < num_of_blocks) {
        //printf("ID: %d\n", gid); 
        //printf("%c\n", inf[gid].block[0]);
        
        __private unsigned long int i, j, sum; 
        
        int count[256];
        int s0Idx = inf[gid].s0Idx;
        int blockSize = inf[gid].len;

        //printf("blockSize %d: %d\n", blockSize, gid);

        //#pragma unroll 4  
        for(i=0; i<256; i++) {
            count[i] = 0;
        }
        
        // Set pred[i] to the number of times block[i] appears in the
        // substring block[0..i-1]. 
        // count[i] = number of times character i appers in block.

        for(i=0; i < blockSize; ++i) {
            inf[gid].pred[i] = count[inf[gid].block[i]];
            count[inf[gid].block[i]]++;
        }

        // Set count[i] to the number of characters in block
        // lexicographically less then i.
        sum = 0;

        for(i = 0; i<256; ++i) {
            j = count[i];
            count[i] = sum;
            sum += j;
        }

        // Constructing the string.
        i = s0Idx;
        for(j = blockSize; j > 0; j--) {
            inf[gid].unrotated[j-1] = inf[gid].block[i];
            i = inf[gid].pred[i] + count[inf[gid].block[i]];
        }
    }
}

