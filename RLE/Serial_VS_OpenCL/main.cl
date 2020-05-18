__kernel void compactKernel(__global int* g_in, __global int* g_scannedBackwardMask,
        __global int *g_compactedBackwardMask, __global int *g_totalRuns, const unsigned int n) {

    int id = get_global_id(0);

    if (id == (n - 1)) {
        g_compactedBackwardMask[g_scannedBackwardMask[id] + 0] = id + 1;
        *g_totalRuns = g_scannedBackwardMask[id];
    }

    if (id == 0) {
        g_compactedBackwardMask[0] = 0;
    }
    else if (g_scannedBackwardMask[id] != g_scannedBackwardMask[id - 1]) {
        g_compactedBackwardMask[g_scannedBackwardMask[id] - 1] = id;
    }

}

__kernel void scatterKernel(__global int* g_compactedBackwardMask, __global int* g_totalRuns,
        __global int* g_in, __global int* g_symbolsOut, __global int* g_countsOut) {
    int n = *g_totalRuns;

    int id= get_global_id(0);

    int a = g_compactedBackwardMask[id];
    int b = g_compactedBackwardMask[id + 1];

    g_symbolsOut[id] = g_in[a];
    g_countsOut[id] = b - a;
}

__kernel void maskKernel(__global int *g_in, __global int* g_backwardMask, const unsigned int n) {
    int id = get_global_id(0);
    if(id == 0)
        g_backwardMask[id] = 1;
    else
        g_backwardMask[id] = (g_in[id] != g_in[id -1]);
}
