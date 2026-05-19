#pragma once
#include <cuda_runtime.h>

// __constant__ and __device__ symbols are defined in cuda_automaton.cu
// (same compilation unit as the kernels).
// Other .cu files should NOT use these symbols directly — they are
// only accessible to the kernel code.

#ifdef __cplusplus
extern "C" {
#endif
void setCudaConstants(unsigned EL, unsigned W_USED, unsigned RMAX);
void resetCudaCtrl();
#ifdef __cplusplus
}
#endif
