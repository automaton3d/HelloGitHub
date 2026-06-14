/*
 * ca_cuda.h — C-linkage declarations for CUDA kernel wrappers
 *
 * Include from mytry.c (via pulsating.h when USE_CUDA is defined)
 * and from ca_cuda.cu.
 */

#ifndef CA_CUDA_H_
#define CA_CUDA_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Allocate device grids and zero d_and_count */
void cuda_alloc_grids(void);

/* Upload host grids to device (call once after init) */
void cuda_upload_grid(void *h_grid, void *h_grid_next);

/* Run one pulse step on GPU (BFS + activation flags) */
void cuda_pulse_step(int tick);

/* Run one sinc step on GPU (wave eq + Bresenham + TTL + AND + damping copy-back) */
void cuda_sinc_step(int tick);

/* Copy grid from device to host for rendering */
void cuda_download_grid(void *h_grid);

/* Copy and_count from device to host for rendering */
void cuda_download_and_count(int *h_and_count, int n);

/* Upload full grid back to device (after convergence detection on host) */
void cuda_upload_grid_full(void *h_grid);

/* Free device memory */
void cuda_free(void);

#ifdef __cplusplus
}
#endif

#endif /* CA_CUDA_H_ */
