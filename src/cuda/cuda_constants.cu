// cuda_constants.cu — STUB
//
// All __constant__ definitions, setCudaConstants(), and resetCudaCtrl()
// have been consolidated into cuda_automaton.cu so that they reside in the
// same compilation unit as the kernels (required without -dc separate
// compilation; otherwise cudaMemcpyToSymbol writes to a different copy of
// constant memory than the one the kernel reads).
//
// This file is kept as a stub so the Makefile still compiles it without error.

#include <cuda_runtime.h>
