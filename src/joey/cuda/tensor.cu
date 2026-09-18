#include <cuda_runtime.h>
#include <stdio.h>

// Joey Native CUDA Tensor Operations
// Implements core forward pass and LoRA fine-tuning operations natively on GPU

__global__ void matmul_kernel(const float* A, const float* B, float* C, int M, int N, int K) {
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;

    if (row < M && col < N) {
        float sum = 0.0f;
        for (int i = 0; i < K; ++i) {
            sum += A[row * K + i] * B[i * N + col];
        }
        C[row * N + col] = sum;
    }
}

// LoRA Forward Pass: y = x W + x A B
__global__ void lora_forward_kernel(const float* x, const float* W, const float* A, const float* B, float* y, int batch_seq, int d_model, int r) {
    int row = blockIdx.y * blockDim.y + threadIdx.y; // batch_seq
    int col = blockIdx.x * blockDim.x + threadIdx.x; // d_model

    if (row < batch_seq && col < d_model) {
        // Base weight multiplication (x * W)
        float base_val = 0.0f;
        for(int k=0; k < d_model; ++k) {
            base_val += x[row * d_model + k] * W[k * d_model + col];
        }
        
        // LoRA multiplication (x * A * B)
        float lora_a_val[128]; // max r=128
        for(int k=0; k<r; ++k) {
            lora_a_val[k] = 0.0f;
            for(int i=0; i<d_model; ++i) {
                lora_a_val[k] += x[row * d_model + i] * A[i * r + k];
            }
        }
        
        float lora_val = 0.0f;
        for(int k=0; k<r; ++k) {
            lora_val += lora_a_val[k] * B[k * d_model + col];
        }

        y[row * d_model + col] = base_val + lora_val;
    }
}

extern "C" void joey_cuda_lora_forward(float* x, float* W, float* A, float* B, float* y, int batch_seq, int d_model, int r) {
    dim3 threads(16, 16);
    dim3 blocks((d_model + threads.x - 1) / threads.x, (batch_seq + threads.y - 1) / threads.y);
    lora_forward_kernel<<<blocks, threads>>>(x, W, A, B, y, batch_seq, d_model, r);
    cudaDeviceSynchronize();
}

