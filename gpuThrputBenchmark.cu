/*
 * Author: Brian Bowden
 * Date: 11/2/11
 *
 * gpuThrputBenchmark.cu
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include "repeat.h"

#define REPEAT(iters)	repeat ## iters

#define INT 0
#define UINT 1
#define FLOAT 2
#define DOUBLE 3

// ############# CHANGE BELOW 2 LINES FOR DIFFERENT DATA TYPES!!!!! ######################
typedef float TYPE;
#define DATATYPE (FLOAT)
// ############# CHANGE BELOW FOR MAXIMUM OCCUPANCY OR FOR LATENCY #######################
#define MAX_OCCUPANCY 0

int total_threads;
const int threads_per_warp = 32;
const int max_warps = 48;
const int number_multi_processors = 14;
const float clock_speed = 1.5e9;	//1.5 GHz
const int number_runs = 25;
const int N = threads_per_warp * max_warps * number_multi_processors;
TYPE operand = 10;

const int block_size[] = {2, 3, 4, 6, 8};
TYPE* host_A;
TYPE* host_B;
TYPE* host_C;
TYPE* host_D;
TYPE* device_A;
TYPE* device_B;
TYPE* device_C;
TYPE* device_D;
	
cudaEvent_t start, stop;

void print_results(double average_time, int number_runs, int total_threads, int iterations) 
{
	int number_instructions = total_threads * iterations * 4;
	average_time = average_time / number_runs;
	long long int number_cycles = (long long int)((average_time * clock_speed) / 1000);
	double throughput = (double) number_instructions / number_cycles;;
	
#if (MAX_OCCUPANCY)
	printf("Average Time for %d iterations : %g (ms)\n", iterations, average_time);
	printf("Total number cycles : %ld\n", number_cycles);
	printf("Throughput : %0.3g\n", throughput);
#endif
#if (!MAX_OCCUPANCY)
	int number_warps = total_threads / threads_per_warp;
	printf("%d warps : %0.3g\n", number_warps, throughput);
#endif
}

__global__ void kernelAdd(TYPE* A, TYPE* B, TYPE* C, TYPE *D, TYPE operand)
{
	int i = (blockIdx.x * blockDim.x) + threadIdx.x;
	TYPE a_val = A[i];
	TYPE b_val = B[i];
	TYPE c_val = C[i];
	TYPE d_val = D[i];
	repeat4096(a_val = b_val + operand; b_val = c_val + operand; c_val = d_val + operand; d_val = a_val + operand;);
	D[i] = d_val;
	C[i] = c_val;
	B[i] = b_val;
	A[i] = a_val;
}

void getAddThroughput()
{
	double average_time = 0.0;
	float time_elapsed;
	int number_threads = 0;
	int iterations = 4096;
	if (total_threads == 32 || MAX_OCCUPANCY) printf("\n---------------Addition--------------------\n"); 
	int i = 0;
	number_threads = total_threads / block_size[i];
	for (int j = 0; j < number_runs; j++) 
	{
		cudaEventRecord(start, 0);
		kernelAdd<<<block_size[i] * number_multi_processors, number_threads>>>(device_A, device_B, device_C, device_D, operand);
		cudaEventRecord(stop, 0);
		cudaEventSynchronize(start);
		cudaEventSynchronize(stop);
		cudaEventElapsedTime(&time_elapsed, start, stop);
		average_time += time_elapsed;
	}
	print_results(average_time, number_runs, total_threads, iterations);
}

__global__ void kernelSub(TYPE* A, TYPE* B, TYPE* C, TYPE *D, TYPE operand)
{
	int i = (blockIdx.x * blockDim.x) + threadIdx.x;
	TYPE a_val = A[i];
	TYPE b_val = B[i];
	TYPE c_val = C[i];
	TYPE d_val = D[i];
	repeat4096(a_val = b_val - operand; b_val = c_val - operand; c_val = d_val - operand; d_val = a_val - operand;);
	D[i] = d_val;
	C[i] = c_val;
	B[i] = b_val;
	A[i] = a_val;
}

void getSubtractThroughput()
{
	double average_time = 0.0;
	float time_elapsed;
	int number_threads = 0;
	int iterations = 4096;
	if (total_threads == 32 || MAX_OCCUPANCY) printf("\n--------------Subtraction------------------\n"); 
	int i = 0;
	number_threads = total_threads / block_size[i];
	for (int j = 0; j < number_runs; j++) 
	{
		cudaEventRecord(start, 0);
		kernelSub<<<block_size[i] * number_multi_processors, number_threads>>>(device_A, device_B, device_C, device_D, operand);
		cudaEventRecord(stop, 0);
		cudaEventSynchronize(start);
		cudaEventSynchronize(stop);
		cudaEventElapsedTime(&time_elapsed, start, stop);
		average_time += time_elapsed;
	}
	print_results(average_time, number_runs, total_threads, iterations);
}


__global__ void kernelMul(TYPE* A, TYPE* B, TYPE* C, TYPE *D, TYPE operand)
{
	int i = (blockIdx.x * blockDim.x) + threadIdx.x;
	TYPE a_val = A[i];
	TYPE b_val = B[i];
	TYPE c_val = C[i];
	TYPE d_val = D[i];
	repeat4096(a_val = b_val * operand; b_val = c_val * operand; c_val = d_val * operand; d_val = a_val * operand;);
	D[i] = d_val;
	C[i] = c_val;
	B[i] = b_val;
	A[i] = a_val;
}

void getMultiplyThroughput()
{
	double average_time = 0.0;
	float time_elapsed;
	int number_threads = 0;
	int iterations = 4096;
	if (total_threads == 32 || MAX_OCCUPANCY) printf("\n------------Multiplication-----------------\n"); 
	int i = 0;
	number_threads = total_threads / block_size[i];
	for (int j = 0; j < number_runs; j++) 
	{
		cudaEventRecord(start, 0);
		kernelMul<<<block_size[i] * number_multi_processors, number_threads>>>(device_A, device_B, device_C, device_D, operand);
		cudaEventRecord(stop, 0);
		cudaEventSynchronize(start);
		cudaEventSynchronize(stop);
		cudaEventElapsedTime(&time_elapsed, start, stop);
		average_time += time_elapsed;
	}
	print_results(average_time, number_runs, total_threads, iterations);
}


__global__ void kernelDiv(TYPE* A, TYPE* B, TYPE* C, TYPE *D, TYPE operand)
{
	int i = (blockIdx.x * blockDim.x) + threadIdx.x;
	TYPE a_val = A[i];
	TYPE b_val = B[i];
	TYPE c_val = C[i];
	TYPE d_val = D[i];
	repeat512(a_val = b_val / operand; b_val = c_val / operand; c_val = d_val / operand; d_val = a_val / operand;);
	D[i] = d_val;
	C[i] = c_val;
	B[i] = b_val;
	A[i] = a_val;
}

void getDivideThroughput()
{
	double average_time = 0.0;
	float time_elapsed;
	int number_threads = 0;
	int iterations = 512;
	if (total_threads == 32 || MAX_OCCUPANCY) printf("\n---------------Division--------------------\n"); 
	int i = 0;
	number_threads = total_threads / block_size[i];
	for (int j = 0; j < number_runs; j++) 
	{
		cudaEventRecord(start, 0);
		kernelDiv<<<block_size[i] * number_multi_processors, number_threads>>>(device_A, device_B, device_C, device_D, operand);
		cudaEventRecord(stop, 0);
		cudaEventSynchronize(start);
		cudaEventSynchronize(stop);
		cudaEventElapsedTime(&time_elapsed, start, stop);
		average_time += time_elapsed;
	}
	print_results(average_time, number_runs, total_threads, iterations);
}

__global__ void kernelMAD(TYPE* A, TYPE* B, TYPE* C, TYPE *D, TYPE operand)
{
	int i = (blockIdx.x * blockDim.x) + threadIdx.x;
	TYPE a_val = A[i];
	TYPE b_val = B[i];
	TYPE c_val = C[i];
	TYPE d_val = D[i];
	repeat4096(a_val = b_val * operand; a_val = b_val + operand; b_val = c_val * operand; b_val = c_val + operand; 
	c_val = d_val * operand; c_val = d_val + operand; d_val = a_val * operand; d_val = a_val + operand;);
	D[i] = d_val;
	C[i] = c_val;
	B[i] = b_val;
	A[i] = a_val;
}

void getMultiplyAddThroughput()
{
	double average_time = 0.0;
	float time_elapsed;
	int number_threads = 0;
	int iterations = 4096;
	if (total_threads == 32 || MAX_OCCUPANCY) printf("\n-------------Multiply-Add------------------\n"); 
	int i = 0;
	number_threads = total_threads / block_size[i];
	for (int j = 0; j < number_runs; j++) 
	{
		cudaEventRecord(start, 0);
		kernelMAD<<<block_size[i] * number_multi_processors, number_threads>>>(device_A, device_B, device_C, device_D, operand);
		cudaEventRecord(stop, 0);
		cudaEventSynchronize(start);
		cudaEventSynchronize(stop);
		cudaEventElapsedTime(&time_elapsed, start, stop);
		average_time += time_elapsed;
	}
	print_results(average_time, number_runs, total_threads, iterations);
}


#if DATATYPE == INT || DATATYPE == UINT
__global__ void kernelVectorAdd(TYPE* A, TYPE* B, TYPE* C, TYPE *D, TYPE operand)
{
	int i = (blockIdx.x * blockDim.x) + threadIdx.x;
	TYPE a_val = A[i];
	TYPE b_val = B[i];
	TYPE c_val = C[i];
	TYPE d_val = D[i];
	repeat2048(a_val = a_val + b_val + operand; b_val = b_val + c_val + operand; c_val = c_val + d_val + operand; a_val = a_val + d_val + operand;);
	D[i] = d_val;
	C[i] = c_val;
	B[i] = b_val;
	A[i] = a_val;
}

void getVectorAddThroughput()
{
	double average_time = 0.0;
	float time_elapsed;
	int number_threads = 0;
	int iterations = 2048;
	if (total_threads == 32 || MAX_OCCUPANCY) printf("\n-------------Vector-Addition-----------------\n"); 
	int i = 0;
	number_threads = total_threads / block_size[i];
	for (int j = 0; j < number_runs; j++) 
	{
		cudaEventRecord(start, 0);
		kernelVectorAdd<<<block_size[i] * number_multi_processors, number_threads>>>(device_A, device_B, device_C, device_D, operand);
		cudaEventRecord(stop, 0);
		cudaEventSynchronize(start);
		cudaEventSynchronize(stop);
		cudaEventElapsedTime(&time_elapsed, start, stop);
		average_time += time_elapsed;
	}
	print_results(average_time, number_runs, total_threads, iterations);
}

__global__ void kernelRemainder(TYPE* A, TYPE* B, TYPE* C, TYPE *D, TYPE operand)
{
	int i = (blockIdx.x * blockDim.x) + threadIdx.x;
	TYPE a_val = A[i];
	TYPE b_val = B[i];
	TYPE c_val = C[i];
	TYPE d_val = D[i];
	repeat256(a_val = b_val % operand; b_val = c_val % operand; c_val = d_val % operand; d_val = a_val % operand;);
	D[i] = d_val;
	C[i] = c_val;
	B[i] = b_val;
	A[i] = a_val;
}

void getRemainderThroughput()
{
	double average_time = 0.0;
	float time_elapsed;
	int number_threads = 0;
	int iterations = 256;
	if (total_threads == 32 || MAX_OCCUPANCY) printf("\n-------------Remainder------------------\n"); 
	int i = 0;
	number_threads = total_threads / block_size[i];
	for (int j = 0; j < number_runs; j++) 
	{
		cudaEventRecord(start, 0);
		kernelRemainder<<<block_size[i] * number_multi_processors, number_threads>>>(device_A, device_B, device_C, device_D, operand);
		cudaEventRecord(stop, 0);
		cudaEventSynchronize(start);
		cudaEventSynchronize(stop);
		cudaEventElapsedTime(&time_elapsed, start, stop);
		average_time += time_elapsed;
	}
	print_results(average_time, number_runs, total_threads, iterations);
}
#endif

#if DATATYPE == INT

__global__ void kernelAnd(int* A, int* B, int* C, int *D, int operand)
{
	int i = (blockIdx.x * blockDim.x) + threadIdx.x;
	int a_val = A[i];
	int b_val = B[i];
	int c_val = C[i];
	int d_val = D[i];
	repeat4096(a_val = b_val & operand; b_val = c_val & operand; c_val = d_val & operand; d_val = a_val & b_val;);
	D[i] = d_val;
	C[i] = c_val;
	B[i] = b_val;
	A[i] = a_val;
}

void getAndThroughput()
{
	double average_time = 0.0;
	float time_elapsed;
	int number_threads = 0;
	int iterations = 4096;
	if (total_threads == 32 || MAX_OCCUPANCY) printf("\n-----------------AND-----------------------\n");
	int i = 0;
	number_threads = total_threads / block_size[i];
	for (int j = 0; j < number_runs; j++) 
	{
		cudaEventRecord(start, 0);
		kernelAnd<<<block_size[i] * number_multi_processors, number_threads>>>(device_A, device_B, device_C, device_D, operand);
		cudaEventRecord(stop, 0);
		cudaEventSynchronize(start);
		cudaEventSynchronize(stop);
		cudaEventElapsedTime(&time_elapsed, start, stop);
		average_time += time_elapsed;
	}
	print_results(average_time, number_runs, total_threads, iterations);
}

__global__ void kernelOr(int* A, int* B, int* C, int *D, int operand)
{
	int i = (blockIdx.x * blockDim.x) + threadIdx.x;
	int a_val = A[i];
	int b_val = B[i];
	int c_val = C[i];
	int d_val = D[i];
	repeat4096(a_val = b_val | operand; b_val = c_val | operand; c_val = d_val | operand; d_val = a_val | b_val;);
	D[i] = d_val;
	C[i] = c_val;
	B[i] = b_val;
	A[i] = a_val;
}

void getOrThroughput()
{
	double average_time = 0.0;
	float time_elapsed;
	int number_threads = 0;
	int iterations = 4096;
    if (total_threads == 32 || MAX_OCCUPANCY) printf("\n-----------------OR------------------------\n"); 
	int i = 0;
	number_threads = total_threads / block_size[i];
	for (int j = 0; j < number_runs; j++) 
	{
		cudaEventRecord(start, 0);
		kernelOr<<<block_size[i] * number_multi_processors, number_threads>>>(device_A, device_B, device_C, device_D, operand);
		cudaEventRecord(stop, 0);
		cudaEventSynchronize(start);
		cudaEventSynchronize(stop);
		cudaEventElapsedTime(&time_elapsed, start, stop);
		average_time += time_elapsed;
	}
	print_results(average_time, number_runs, total_threads, iterations);
}

__global__ void kernelXor(int* A, int* B, int* C, int *D, int operand)
{
	int i = (blockIdx.x * blockDim.x) + threadIdx.x;
	int a_val = A[i];
	int b_val = B[i];
	int c_val = C[i];
	int d_val = D[i];
	repeat4096(a_val = b_val ^ operand; b_val = c_val ^ operand; c_val = d_val ^ operand; d_val = a_val ^ operand;);
	D[i] = d_val;
	C[i] = c_val;
	B[i] = b_val;
	A[i] = a_val;
}

void getXorThroughput()
{
	double average_time = 0.0;
	float time_elapsed;
	int number_threads = 0;
	int iterations = 4096;
	if (total_threads == 32 || MAX_OCCUPANCY) printf("\n-----------------XOR-----------------------\n");
	int i = 0;
	number_threads = total_threads / block_size[i];
	for (int j = 0; j < number_runs; j++) 
	{
		cudaEventRecord(start, 0);
		kernelXor<<<block_size[i] * number_multi_processors, number_threads>>>(device_A, device_B, device_C, device_D, operand);
		cudaEventRecord(stop, 0);
		cudaEventSynchronize(start);
		cudaEventSynchronize(stop);
		cudaEventElapsedTime(&time_elapsed, start, stop);
		average_time += time_elapsed;
	}
	print_results(average_time, number_runs, total_threads, iterations);
}

__global__ void kernelShl(int* A, int* B, int* C, int *D, int operand)
{
	int i = (blockIdx.x * blockDim.x) + threadIdx.x;
	int a_val = A[i];
	int b_val = B[i];
	int c_val = C[i];
	int d_val = D[i];
	repeat4096(a_val = b_val << operand; b_val = c_val << operand; c_val = d_val << operand; d_val = a_val << operand;);
	D[i] = d_val;
	C[i] = c_val;
	B[i] = b_val;
	A[i] = a_val;
}

void getShlThroughput()
{
	double average_time = 0.0;
	float time_elapsed;
	int number_threads = 0;
	int iterations = 4096;
	if (total_threads == 32 || MAX_OCCUPANCY) printf("\n--------------Shift-Left-------------------\n");
	int i = 0;
	number_threads = total_threads / block_size[i];
	for (int j = 0; j < number_runs; j++) 
	{
		cudaEventRecord(start, 0);
		kernelShl<<<block_size[i] * number_multi_processors, number_threads>>>(device_A, device_B, device_C, device_D, operand);
		cudaEventRecord(stop, 0);
		cudaEventSynchronize(start);
		cudaEventSynchronize(stop);
		cudaEventElapsedTime(&time_elapsed, start, stop);
		average_time += time_elapsed;
	}
	print_results(average_time, number_runs, total_threads, iterations);
}

__global__ void kernelShr(int* A, int* B, int* C, int *D, int operand)
{
	int i = (blockIdx.x * blockDim.x) + threadIdx.x;
	int a_val = A[i];
	int b_val = B[i];
	int c_val = C[i];
	int d_val = D[i];
	repeat4096(a_val = b_val >> operand; b_val = c_val >> operand; c_val = d_val >> operand; d_val = a_val >> operand;);
	D[i] = d_val;
	C[i] = c_val;
	B[i] = b_val;
	A[i] = a_val;
}

void getShrThroughput()
{
	double average_time = 0.0;
	float time_elapsed;
	int number_threads = 0;
	int iterations = 4096;
	if (total_threads == 32 || MAX_OCCUPANCY) printf("\n--------------Shift-Right-------------------\n");
	int i = 0;
	number_threads = total_threads / block_size[i];
	for (int j = 0; j < number_runs; j++) 
	{
		cudaEventRecord(start, 0);
		kernelShr<<<block_size[i] * number_multi_processors, number_threads>>>(device_A, device_B, device_C, device_D, operand);
		cudaEventRecord(stop, 0);
		cudaEventSynchronize(start);
		cudaEventSynchronize(stop);
		cudaEventElapsedTime(&time_elapsed, start, stop);
		average_time += time_elapsed;
	}
	print_results(average_time, number_runs, total_threads, iterations);
}

#endif

int main(int argc, char **argv)
{
	size_t array_size = N * sizeof(TYPE);
	cudaEventCreate(&start);
	cudaEventCreate(&stop);

	host_A = (TYPE *) malloc(array_size);
	host_B = (TYPE *) malloc(array_size);
	host_C = (TYPE *) malloc(array_size);
	host_D = (TYPE *) malloc(array_size);		
	
	if (host_A == NULL || host_B == NULL || host_C == NULL || host_D == NULL)
		exit(1);

	//Initilize arrays
	for (int i = 0; i < N; i++) 
	{
		host_A[i] = i * 10000;
		host_B[i] = i * 1000;
		host_C[i] = i * 100;
		host_D[i] = i * 10;
	}

	cudaMalloc((void**) &device_A, array_size);
	cudaMalloc((void**) &device_B, array_size);
	cudaMalloc((void**) &device_C, array_size);
	cudaMalloc((void**) &device_D, array_size);

	//Copy ints from host to device arrays
	cudaMemcpy(device_A, host_A, array_size, cudaMemcpyHostToDevice);
	cudaMemcpy(device_B, host_B, array_size, cudaMemcpyHostToDevice);
	cudaMemcpy(device_C, host_C, array_size, cudaMemcpyHostToDevice);
	cudaMemcpy(device_D, host_D, array_size, cudaMemcpyHostToDevice);
	
	switch(DATATYPE)
	{
		case INT: printf("**************Integer********************\n"); break;
		case UINT: printf("***********Unsigned-Integer***************\n"); break;
		case FLOAT: printf("**************Float********************\n"); break;
		case DOUBLE: printf("**************Double********************\n"); break;
	}
	
#if (MAX_OCCUPANCY)
	total_threads = max_warps * threads_per_warp;
	for (int k = 0; k < 12; k++) 
	{
		switch(k) 
		{
			case 0:	getAddThroughput();	break;
			case 1: getSubtractThroughput(); break;
			case 2: getMultiplyThroughput(); break;
			case 3: getDivideThroughput(); break;
			case 4: getMultiplyAddThroughput(); break;
#if DATATYPE == INT || DATATYPE == UINT
			case 5: getVectorAddThroughput(); break;
			case 6: getRemainderThroughput(); break;
#endif
#if DATATYPE == INT
			case 7: getAndThroughput(); break;
			case 8: getOrThroughput(); break;
			case 9: getXorThroughput(); break;
			case 10: getShlThroughput(); break;
			case 11: getShrThroughput(); break;
#endif
		}
	}
#endif
#if (!MAX_OCCUPANCY)
	for (int k = 0; k < 12; k++) {			
		for (int i = 1; i <= max_warps; i++) 
		{
			total_threads = i * threads_per_warp;
			switch(k) 
			{
				case 0:	getAddThroughput();	break;
				case 1: getSubtractThroughput(); break;
				case 2: getMultiplyThroughput(); break;
				case 3: getDivideThroughput(); break;
				case 4: getMultiplyAddThroughput(); break;
#if DATATYPE == INT || DATATYPE == UINT
				case 5: getVectorAddThroughput(); break;
				case 6: getRemainderThroughput(); break;
#endif
#if DATATYPE == INT
				case 7: getAndThroughput(); break;
				case 8: getOrThroughput(); break;
				case 9: getXorThroughput(); break;
				case 10: getShlThroughput(); break;
				case 11: getShrThroughput(); break;
#endif
			}
		}
	}
#endif
	
	cudaFree(device_A);
	cudaFree(device_B);
	cudaFree(device_C);
	cudaFree(device_D);
	free(host_A);
	free(host_B);
	free(host_C);
	free(host_D);
	
	return 0;
}



