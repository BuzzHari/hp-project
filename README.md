# Heterogeneous Parallelism Course Project
Accelerating lossless compression algorithms.

This repository contains Serial, CUDA and OpenCL implementation of the following alogorithms:
* Burrows-Wheeler Transform
* LZSS
* RLE

The Serial and CUDA implementations were borrowed from the repositories as mentioned below:

* Burrows-Wheeler Transform:
  * Serial: https://github.com/MichaelDipperstein/bwt 
  * CUDA:  https://github.com/keith-daigle/cuda-bwt 
* LZSS:
  * Serial: https://github.com/MichaelDipperstein/lzss
  * CUDA: https://github.com/adnanozsoy/CUDA_Compression
* RLE:
  * Serial and CUDA: https://github.com/Erkaman/parle-cuda
  
  Using the above repositories as a refernece and for benchmarking, we implemented OpenCL version of these algorithms.
  You could see our results in the report.
