#define BLOCK_SIZE 1048576

struct __attribute__((packed)) FIFO {
    int id;
    int len;
    char block[BLOCK_SIZE];
};


__kernel void BwTransform(__global struct FIFO *infifo,
                          __global struct FIFO *outfifo,
                          const unsigned int num_of_blocks)
{
    //Getting ID of one of the data blocks. 
    int id = get_global_id(0);

    //Idea is to transform this block and store it back.
    if(id < num_of_blocks) {
    
        unsigned int i, j, k;
        unsigned int *rotationIdx; // index of first char in rotation.
        unsigned int *v;           // index fo radix sorted characters
        int s0Idx;                 // index of S0 in rotations.
        unsigned char *last;       // last characters from sorted rotations.

        unsigned int counters[256]; // counters and offsets
        unsigned int offsetTables[256]; // for radix sorting with characters.
         
        ;//Have to implement Heap for this?! 
        rotationIdx = (unsigned int *) malloc(BLOCK_SIZE * sizeof(unsigned int));
        ;// Might need two kernels.
        ;// One to create the rotated data,
        ;// This rotated data will be passed to CPU.
        ;// The I sort it. then pass it back to another kernel
        ;// to get the final transformation.

        ;// Another idea, instead of CPU sort, I'll have
        ;// implement GPU sort.
    }

}
    


