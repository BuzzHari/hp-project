/**
*   Copyright 2012,2015 Keith Daigle
*   This file is part of cuda-bwt.
*
*   cuda-bwt is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   cuda-bwt is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with cuda-bwt.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "bucket.h"
//This kernel will take 3 arrays and put their data back into 2 other arrays based upon 
//the bucket index array passed in and the 
__global__ void CalcPushPositions(
		bucket_t * buckets,
		unsigned int * rotation_idx,
		bucket_t * sorted_buckets,
		unsigned int * sorted_rotation_idx,
		unsigned int * sorted_bucket_index,
		unsigned int * offset_tmp,
		unsigned int n,
		unsigned int BLOCK_LEN)
{
        unsigned int i = (blockIdx.x * BLOCK_LEN) + threadIdx.x;
        unsigned int prev_bucket = ((i+n)-1) % n;
        if( i < n)
        {
		//if we are on the edge of a bucket, walk through the whole bucket
		//to make sure that we note each member of the bucket's index in the bucket
	        if(sorted_bucket_index[i] != sorted_bucket_index[prev_bucket] || i == 0)
	        {
			 register unsigned int curr_bucket_start = sorted_bucket_index[i];
	                 register unsigned int j = 0;
			 while( sorted_bucket_index[i] == sorted_bucket_index[i+j]  && j+i < n )
			 {
				offset_tmp[i+j] = curr_bucket_start + j;
				//incrmenting in the above assignment is bad, mmmkayyy?
				j++;
			 }
	        }
	}
}

