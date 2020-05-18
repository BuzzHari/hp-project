#define BLOCKSIZE 1048576

struct __attribute__((packed)) FIFO {
    int id;
    int len;
    char block[BLOCKSIZE];
};


__kernel void BwTransform(__global struct FIFO *infifo,
                          __global struct FIFO *outfifo,
                          const unsigned int num_of_blocks)
{
    //Getting ID of one of the data blocks. 
    int id = get_global_id(0);

    //Idea is to transform this block and store it back.
    printf("ID: %d\n", id);
    printf("Num_of_Blocks: %d\n", num_of_blocks);
    uchar a  = (uchar)('a');
    uchar z  = (uchar)('z');
    uchar aA = (uchar)('a' - 'A');
    if(id < num_of_blocks) {
        //outfifo[id].block[0] = 'A';
        //printf("%c", outfifo[id].block[0]);
        outfifo[id].id = id;
        outfifo[id].len = infifo[id].len;
        for(int i = 0; i < infifo[id].len; i++) {
            uchar uc = (uchar)(infifo[id].block[i] - aA);
            outfifo[id].block[i] = ( ( infifo[id].block[i] >= a) && (infifo[id].block[i] <= z)) ? uc : infifo[id].block[i];
        }
    }

}
    


