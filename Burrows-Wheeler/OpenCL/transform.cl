
//#define BLOCK_SIZE 1048576
#define BLOCK_SIZE 4*1024 
#define UCHAR_MAX 255
#define Wrap(value, limit) (((value) < (limit)) ? (value) : ((value) - (limit)))

struct __attribute__((packed)) FIFO {
    int id;
    int len;
    char block[BLOCK_SIZE];
};


__kernel void BwTransform(__global struct FIFO *infifo,
                          __global struct FIFO *outfifo,
                          __global unsigned int *rotationIdx,
                          __global unsigned int *v,
                          __global unsigned char *last,
                          const unsigned int num_of_blocks)
{
    //Getting ID of one of the data blocks. 
    int id = get_global_id(0);
    int oset = id * BLOCK_SIZE;
    //Idea is to transform this block and store it back.
    if(id < num_of_blocks) {
        printf("ID: %d\n", id); 
        unsigned int i, j, k;
        unsigned int *rotationIdx; // index of first char in rotation.
        unsigned int *v;           // index fo radix sorted characters
        int s0Idx;                 // index of S0 in rotations.
        unsigned char *last;       // last characters from sorted rotations.

        unsigned int counters[256]; // counters and offsets
        unsigned int offsetTable[256]; // for radix sorting with characters.
         
        ;//Have to implement Heap for this?! 
        ;//rotationIdx = (unsigned int *) malloc(BLOCK_SIZE * sizeof(unsigned int));
        ;// Might need two kernels.
        ;// One to create the rotated data,
        ;// This rotated data will be passed to CPU.
        ;// The I sort it. then pass it back to another kernel
        ;// to get the final transformation.

        ;// Another idea, instead of CPU sort, I'll have
        ;// implement GPU sort.

        // Counting number fo characters for radix sort.
        
        for(i = 0; i < infifo[id].len; ++i) {
           counters[infifo[id].block[i]]++;
           //if(id ==1 )
           // printf("%d ",counters[infifo[id].block[i]]);
        }
    
        //if(id ==1 )
        //  printf("\n");

        offsetTable[0] = 0;

        for(i = 1; i < 256; ++i) {
              // determine number of values before those sorted under i.

            printf("ID:%d | Test: 4\n", id);
            offsetTable[i] = offsetTable[i-1] + counters[i-1];
        }

        // Sort on 2nd character.
        //#pragma unroll
        printf("infifo[id].len: %d", infifo[id].len);
        for(i = 0; i < infifo[id].len; ++i) 
        {
           printf("ID:%d | Test: 3\n", id);
           j = infifo[id].block[i + 1];     
           printf("J: %d\n", j);
           printf("%d\n", oset + offsetTable[j]);
           v[oset + offsetTable[j]] = i;
           printf("V:%d\n", v[oset + offsetTable[j]]);
           v[oset + offsetTable[j]] = v[oset + offsetTable[j]] + 1;
        }

        // Handling wrap around.
        j = infifo[id].block[0];
        v[oset + offsetTable[j]] = i;
        offsetTable[0] = 0;
        printf("Test: 2\n");
        // Radix sort on first charcter in rotation.

        for(i = 1; i < 256; ++i){
              printf("Test: 1\n");
              offsetTable[i] = offsetTable[i-1] + counters[i-1];
        }
        
        for(i = 0; i < infifo[id].len; ++i) {
           j = v[oset + i];     
           j = infifo[id].block[j];
           rotationIdx[oset + offsetTable[j]] = v[oset + i];
           printf("Test\n");
           printf("%u",rotationIdx[oset + offsetTable[j]]);
           offsetTable[j] = offsetTable[j] + 1;  
        }

        for (i = 0, k = 0; (i <= UCHAR_MAX) && (k < (infifo[id].len - 1)); i++)
        {
            for (j = 0; (j <= UCHAR_MAX) && (k < (infifo[id].len - 1)); j++)
            {
                unsigned int first = k;

                /* count strings starting with ij */
                while ((i == infifo[id].block[rotationIdx[oset + k]]) &&
                    (j == infifo[id].block[Wrap(rotationIdx[oset + k] + 1,  infifo[id].len)]))
                {
                    k++;

                    if (k == infifo[id].len)
                    {
                        /* we've searched the whole block */
                        break;
                    }
                }
                //printf("%d",rotationIdx[oset + first]);
               
                //if (k - first > 1)
                //{
                //    /* there are at least 2 strings staring with ij, sort them */
                //    //qsort(&rotationIdx[oset + first], k - first, sizeof(int),
                //    //    ComparePresorted);
                //}*/
            }
        }
    }
}
    


