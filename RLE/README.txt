to compile Serial_VS_CUDA:
    nvcc kernel.cu

to run:
    ./a.out


to compile Serial_VS_OpenCL:
    g++ -std=c++0x -lOpenCL -lm main.cpp

to run:
    ./a.out
