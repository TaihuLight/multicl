//
// © 2013.  Virginia Polytechnic Institute & State University
// 
// This GPU-accelerated code is based on the MPI code supplied by Pengcheng Liu of USBR.
//
/**************************************************************************/
/* Author: Kaixi Hou                                                      */
/* Organization: Department of Computer Science, Virginia Tech            */
/*                                                                        */
/* All copyrights reserved.                                               */
/*                                                                        */
/**************************************************************************/
# include <stdlib.h>
# include <stdio.h>
# include <string.h>

#ifdef __APPLE__
//# include <OpenCL/opencl.h>
#else
//# include <CL/opencl.h>
#endif
#include "mpiacc_cl.h"
//#include "papi_interface.h"
#include "RealTimer.h"
#include "switches.h"
#define CHECK_ERROR(err, str) \
	if (err != CL_SUCCESS) \
{\
	fprintf(stderr, "Error in \"%s\", %d\n", str, err); \
}

#define CHECK_NULL_ERROR(err, str) \
	if (err == NULL) \
{\
	fprintf(stderr, "Error creating memory objects in \"%s\"\n", str); \
}
//#define DISFD_DEBUG
//#define DISFD_PAPI
#define DISFD_USE_ROW_MAJOR_DATA
//#define DEVICE_FISSION
#if 1
#define DISFD_H2D_SYNC_KERNEL
#else
#define DISFD_EAGER_DATA_TRANSFER
#endif
//#define SNUCL_PERF_MODEL_OPTIMIZATION
#define SOCL_OPTIMIZATIONS
//#define DISFD_SINGLE_COMMANDQ
/***********************************************/
/* for debug: check the output                 */
/***********************************************/
#define MaxThreadsPerBlock 1024
#define NTHREADS 8

struct __dim3 {
	int x;
	int y;
	int z;
	
	__dim3() {}
	__dim3(int _x = 1, int _y = 1, int _z = 1) 
	{
		x = _x;
		y = _y;
		z = _z;
	}
};

typedef struct __dim3 dim3;

#ifdef SOCL_OPTIMIZATIONS
//extern "C" {
#include "socl.h"
//}
#endif
#ifdef __cplusplus
extern "C" {
#endif

void logger_log_timing(const char *msg, double time) 
{
	fprintf(stdout, "[Disfd] %s %g\n", msg, time);
}

void write_output(float *arr, int size, const char *filename)
{
	FILE *fp;
	if((fp = fopen(filename, "a")) == NULL)
	{
		fprintf(stderr, "File write error!\n");
	}
	int i;
	for(i = 0; i < size; i++)
	{
		fprintf(fp, "%f ", arr[i]);
		if( i%10 == 0)
			fprintf(fp, "\n");
	}
	fprintf(fp, "\n");
	fclose(fp);
}

//#define NUM_COMMAND_QUEUES	2
cl_platform_id     _cl_firstPlatform;
cl_device_id       _cl_firstDevice;
cl_device_id       *_cl_devices;
cl_context         _cl_context;
//cl_command_queue   _cl_commandQueue;
cl_command_queue _cl_commandQueues[NUM_COMMAND_QUEUES];
cl_event _cl_events[NUM_COMMAND_QUEUES];
cl_program         _cl_program;

size_t globalWorkSize[3];
size_t localWorkSize[3];

RealTimer kernelTimerStress[NUM_COMMAND_QUEUES];
RealTimer kernelTimerVelocity[NUM_COMMAND_QUEUES];
RealTimer h2dTimerStress[NUM_COMMAND_QUEUES];
RealTimer h2dTimerVelocity[NUM_COMMAND_QUEUES];
RealTimer d2hTimerStress[NUM_COMMAND_QUEUES];
RealTimer d2hTimerVelocity[NUM_COMMAND_QUEUES];

cl_kernel _cl_kernel_velocity_inner_IC;
cl_kernel _cl_kernel_velocity_inner_IIC;
cl_kernel _cl_kernel_vel_PmlX_IC;
cl_kernel _cl_kernel_vel_PmlY_IC;
cl_kernel _cl_kernel_vel_PmlX_IIC;
cl_kernel _cl_kernel_vel_PmlY_IIC;
cl_kernel _cl_kernel_vel_PmlZ_IIC;
cl_kernel _cl_kernel_stress_norm_xy_IC;
cl_kernel _cl_kernel_stress_xz_yz_IC;
cl_kernel _cl_kernel_stress_resetVars;
cl_kernel _cl_kernel_stress_norm_PmlX_IC;
cl_kernel _cl_kernel_stress_norm_PmlY_IC;
cl_kernel _cl_kernel_stress_xy_PmlX_IC;
cl_kernel _cl_kernel_stress_xy_PmlY_IC;
cl_kernel _cl_kernel_stress_xz_PmlX_IC;
cl_kernel _cl_kernel_stress_xz_PmlY_IC;
cl_kernel _cl_kernel_stress_yz_PmlX_IC;
cl_kernel _cl_kernel_stress_yz_PmlY_IC;
cl_kernel _cl_kernel_stress_norm_xy_II;
cl_kernel _cl_kernel_stress_xz_yz_IIC;
cl_kernel _cl_kernel_stress_norm_PmlX_IIC;
cl_kernel _cl_kernel_stress_norm_PmlY_II;
cl_kernel _cl_kernel_stress_norm_PmlZ_IIC;
cl_kernel _cl_kernel_stress_xy_PmlX_IIC;
cl_kernel _cl_kernel_stress_xy_PmlY_IIC;
cl_kernel _cl_kernel_stress_xy_PmlZ_II;
cl_kernel _cl_kernel_stress_xz_PmlX_IIC;
cl_kernel _cl_kernel_stress_xz_PmlY_IIC;
cl_kernel _cl_kernel_stress_xz_PmlZ_IIC;
cl_kernel _cl_kernel_stress_yz_PmlX_IIC;
cl_kernel _cl_kernel_stress_yz_PmlY_IIC;
cl_kernel _cl_kernel_stress_yz_PmlZ_IIC;

cl_kernel _cl_kernel_vel_sdx51;
cl_kernel _cl_kernel_vel_sdx52;
cl_kernel _cl_kernel_vel_sdx41;
cl_kernel _cl_kernel_vel_sdx42;
cl_kernel _cl_kernel_vel_sdy51;
cl_kernel _cl_kernel_vel_sdy52;
cl_kernel _cl_kernel_vel_sdy41;
cl_kernel _cl_kernel_vel_sdy42;
cl_kernel _cl_kernel_vel_rcx51;
cl_kernel _cl_kernel_vel_rcx52;
cl_kernel _cl_kernel_vel_rcx41;
cl_kernel _cl_kernel_vel_rcx42;
cl_kernel _cl_kernel_vel_rcy51;
cl_kernel _cl_kernel_vel_rcy52;
cl_kernel _cl_kernel_vel_rcy41;
cl_kernel _cl_kernel_vel_rcy42;
cl_kernel _cl_kernel_vel_sdx1;
cl_kernel _cl_kernel_vel_sdy1;
cl_kernel _cl_kernel_vel_sdx2;
cl_kernel _cl_kernel_vel_sdy2;
cl_kernel _cl_kernel_vel_rcx1;
cl_kernel _cl_kernel_vel_rcy1;
cl_kernel _cl_kernel_vel_rcx2;
cl_kernel _cl_kernel_vel_rcy2;
cl_kernel _cl_kernel_vel1_interpl_3vbtm;
cl_kernel _cl_kernel_vel3_interpl_3vbtm;
cl_kernel _cl_kernel_vel4_interpl_3vbtm;
cl_kernel _cl_kernel_vel5_interpl_3vbtm;
cl_kernel _cl_kernel_vel6_interpl_3vbtm;
cl_kernel _cl_kernel_vel7_interpl_3vbtm;
cl_kernel _cl_kernel_vel8_interpl_3vbtm;
cl_kernel _cl_kernel_vel9_interpl_3vbtm;
cl_kernel _cl_kernel_vel11_interpl_3vbtm;
cl_kernel _cl_kernel_vel13_interpl_3vbtm;
cl_kernel _cl_kernel_vel14_interpl_3vbtm;
cl_kernel _cl_kernel_vel15_interpl_3vbtm;
cl_kernel _cl_kernel_vel1_vxy_image_layer;
cl_kernel _cl_kernel_vel2_vxy_image_layer;
cl_kernel _cl_kernel_vel3_vxy_image_layer;
cl_kernel _cl_kernel_vel4_vxy_image_layer;
cl_kernel _cl_kernel_vel_vxy_image_layer1;
cl_kernel _cl_kernel_vel_vxy_image_layer_sdx;
cl_kernel _cl_kernel_vel_vxy_image_layer_sdy;
cl_kernel _cl_kernel_vel_vxy_image_layer_rcx;
cl_kernel _cl_kernel_vel_vxy_image_layer_rcy;
cl_kernel _cl_kernel_vel_add_dcs;
cl_kernel _cl_kernel_stress_sdx41;
cl_kernel _cl_kernel_stress_sdx42;
cl_kernel _cl_kernel_stress_sdx51;
cl_kernel _cl_kernel_stress_sdx52;
cl_kernel _cl_kernel_stress_sdy41;
cl_kernel _cl_kernel_stress_sdy42;
cl_kernel _cl_kernel_stress_sdy51;
cl_kernel _cl_kernel_stress_sdy52;
cl_kernel _cl_kernel_stress_rcx41;
cl_kernel _cl_kernel_stress_rcx42;
cl_kernel _cl_kernel_stress_rcx51;
cl_kernel _cl_kernel_stress_rcx52;
cl_kernel _cl_kernel_stress_rcy41;
cl_kernel _cl_kernel_stress_rcy42;
cl_kernel _cl_kernel_stress_rcy51;
cl_kernel _cl_kernel_stress_rcy52;
cl_kernel _cl_kernel_stress_interp_stress;
cl_kernel _cl_kernel_stress_interp1;
cl_kernel _cl_kernel_stress_interp2;
cl_kernel _cl_kernel_stress_interp3;

//device memory pointers
static cl_mem nd1_velD;
static cl_mem nd1_txyD;
static cl_mem nd1_txzD;
static cl_mem nd1_tyyD;
static cl_mem nd1_tyzD;
static cl_mem rhoD;
static cl_mem drvh1D;
static cl_mem drti1D;
static cl_mem drth1D;
static cl_mem damp1_xD;
static cl_mem damp1_yD;
static cl_mem idmat1D;
static cl_mem dxi1D;
static cl_mem dyi1D;
static cl_mem dzi1D;
static cl_mem dxh1D;
static cl_mem dyh1D;
static cl_mem dzh1D;
static cl_mem t1xxD;
static cl_mem t1xyD;
static cl_mem t1xzD;
static cl_mem t1yyD;
static cl_mem t1yzD;
static cl_mem t1zzD;
static cl_mem t1xx_pxD;
static cl_mem t1xy_pxD;
static cl_mem t1xz_pxD;
static cl_mem t1yy_pxD;
static cl_mem qt1xx_pxD;
static cl_mem qt1xy_pxD;
static cl_mem qt1xz_pxD;
static cl_mem qt1yy_pxD;
static cl_mem t1xx_pyD;
static cl_mem t1xy_pyD;
static cl_mem t1yy_pyD;
static cl_mem t1yz_pyD;
static cl_mem qt1xx_pyD;
static cl_mem qt1xy_pyD;
static cl_mem qt1yy_pyD;
static cl_mem qt1yz_pyD;
static cl_mem qt1xxD;
static cl_mem qt1xyD;
static cl_mem qt1xzD;
static cl_mem qt1yyD;
static cl_mem qt1yzD;
static cl_mem qt1zzD;
static cl_mem clamdaD;
static cl_mem cmuD;
static cl_mem epdtD;
static cl_mem qwpD;
static cl_mem qwsD;
static cl_mem qwt1D;
static cl_mem qwt2D;

static cl_mem v1xD;    //output
static cl_mem v1yD;
static cl_mem v1zD;
static cl_mem v1x_pxD;
static cl_mem v1y_pxD;
static cl_mem v1z_pxD;
static cl_mem v1x_pyD;
static cl_mem v1y_pyD;
static cl_mem v1z_pyD;

//for inner_II---------------------------------------------------------
static cl_mem nd2_velD;
static cl_mem nd2_txyD;  //int[18]
static cl_mem nd2_txzD;  //int[18]
static cl_mem nd2_tyyD;  //int[18]
static cl_mem nd2_tyzD;  //int[18]

static cl_mem drvh2D;
static cl_mem drti2D;
static cl_mem drth2D; 	//float[mw2_pml1,0:1]

static cl_mem idmat2D;
static cl_mem damp2_xD;
static cl_mem damp2_yD;
static cl_mem damp2_zD;
static cl_mem dxi2D;
static cl_mem dyi2D;
static cl_mem dzi2D;
static cl_mem dxh2D;
static cl_mem dyh2D;
static cl_mem dzh2D;
static cl_mem t2xxD;
static cl_mem t2xyD;
static cl_mem t2xzD;
static cl_mem t2yyD;
static cl_mem t2yzD;
static cl_mem t2zzD;
static cl_mem qt2xxD;
static cl_mem qt2xyD;
static cl_mem qt2xzD;
static cl_mem qt2yyD;
static cl_mem qt2yzD;
static cl_mem qt2zzD;

static cl_mem t2xx_pxD;
static cl_mem t2xy_pxD;
static cl_mem t2xz_pxD;
static cl_mem t2yy_pxD;
static cl_mem qt2xx_pxD;
static cl_mem qt2xy_pxD;
static cl_mem qt2xz_pxD;
static cl_mem qt2yy_pxD;

static cl_mem t2xx_pyD;
static cl_mem t2xy_pyD;
static cl_mem t2yy_pyD;
static cl_mem t2yz_pyD;
static cl_mem qt2xx_pyD;
static cl_mem qt2xy_pyD;
static cl_mem qt2yy_pyD;
static cl_mem qt2yz_pyD;

static cl_mem t2xx_pzD;
static cl_mem t2xz_pzD;
static cl_mem t2yz_pzD;
static cl_mem t2zz_pzD;
static cl_mem qt2xx_pzD;
static cl_mem qt2xz_pzD;
static cl_mem qt2yz_pzD;
static cl_mem qt2zz_pzD;

static cl_mem v2xD;		//output
static cl_mem v2yD;
static cl_mem v2zD;
static cl_mem v2x_pxD;
static cl_mem v2y_pxD;
static cl_mem v2z_pxD;
static cl_mem v2x_pyD;
static cl_mem v2y_pyD;
static cl_mem v2z_pyD;
static cl_mem v2x_pzD;
static cl_mem v2y_pzD;
static cl_mem v2z_pzD;

// MPI-ACC pointers
cl_mem sdx51D;
cl_mem sdx52D;
cl_mem sdx41D;
cl_mem sdx42D;
cl_mem sdy51D;
cl_mem sdy52D;
cl_mem sdy41D;
cl_mem sdy42D;
cl_mem rcx51D;
cl_mem rcx52D;
cl_mem rcx41D;
cl_mem rcx42D;
cl_mem rcy51D;
cl_mem rcy52D;
cl_mem rcy41D;
cl_mem rcy42D;

cl_mem sdx1D;
cl_mem sdy1D;
cl_mem sdx2D;
cl_mem sdy2D;
cl_mem rcx1D;
cl_mem rcy1D;
cl_mem rcx2D;
cl_mem rcy2D;

static cl_mem  cixD;
static cl_mem  ciyD;
static cl_mem  chxD;
static cl_mem  chyD;

static cl_mem index_xyz_sourceD;
static cl_mem fampD;
static cl_mem sparam2D;
static cl_mem risetD;
static cl_mem ruptmD;
static cl_mem sutmArrD;

cl_command_queue stream[2];
cl_mem  temp_gpu_arr; // used by get_gpu_arrC
int rank;


//debug----------------------
float totalTimeH2DV, totalTimeD2HV;
float totalTimeH2DS, totalTimeD2HS;
float totalTimeCompV, totalTimeCompS;
float tmpTime;
float onceTime;
float onceTime2;
struct timeval t1, t2;
int procID;
//---------------------------

void record_time(double* time);

// A GPU Array metadata Structure
struct GPUArray_Metadata {
	char* arr_name;
	int dimensions;
	int i;
	int j;
	int k;
	cl_mem dptr_1D;
	cl_mem dptr_2D;
	cl_mem dptr_3D;
} 
v1xD_meta, v2xD_meta, v1yD_meta, v2yD_meta, v1zD_meta, v2zD_meta,
	t1xxD_meta, t1xyD_meta, t1xzD_meta, t1yyD_meta, t1yzD_meta, t1zzD_meta,
	t2xxD_meta, t2xyD_meta, t2xzD_meta, t2yyD_meta, t2yzD_meta, t2zzD_meta,
	sdx1D_meta, sdx2D_meta, sdy1D_meta, sdy2D_meta,
	rcx1D_meta, rcx2D_meta, rcy1D_meta, rcy2D_meta,
	sdx41D_meta, sdx42D_meta, sdy41D_meta, sdy42D_meta,
	sdx51D_meta, sdx52D_meta, sdy51D_meta, sdy52D_meta,
	rcx41D_meta, rcx42D_meta, rcy41D_meta, rcy42D_meta,
	rcx51D_meta, rcx52D_meta, rcy51D_meta, rcy52D_meta;


/* 
   Initialises all the GPU array metadata
IMPORTANT : MUST BE IN SYNC WITH Array_Info metadata in disfd_comm.f90
*/
void init_gpuarr_metadata (int nxtop, int nytop, int nztop, int nxbtm, int nybtm, int nzbtm) 
{
	v1xD_meta = (struct GPUArray_Metadata){"v1x", 3,nztop+2, nxtop+3,  nytop+3, NULL, NULL, v1xD}; 
	v1yD_meta = (struct GPUArray_Metadata){"v1y", 3,nztop+2, nxtop+3,  nytop+3, NULL, NULL, v1yD}; 
	v1zD_meta = (struct GPUArray_Metadata){"v1z", 3,nztop+2, nxtop+3,  nytop+3, NULL, NULL, v1zD};
	v2xD_meta = (struct GPUArray_Metadata){"v2x", 3,nzbtm+1, nxbtm+3,  nybtm+3, NULL, NULL, v2xD}; 
	v2yD_meta = (struct GPUArray_Metadata){"v2y", 3,nzbtm+1, nxbtm+3,  nybtm+3, NULL, NULL, v2yD};
	v2zD_meta = (struct GPUArray_Metadata){"v2z", 3,nzbtm+1, nxbtm+3,  nybtm+3, NULL, NULL, v2zD}; 

	t1xxD_meta = (struct GPUArray_Metadata){"t1xx", 3, nztop, nxtop+3, nytop, NULL, NULL  ,  t1xxD};
	t1xyD_meta = (struct GPUArray_Metadata){"t1xy", 3, nztop, nxtop+3, nytop+3, NULL, NULL,  t1xyD};
	t1xzD_meta = (struct GPUArray_Metadata){"t1xz", 3, nztop+1, nxtop+3, nytop, NULL, NULL,  t1xzD};
	t1yyD_meta = (struct GPUArray_Metadata){"t1yy", 3, nztop, nxtop, nytop+3, NULL, NULL  ,  t1yyD};
	t1yzD_meta = (struct GPUArray_Metadata){"t1yz", 3, nztop+1, nxtop, nytop+3, NULL, NULL,  t1yzD};
	t1zzD_meta = (struct GPUArray_Metadata){"t1zz", 3, nztop, nxtop, nytop, NULL, NULL    ,  t1zzD};

	t2xxD_meta = (struct GPUArray_Metadata){"t2xx", 3, nzbtm, nxbtm+3, nybtm, NULL, NULL  ,  t2xxD};
	t2xyD_meta = (struct GPUArray_Metadata){"t2xy", 3, nzbtm, nxbtm+3, nybtm+3, NULL, NULL,  t2xyD};
	t2xzD_meta = (struct GPUArray_Metadata){"t2xz", 3, nzbtm+1, nxbtm+3, nybtm, NULL, NULL,  t2xzD};
	t2yyD_meta = (struct GPUArray_Metadata){"t2yy", 3, nzbtm, nxbtm, nybtm+3, NULL, NULL  ,  t2yyD};
	t2yzD_meta = (struct GPUArray_Metadata){"t2yz", 3, nzbtm+1, nxbtm, nybtm+3, NULL, NULL,  t2yzD};
	t2zzD_meta = (struct GPUArray_Metadata){"t2zz", 3, nzbtm+1, nxbtm, nybtm, NULL, NULL  ,  t2zzD};

	sdx1D_meta = (struct GPUArray_Metadata){"sdx1", 1, nytop+6, 0, 0, sdx1D,  NULL, NULL}; 
	sdx2D_meta = (struct GPUArray_Metadata){"sdx2", 1, nytop+6, 0, 0, sdx2D,  NULL, NULL}; 
	sdy1D_meta = (struct GPUArray_Metadata){"sdy1", 1, nxtop+6, 0, 0, sdy1D,  NULL, NULL}; 
	sdy2D_meta = (struct GPUArray_Metadata){"sdy2", 1, nxtop+6, 0, 0, sdy2D,  NULL, NULL}; 
	rcx1D_meta = (struct GPUArray_Metadata){"rcx1", 1, nytop+6, 0, 0, rcx1D,  NULL, NULL}; 
	rcx2D_meta = (struct GPUArray_Metadata){"rcx2", 1, nytop+6, 0, 0, rcx2D,  NULL, NULL}; 
	rcy1D_meta = (struct GPUArray_Metadata){"rcy1", 1, nxtop+6, 0, 0, rcy1D,  NULL, NULL}; 
	rcy2D_meta = (struct GPUArray_Metadata){"rcy2", 1, nxtop+6, 0, 0, rcy2D,  NULL, NULL}; 

	sdx41D_meta = (struct GPUArray_Metadata){"sdx41", 3, nztop, nytop, 4, NULL, NULL, sdx41D};
	sdx42D_meta = (struct GPUArray_Metadata){"sdx42", 3, nzbtm, nybtm, 4, NULL, NULL, sdx42D};
	sdy41D_meta = (struct GPUArray_Metadata){"sdy41", 3, nztop, nxtop, 4, NULL, NULL, sdy41D};
	sdy42D_meta = (struct GPUArray_Metadata){"sdy42", 3, nzbtm, nxbtm, 4, NULL, NULL, sdy42D};
	sdx51D_meta = (struct GPUArray_Metadata){"sdx51", 3, nztop, nytop, 5, NULL, NULL, sdx51D};
	sdx52D_meta = (struct GPUArray_Metadata){"sdx52", 3, nzbtm, nybtm, 5, NULL, NULL, sdx52D};
	sdy51D_meta = (struct GPUArray_Metadata){"sdy51", 3, nztop, nxtop, 5, NULL, NULL, sdy51D};
	sdy52D_meta = (struct GPUArray_Metadata){"sdy52", 3, nzbtm, nxbtm, 5, NULL, NULL, sdy52D};

	rcx41D_meta = (struct GPUArray_Metadata){"rcx41", 3, nztop, nytop, 4, NULL, NULL, rcx41D};
	rcx42D_meta = (struct GPUArray_Metadata){"rcx42", 3, nzbtm, nybtm, 4, NULL, NULL, rcx42D};
	rcy41D_meta = (struct GPUArray_Metadata){"rcy41", 3, nztop, nxtop, 4, NULL, NULL, rcy41D};
	rcy42D_meta = (struct GPUArray_Metadata){"rcy42", 3, nzbtm, nxbtm, 4, NULL, NULL, rcy42D};
	rcx51D_meta = (struct GPUArray_Metadata){"rcx51", 3, nztop, nytop, 5, NULL, NULL, rcx51D};
	rcx52D_meta = (struct GPUArray_Metadata){"rcx52", 3, nzbtm, nybtm, 5, NULL, NULL, rcx52D};
	rcy51D_meta = (struct GPUArray_Metadata){"rcy51", 3, nztop, nxtop, 5, NULL, NULL, rcy51D};
	rcy52D_meta = (struct GPUArray_Metadata){"rcy52", 3, nzbtm, nxbtm, 5, NULL, NULL, rcy52D};
}
/* 
   Get GPU Array Metadata from the  array id
   The id's for Arrays is set in disfd_comm.f90
   */
struct GPUArray_Metadata getGPUArray_Metadata(int id) {
	switch(id) {
		case  10: return sdx1D_meta; 
		case  11: return sdy1D_meta; 
		case  12: return sdx2D_meta; 
		case  13: return sdy2D_meta; 
		case  14: return rcx1D_meta; 
		case  15: return rcy1D_meta; 
		case  16: return rcx2D_meta; 
		case  17: return rcy2D_meta; 
		case  40: return v1xD_meta; 
		case  41: return v1yD_meta; 
		case  42: return v1zD_meta; 
		case  43: return v2xD_meta; 
		case  44: return v2yD_meta; 
		case  45: return v2zD_meta; 
		case  46: return t1xxD_meta; 
		case  47: return t1xyD_meta; 
		case  48: return t1xzD_meta; 
		case  49: return t1yyD_meta; 
		case  50: return t1yzD_meta; 
		case  51: return t1zzD_meta; 
		case  52: return t2xxD_meta; 
		case  53: return t2xyD_meta; 
		case  54: return t2xzD_meta; 
		case  55: return t2yyD_meta; 
		case  56: return t2yzD_meta; 
		case  57: return t2zzD_meta; 
		case  80: return sdx51D_meta; 
		case  81: return sdy51D_meta; 
		case  82: return sdx52D_meta; 
		case  83: return sdy52D_meta; 
		case  84: return sdx41D_meta; 
		case  85: return sdy41D_meta; 
		case  86: return sdx42D_meta; 
		case  87: return sdy42D_meta; 
		case  88: return rcx51D_meta; 
		case  89: return rcy51D_meta; 
		case  90: return rcx52D_meta; 
		case  91: return rcy52D_meta; 
		case  92: return rcx41D_meta; 
		case  93: return rcy41D_meta; 
		case  94: return rcx42D_meta; 
		case  95: return rcy42D_meta; 
		default : printf("Wrong pointer id\n"); exit(0);
	}    
}

/* 
   Get GPU Array pointer from its id
   The id's for Arrays is set in disfd_comm.f90
   */
cl_mem getDevicePointer(int id) {
	struct GPUArray_Metadata gpumeta= getGPUArray_Metadata(id);
	//	printf("MPI-ACC ptr info for ID %d: ( %s %d %p %p %p )\n", id, gpumeta.arr_name, gpumeta.dimensions, 
	//					gpumeta.dptr_1D, gpumeta.dptr_2D, gpumeta.dptr_3D);
	switch(gpumeta.dimensions) {
		case 1: 
			//printf("MPI using Dev ptr: %p\n", gpumeta.dptr_1D);
			return gpumeta.dptr_1D; 
		case 2: 
			//printf("MPI using Dev ptr: %p\n", gpumeta.dptr_2D);
			return gpumeta.dptr_2D; 
		case 3: 
			//printf("MPI using Dev ptr: %p\n", gpumeta.dptr_3D);
			return gpumeta.dptr_3D; 
		default : printf("Wrong array dimensions\n"); exit(0);
	}

}

size_t LoadProgramSource(const char *filename1, const char **progSrc) 
{
	FILE *f1 = fopen(filename1, "r");
	fseek(f1, 0, SEEK_END);
	size_t len1 = (size_t) ftell(f1);

	*progSrc = (const char *) malloc(sizeof(char)*(len1));

	rewind(f1);
	fread((void *) *progSrc, len1, 1, f1);
	fclose(f1);
	return len1;
}
/*
size_t LoadProgramSource(const char *filename1, const char *filename2, const char **progSrc) 
{
	FILE *f1 = fopen(filename1, "r");
	fseek(f1, 0, SEEK_END);
	size_t len1 = (size_t) ftell(f1);
	FILE *f2 = fopen(filename2, "r");
	fseek(f2, 0, SEEK_END);
	size_t len2 = (size_t) ftell(f2);

	*progSrc = (const char *) malloc(sizeof(char)*(len1+len2));

	rewind(f1);
	fread((void *) *progSrc, len1, 1, f1);
	fclose(f1);
	rewind(f2);
	fread((void *) ((*progSrc) + len1), len2, 1, f2);
	fclose(f2);
	return len1+len2;
}
*/
// opencl initialization
void init_cl(int *deviceID) 
{
	int i;
#ifdef DISFD_PAPI
	papi_init();
	printf("PAPI Init Done\n");
#endif
	cl_int errNum;
	cl_int err;
	// launch the device
	size_t progLen;
	const char *progSrc;
	cl_uint num_platforms;
	size_t            platform_name_size;
	char*             platform_name;
	cl_platform_id *platforms;
	const char *PLATFORM_NAME = "SnuCL Single";
	for(i = 0; i < NUM_COMMAND_QUEUES; i++) {
		Init(&kernelTimerStress[i]);
		Init(&kernelTimerVelocity[i]);
		Init(&h2dTimerStress[i]);
		Init(&h2dTimerVelocity[i]);
		Init(&d2hTimerStress[i]);
		Init(&d2hTimerVelocity[i]);
	}
	errNum = clGetPlatformIDs(0, NULL, &num_platforms);
	CHECK_ERROR(errNum, "Platform Count");
	platforms = (cl_platform_id*)malloc(sizeof(cl_platform_id) * num_platforms);
	err = clGetPlatformIDs(num_platforms, platforms, NULL);
	CHECK_ERROR(err, "Platform Count");

	for (i = 0; i < num_platforms; i++) {
		err = clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, 0, NULL,
				&platform_name_size);
		CHECK_ERROR(err, "Platform Info");

		platform_name = (char*)malloc(sizeof(char) * platform_name_size);
		err = clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, platform_name_size,
				platform_name, NULL);
		CHECK_ERROR(err, "Platform Info");

		printf("Platform %d: %s\n", i, platform_name);
		if (strcmp(platform_name, PLATFORM_NAME) == 0)
		{
			printf("Choosing Platform %d: %s\n", i, platform_name);
			_cl_firstPlatform = platforms[i];
		}
		free(platform_name);
	}

	if (_cl_firstPlatform == NULL) {
		printf("%s platform is not found.\n", PLATFORM_NAME);
		//exit(EXIT_FAILURE);
	}
	/*errNum = clGetPlatformIDs(1, &_cl_firstPlatform, NULL);
	  if(errNum != CL_SUCCESS)
	  {
	  fprintf(stderr, "Failed to find any available OpenCL platform!\n");
	  }*/
	cl_uint num_devices;
#ifdef DEVICE_FISSION
  cl_device_id *gpu_devices = NULL;
  cl_device_id *out_cpu_devices = NULL;
  cl_uint num_gpu_devices = 0;
  int num_partitions = 0;
  cl_device_type device_type = CL_DEVICE_TYPE_ALL;
  if(device_type == CL_DEVICE_TYPE_GPU || device_type == CL_DEVICE_TYPE_ALL) {
	  clGetDeviceIDs(_cl_firstPlatform, CL_DEVICE_TYPE_GPU, 0, 
			  NULL, &num_gpu_devices);
	  CHECK_ERROR(errNum, "clGetDeviceIDs()");
	  gpu_devices = (cl_device_id *)malloc(sizeof(cl_device_id) * 
			  num_gpu_devices);
	  clGetDeviceIDs(_cl_firstPlatform, CL_DEVICE_TYPE_GPU, num_gpu_devices,
			  gpu_devices, NULL);
	  CHECK_ERROR(errNum, "clGetDeviceIDs()");
  }
  if(device_type == CL_DEVICE_TYPE_CPU || device_type == CL_DEVICE_TYPE_ALL) {
	  num_partitions = 2;
	  cl_device_id cpu_device;
	  out_cpu_devices = (cl_device_id *)malloc(sizeof(cl_device_id) * num_partitions);

	  errNum = clGetDeviceIDs(_cl_firstPlatform, CL_DEVICE_TYPE_CPU, 1, &cpu_device, NULL);
	  CHECK_ERROR(errNum, "clGetDeviceIDs()");
#if 0
	  cl_device_affinity_domain supported_affinity = 0;
	  errNum = clGetDeviceInfo(cpu_device,
			  CL_DEVICE_PARTITION_AFFINITY_DOMAIN,
			  sizeof(cl_device_affinity_domain),
			  &supported_affinity,
			  NULL);
	  CHECK_ERROR(errNum, "clGetDeviceInfo");
	  printf("Supported Affinity: %x\n", supported_affinity);

#endif

	  /*cl_device_partition_property  partition_properties[3] = {
		  CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN,
		  CL_DEVICE_AFFINITY_DOMAIN_NEXT_PARTITIONABLE,
		  //		CL_DEVICE_AFFINITY_DOMAIN_NUMA,
		  0 };*/
	  cl_device_partition_property  partition_properties[3] = {
	  	  CL_DEVICE_PARTITION_EQUALLY, 8, 0 };
	  cl_uint ret_device_count;
	  errNum = clCreateSubDevices(cpu_device,
			  partition_properties,
			  num_partitions,
			  out_cpu_devices,
			  &ret_device_count);
	  CHECK_ERROR(errNum, "clCreateSubDevices()");
  }
  num_devices = num_gpu_devices + num_partitions;
  _cl_devices = (cl_device_id *)malloc(sizeof(cl_device_id) * num_devices);

  int dev_id = 0;
  for(i = 0; i < num_partitions; i++) {
  	_cl_devices[dev_id++] = out_cpu_devices[i];
  }
  for(i = 0; i < num_gpu_devices; i++) {
  	_cl_devices[dev_id++] = gpu_devices[i];
  }
  if(gpu_devices) free(gpu_devices);
  if(out_cpu_devices) free(out_cpu_devices);
#else
    errNum = clGetDeviceIDs(_cl_firstPlatform, CL_DEVICE_TYPE_ALL, 0, NULL, &num_devices);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Failed to find any available OpenCL device!\n");
	}
//	num_devices = 2;
	_cl_devices = (cl_device_id *)malloc(sizeof(cl_device_id) * num_devices);
	errNum = clGetDeviceIDs(_cl_firstPlatform, CL_DEVICE_TYPE_ALL, num_devices, _cl_devices, NULL);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Failed to find any available OpenCL device!\n");
	}
#endif
#ifdef SNUCL_PERF_MODEL_OPTIMIZATION
	cl_context_properties props[5] = {
		CL_CONTEXT_PLATFORM,
		(cl_context_properties) _cl_firstPlatform,
		CL_CONTEXT_SCHEDULER,
		CL_CONTEXT_SCHEDULER_CODE_SEGMENTED_PERF_MODEL,
		//CL_CONTEXT_SCHEDULER_PERF_MODEL,
		//CL_CONTEXT_SCHEDULER_FIRST_EPOCH_BASED_PERF_MODEL,
		//CL_CONTEXT_SCHEDULER_ALL_EPOCH_BASED_PERF_MODEL,
		0 };
	_cl_context = clCreateContext(props, num_devices, _cl_devices, NULL, NULL, &errNum);
#elif defined(SOCL_OPTIMIZATIONS)
	cl_context_properties props[5] = {
		CL_CONTEXT_PLATFORM,
		(cl_context_properties)platform,
		CL_CONTEXT_SCHEDULER_SOCL,
		"dmda",
		0 };
    _cl_context = clCreateContext(props, num_devices, _cl_devices, NULL, NULL, &errNum);
#else
	_cl_context = clCreateContext(NULL, num_devices, _cl_devices, NULL, NULL, &errNum);
#endif
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Failed to create GPU context!\n");
	}

	//_cl_commandQueue = clCreateCommandQueue(_cl_context, _cl_firstDevice, CL_QUEUE_PROFILING_ENABLE, NULL);
#ifndef DISFD_SINGLE_COMMANDQ
	for(i = 0; i < NUM_COMMAND_QUEUES; i++)
	{
		int chosen_dev_id = 0; 
		//= (i + 2) % num_devices;
		if(i == 0) 
		{
			char *foo = getenv("SNUCL_DEV_0");
			if(foo != NULL)
				chosen_dev_id = atoi(foo);
		}
		else if (i == 1) 
		{
			char *foo = getenv("SNUCL_DEV_1");
			if(foo != NULL)
				chosen_dev_id = atoi(foo);
		}
		printf("[OpenCL] %dth command queue uses Dev ID %d\n", i, chosen_dev_id);
#ifdef SOCL_OPTIMIZATIONS
		_cl_commandQueues[i] = clCreateCommandQueue(_cl_context, 
				NULL, 
#else
		_cl_commandQueues[i] = clCreateCommandQueue(_cl_context, 
				_cl_devices[chosen_dev_id], 
#endif
#ifdef SNUCL_PERF_MODEL_OPTIMIZATION
				//CL_QUEUE_AUTO_DEVICE_SELECTION | 
				//CL_QUEUE_ITERATIVE | 
				//CL_QUEUE_IO_INTENSIVE | 
				//CL_QUEUE_COMPUTE_INTENSIVE | 
#endif
				CL_QUEUE_PROFILING_ENABLE, NULL);
		if(_cl_commandQueues[i] == NULL)
		{
			fprintf(stderr, "Failed to create commandQueue for the %dth device!\n", i);
		}
	}
#else
	int chosen_dev_id = 0;
	char *foo = getenv("SNUCL_DEV_0");
	if(foo != NULL)
		chosen_dev_id = atoi(foo);
	printf("[OpenCL] All command queue uses Dev ID %d\n", chosen_dev_id);
#ifdef SOCL_OPTIMIZATIONS
	_cl_commandQueues[0] = clCreateCommandQueue(_cl_context, NULL, 
#else
	_cl_commandQueues[0] = clCreateCommandQueue(_cl_context, _cl_devices[chosen_dev_id], 
#endif
#ifdef SNUCL_PERF_MODEL_OPTIMIZATION
			//CL_QUEUE_AUTO_DEVICE_SELECTION | 
			//CL_QUEUE_ITERATIVE | 
			//CL_QUEUE_IO_INTENSIVE | 
			//CL_QUEUE_COMPUTE_INTENSIVE | 
#endif
			CL_QUEUE_PROFILING_ENABLE, NULL);
	if(_cl_commandQueues[0] == NULL)
	{
		fprintf(stderr, "Failed to create commandQueue for the 0th device!\n");
	}
	errNum = clRetainCommandQueue(_cl_commandQueues[0]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Failed to retain cmd queue\n");
	}
	_cl_commandQueues[1] = _cl_commandQueues[0];
#endif
#ifdef DISFD_USE_ROW_MAJOR_DATA
	progLen = LoadProgramSource("disfd_sub_opt.cl", &progSrc);
	//progLen = LoadProgramSource("disfd_sub_opt.cl", "disfd_gpu_marshaling.cl", &progSrc);
#else
	progLen = LoadProgramSource("disfd_sub.cl", &progSrc);
	//progLen = LoadProgramSource("disfd_sub.cl", "disfd_gpu_marshaling.cl", &progSrc);
#endif
	printf("[OpenCL] program loaded\n");
	_cl_program = clCreateProgramWithSource(_cl_context, 1, &progSrc, &progLen, NULL);
	if(_cl_program == NULL)
	{
		fprintf(stderr, "Failed to create CL program from source!\n");
	}

	free((void *)progSrc);
	// -cl-mad-enable
	// -cl-opt-disable
	errNum = clBuildProgram(_cl_program, 0, NULL, "", NULL, NULL);
	printf("[OpenCL] program built!\n");
	//errNum = clBuildProgram(_cl_program, num_devices, _cl_devices, "", NULL, NULL);
	//errNum = clBuildProgram(_cl_program, 1, &_cl_firstDevice, "-cl-opt-disable", NULL, NULL);
	if(errNum != CL_SUCCESS)
	{
		for(i = 0; i < num_devices; i++)
		{
			char buildLog[16384];
			cl_int err = clGetProgramBuildInfo(_cl_program, _cl_devices[i], CL_PROGRAM_BUILD_LOG, sizeof(buildLog), buildLog, NULL);
			fprintf(stderr, "Build Log for device %d:\n", i);
			buildLog[16383] = '\0';
			fprintf(stderr, "%s\n", buildLog);
			// clReleaseProgram(_cl_program);
		}
	}

	// create MPI-ACC related marshaling kernels
#ifdef DISFD_GPU_MARSHALING
	_cl_kernel_vel_sdx51 = clCreateKernel(_cl_program, "vel_sdx51", NULL);
	_cl_kernel_vel_sdx52 = clCreateKernel(_cl_program, "vel_sdx52", NULL);
	_cl_kernel_vel_sdx41 = clCreateKernel(_cl_program, "vel_sdx41", NULL);
	_cl_kernel_vel_sdx42 = clCreateKernel(_cl_program, "vel_sdx42", NULL);
	_cl_kernel_vel_sdy51 = clCreateKernel(_cl_program, "vel_sdy51", NULL);
	_cl_kernel_vel_sdy52 = clCreateKernel(_cl_program, "vel_sdy52", NULL);
	_cl_kernel_vel_sdy41 = clCreateKernel(_cl_program, "vel_sdy41", NULL);
	_cl_kernel_vel_sdy42 = clCreateKernel(_cl_program, "vel_sdy42", NULL);
	_cl_kernel_vel_rcx51 = clCreateKernel(_cl_program, "vel_rcx51", NULL);
	_cl_kernel_vel_rcx52 = clCreateKernel(_cl_program, "vel_rcx52", NULL);
	_cl_kernel_vel_rcx41 = clCreateKernel(_cl_program, "vel_rcx41", NULL);
	_cl_kernel_vel_rcx42 = clCreateKernel(_cl_program, "vel_rcx42", NULL);
	_cl_kernel_vel_rcy51 = clCreateKernel(_cl_program, "vel_rcy51", NULL);
	_cl_kernel_vel_rcy52 = clCreateKernel(_cl_program, "vel_rcy52", NULL);
	_cl_kernel_vel_rcy41 = clCreateKernel(_cl_program, "vel_rcy41", NULL);
	_cl_kernel_vel_rcy42 = clCreateKernel(_cl_program, "vel_rcy42", NULL);
	_cl_kernel_vel_sdx1 = clCreateKernel(_cl_program, "vel_sdx1", NULL);
	_cl_kernel_vel_sdy1 = clCreateKernel(_cl_program, "vel_sdy1", NULL);
	_cl_kernel_vel_sdx2 = clCreateKernel(_cl_program, "vel_sdx2", NULL);
	_cl_kernel_vel_sdy2 = clCreateKernel(_cl_program, "vel_sdy2", NULL);
	_cl_kernel_vel_rcx1 = clCreateKernel(_cl_program, "vel_rcx1", NULL);
	_cl_kernel_vel_rcy1 = clCreateKernel(_cl_program, "vel_rcy1", NULL);
	_cl_kernel_vel_rcx2 = clCreateKernel(_cl_program, "vel_rcx2", NULL);
	_cl_kernel_vel_rcy2 = clCreateKernel(_cl_program, "vel_rcy2", NULL);
	_cl_kernel_vel1_interpl_3vbtm = clCreateKernel(_cl_program, "vel1_interpl_3vbtm", NULL);
	_cl_kernel_vel3_interpl_3vbtm = clCreateKernel(_cl_program, "vel3_interpl_3vbtm", NULL);
	_cl_kernel_vel4_interpl_3vbtm = clCreateKernel(_cl_program, "vel4_interpl_3vbtm", NULL);
	_cl_kernel_vel5_interpl_3vbtm = clCreateKernel(_cl_program, "vel5_interpl_3vbtm", NULL);
	_cl_kernel_vel6_interpl_3vbtm = clCreateKernel(_cl_program, "vel6_interpl_3vbtm", NULL);
	_cl_kernel_vel7_interpl_3vbtm = clCreateKernel(_cl_program, "vel7_interpl_3vbtm", NULL);
	_cl_kernel_vel8_interpl_3vbtm = clCreateKernel(_cl_program, "vel8_interpl_3vbtm", NULL);
	_cl_kernel_vel9_interpl_3vbtm = clCreateKernel(_cl_program, "vel9_interpl_3vbtm", NULL);
	_cl_kernel_vel11_interpl_3vbtm = clCreateKernel(_cl_program, "vel11_interpl_3vbtm", NULL);
	_cl_kernel_vel13_interpl_3vbtm = clCreateKernel(_cl_program, "vel13_interpl_3vbtm", NULL);
	_cl_kernel_vel14_interpl_3vbtm = clCreateKernel(_cl_program, "vel14_interpl_3vbtm", NULL);
	_cl_kernel_vel15_interpl_3vbtm = clCreateKernel(_cl_program, "vel15_interpl_3vbtm", NULL);
	_cl_kernel_vel1_vxy_image_layer = clCreateKernel(_cl_program, "vel1_vxy_image_layer", NULL);
	_cl_kernel_vel2_vxy_image_layer = clCreateKernel(_cl_program, "vel2_vxy_image_layer", NULL);
	_cl_kernel_vel3_vxy_image_layer = clCreateKernel(_cl_program, "vel3_vxy_image_layer", NULL);
	_cl_kernel_vel4_vxy_image_layer = clCreateKernel(_cl_program, "vel4_vxy_image_layer", NULL);
	//_cl_kernel_vel_vxy_image_layer1 = clCreateKernel(_cl_program, "vel_vxy_image_layer1", NULL);
	_cl_kernel_vel_vxy_image_layer_sdx = clCreateKernel(_cl_program, "vel_vxy_image_layer_sdx", NULL);
	_cl_kernel_vel_vxy_image_layer_sdy = clCreateKernel(_cl_program, "vel_vxy_image_layer_sdy", NULL);
	_cl_kernel_vel_vxy_image_layer_rcx = clCreateKernel(_cl_program, "vel_vxy_image_layer_rcx", NULL);
	_cl_kernel_vel_vxy_image_layer_rcy = clCreateKernel(_cl_program, "vel_vxy_image_layer_rcy", NULL);
	_cl_kernel_vel_add_dcs = clCreateKernel(_cl_program, "vel_add_dcs", NULL);
	_cl_kernel_stress_sdx41 = clCreateKernel(_cl_program, "stress_sdx41", NULL);
	_cl_kernel_stress_sdx42 = clCreateKernel(_cl_program, "stress_sdx42", NULL);
	_cl_kernel_stress_sdx51 = clCreateKernel(_cl_program, "stress_sdx51", NULL);
	_cl_kernel_stress_sdx52 = clCreateKernel(_cl_program, "stress_sdx52", NULL);
	_cl_kernel_stress_sdy41 = clCreateKernel(_cl_program, "stress_sdy41", NULL);
	_cl_kernel_stress_sdy42 = clCreateKernel(_cl_program, "stress_sdy42", NULL);
	_cl_kernel_stress_sdy51 = clCreateKernel(_cl_program, "stress_sdy51", NULL);
	_cl_kernel_stress_sdy52 = clCreateKernel(_cl_program, "stress_sdy52", NULL);
	_cl_kernel_stress_rcx41 = clCreateKernel(_cl_program, "stress_rcx41", NULL);
	_cl_kernel_stress_rcx42 = clCreateKernel(_cl_program, "stress_rcx42", NULL);
	_cl_kernel_stress_rcx51 = clCreateKernel(_cl_program, "stress_rcx51", NULL);
	_cl_kernel_stress_rcx52 = clCreateKernel(_cl_program, "stress_rcx52", NULL);
	_cl_kernel_stress_rcy41 = clCreateKernel(_cl_program, "stress_rcy41", NULL);
	_cl_kernel_stress_rcy42 = clCreateKernel(_cl_program, "stress_rcy42", NULL);
	_cl_kernel_stress_rcy51 = clCreateKernel(_cl_program, "stress_rcy51", NULL);
	_cl_kernel_stress_rcy52 = clCreateKernel(_cl_program, "stress_rcy52", NULL);
	_cl_kernel_stress_interp_stress = clCreateKernel(_cl_program, "stress_interp_stress", NULL);
	_cl_kernel_stress_interp1 = clCreateKernel(_cl_program, "stress_interp1", NULL);
	_cl_kernel_stress_interp2 = clCreateKernel(_cl_program, "stress_interp2", NULL);
	_cl_kernel_stress_interp3 = clCreateKernel(_cl_program, "stress_interp3", NULL);
	if(_cl_kernel_vel_sdx51 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_sdx51!\n");
	if(_cl_kernel_vel_sdx52 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_sdx52!\n");
	if(_cl_kernel_vel_sdx41 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_sdx41!\n");
	if(_cl_kernel_vel_sdx42 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_sdx42!\n");
	if(_cl_kernel_vel_sdy51 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_sdy51!\n");
	if(_cl_kernel_vel_sdy52 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_sdy52!\n");
	if(_cl_kernel_vel_sdy41 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_sdy41!\n");
	if(_cl_kernel_vel_sdy42 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_sdy42!\n");
	if(_cl_kernel_vel_rcx51 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_rcx51!\n");
	if(_cl_kernel_vel_rcx52 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_rcx52!\n");
	if(_cl_kernel_vel_rcx41 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_rcx41!\n");
	if(_cl_kernel_vel_rcx42 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_rcx42!\n");
	if(_cl_kernel_vel_rcy51 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_rcy51!\n");
	if(_cl_kernel_vel_rcy52 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_rcy52!\n");
	if(_cl_kernel_vel_rcy41 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_rcy41!\n");
	if(_cl_kernel_vel_rcy42 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_rcy42!\n");
	if(_cl_kernel_vel_sdx1 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_sdx1!\n");
	if(_cl_kernel_vel_sdy1 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_sdy1!\n");
	if(_cl_kernel_vel_sdx2 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_sdx2!\n");
	if(_cl_kernel_vel_sdy2 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_sdy2!\n");
	if(_cl_kernel_vel_rcx1 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_rcx1!\n");
	if(_cl_kernel_vel_rcy1 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_rcy1!\n");
	if(_cl_kernel_vel_rcx2 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_rcx2!\n");
	if(_cl_kernel_vel_rcy2 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_rcy2!\n");
	if(_cl_kernel_vel1_interpl_3vbtm == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel1_interpl_3vbtm!\n");
	if(_cl_kernel_vel3_interpl_3vbtm == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel3_interpl_3vbtm!\n");
	if(_cl_kernel_vel4_interpl_3vbtm == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel4_interpl_3vbtm!\n");
	if(_cl_kernel_vel5_interpl_3vbtm == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel5_interpl_3vbtm!\n");
	if(_cl_kernel_vel6_interpl_3vbtm == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel6_interpl_3vbtm!\n");
	if(_cl_kernel_vel7_interpl_3vbtm == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel7_interpl_3vbtm!\n");
	if(_cl_kernel_vel8_interpl_3vbtm == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel8_interpl_3vbtm!\n");
	if(_cl_kernel_vel9_interpl_3vbtm == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel9_interpl_3vbtm!\n");
	if(_cl_kernel_vel11_interpl_3vbtm == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel11_interpl_3vbtm!\n");
	if(_cl_kernel_vel13_interpl_3vbtm == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel13_interpl_3vbtm!\n");
	if(_cl_kernel_vel14_interpl_3vbtm == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel14_interpl_3vbtm!\n");
	if(_cl_kernel_vel15_interpl_3vbtm == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel15_interpl_3vbtm!\n");
	if(_cl_kernel_vel1_vxy_image_layer == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel1_vxy_image_layer!\n");
	if(_cl_kernel_vel2_vxy_image_layer == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel2_vxy_image_layer!\n");
	if(_cl_kernel_vel3_vxy_image_layer == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel3_vxy_image_layer!\n");
	if(_cl_kernel_vel4_vxy_image_layer == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel4_vxy_image_layer!\n");
	//if(_cl_kernel_vel_vxy_image_layer1 == NULL)
	//	fprintf(stderr, "Failed to create kernel _cl_kernel_vel_vxy_image_layer1!\n");
	if(_cl_kernel_vel_vxy_image_layer_sdx == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_vxy_image_layer_sdx!\n");
	if(_cl_kernel_vel_vxy_image_layer_sdy == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_vxy_image_layer_sdy!\n");
	if(_cl_kernel_vel_vxy_image_layer_rcx == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_vxy_image_layer_rcx!\n");
	if(_cl_kernel_vel_vxy_image_layer_rcy == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_vxy_image_layer_rcy!\n");
	if(_cl_kernel_vel_add_dcs == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_add_dcs!\n");
	if(_cl_kernel_stress_sdx41 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_sdx41!\n");
	if(_cl_kernel_stress_sdx42 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_sdx42!\n");
	if(_cl_kernel_stress_sdx51 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_sdx51!\n");
	if(_cl_kernel_stress_sdx52 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_sdx52!\n");
	if(_cl_kernel_stress_sdy41 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_sdy41!\n");
	if(_cl_kernel_stress_sdy42 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_sdy42!\n");
	if(_cl_kernel_stress_sdy51 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_sdy51!\n");
	if(_cl_kernel_stress_sdy52 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_sdy52!\n");
	if(_cl_kernel_stress_rcx41 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_rcx41!\n");
	if(_cl_kernel_stress_rcx42 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_rcx42!\n");
	if(_cl_kernel_stress_rcx51 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_rcx51!\n");
	if(_cl_kernel_stress_rcx52 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_rcx52!\n");
	if(_cl_kernel_stress_rcy41 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_rcy41!\n");
	if(_cl_kernel_stress_rcy42 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_rcy42!\n");
	if(_cl_kernel_stress_rcy51 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_rcy51!\n");
	if(_cl_kernel_stress_rcy52 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_rcy52!\n");
	if(_cl_kernel_stress_interp_stress == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_interp_stress!\n");
	if(_cl_kernel_stress_interp1 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_interp1!\n");
	if(_cl_kernel_stress_interp2 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_interp2!\n");
	if(_cl_kernel_stress_interp3 == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_interp3!\n");
#endif
	// create the kernels
	_cl_kernel_velocity_inner_IC   	= clCreateKernel(_cl_program,"velocity_inner_IC"   	,NULL);
	_cl_kernel_velocity_inner_IIC  	= clCreateKernel(_cl_program,"velocity_inner_IIC"  	,NULL);
	_cl_kernel_vel_PmlX_IC         	= clCreateKernel(_cl_program,"vel_PmlX_IC"         	,NULL);
	_cl_kernel_vel_PmlY_IC         	= clCreateKernel(_cl_program,"vel_PmlY_IC"         	,NULL);
	_cl_kernel_vel_PmlX_IIC        	= clCreateKernel(_cl_program,"vel_PmlX_IIC"        	,NULL);
	_cl_kernel_vel_PmlY_IIC        	= clCreateKernel(_cl_program,"vel_PmlY_IIC"        	,NULL);
	_cl_kernel_vel_PmlZ_IIC        	= clCreateKernel(_cl_program,"vel_PmlZ_IIC"        	,NULL);
	_cl_kernel_stress_norm_xy_IC   	= clCreateKernel(_cl_program,"stress_norm_xy_IC"   	,NULL);
	_cl_kernel_stress_xz_yz_IC    	= clCreateKernel(_cl_program,"stress_xz_yz_IC"     	,NULL);
	_cl_kernel_stress_resetVars    	= clCreateKernel(_cl_program,"stress_resetVars"    	,NULL);
	_cl_kernel_stress_norm_PmlX_IC 	= clCreateKernel(_cl_program,"stress_norm_PmlX_IC" 	,NULL);
	_cl_kernel_stress_norm_PmlY_IC 	= clCreateKernel(_cl_program,"stress_norm_PmlY_IC" 	,NULL);
	_cl_kernel_stress_xy_PmlX_IC   	= clCreateKernel(_cl_program,"stress_xy_PmlX_IC"   	,NULL);
	_cl_kernel_stress_xy_PmlY_IC   	= clCreateKernel(_cl_program,"stress_xy_PmlY_IC"   	,NULL);
	_cl_kernel_stress_xz_PmlX_IC   	= clCreateKernel(_cl_program,"stress_xz_PmlX_IC"   	,NULL);
	_cl_kernel_stress_xz_PmlY_IC   	= clCreateKernel(_cl_program,"stress_xz_PmlY_IC"   	,NULL);
	_cl_kernel_stress_yz_PmlX_IC   	= clCreateKernel(_cl_program,"stress_yz_PmlX_IC"   	,NULL);
	_cl_kernel_stress_yz_PmlY_IC   	= clCreateKernel(_cl_program,"stress_yz_PmlY_IC"   	,NULL);
	_cl_kernel_stress_norm_xy_II   	= clCreateKernel(_cl_program,"stress_norm_xy_II"   	,NULL);
	_cl_kernel_stress_xz_yz_IIC    	= clCreateKernel(_cl_program,"stress_xz_yz_IIC"    	,NULL);
	_cl_kernel_stress_norm_PmlX_IIC	= clCreateKernel(_cl_program,"stress_norm_PmlX_IIC"	,NULL);
	_cl_kernel_stress_norm_PmlY_II 	= clCreateKernel(_cl_program,"stress_norm_PmlY_II" 	,NULL);
	_cl_kernel_stress_norm_PmlZ_IIC	= clCreateKernel(_cl_program,"stress_norm_PmlZ_IIC"	,NULL);
	_cl_kernel_stress_xy_PmlX_IIC  	= clCreateKernel(_cl_program,"stress_xy_PmlX_IIC"  	,NULL);
	_cl_kernel_stress_xy_PmlY_IIC  	= clCreateKernel(_cl_program,"stress_xy_PmlY_IIC"  	,NULL);
	_cl_kernel_stress_xy_PmlZ_II   	= clCreateKernel(_cl_program,"stress_xy_PmlZ_II"   	,NULL);
	_cl_kernel_stress_xz_PmlX_IIC  	= clCreateKernel(_cl_program,"stress_xz_PmlX_IIC"  	,NULL);
	_cl_kernel_stress_xz_PmlY_IIC  	= clCreateKernel(_cl_program,"stress_xz_PmlY_IIC"  	,NULL);
	_cl_kernel_stress_xz_PmlZ_IIC  	= clCreateKernel(_cl_program,"stress_xz_PmlZ_IIC"  	,NULL);
	_cl_kernel_stress_yz_PmlX_IIC  	= clCreateKernel(_cl_program,"stress_yz_PmlX_IIC"  	,NULL);
	_cl_kernel_stress_yz_PmlY_IIC  	= clCreateKernel(_cl_program,"stress_yz_PmlY_IIC"  	,NULL);
	_cl_kernel_stress_yz_PmlZ_IIC  	= clCreateKernel(_cl_program,"stress_yz_PmlZ_IIC"  	,NULL);

	if(_cl_kernel_velocity_inner_IC	== NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_velocity_inner_IC!\n");
	if(_cl_kernel_velocity_inner_IIC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_velocity_inner_IIC!\n");
	if(_cl_kernel_vel_PmlX_IC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_PmlX_IC!\n");
	if(_cl_kernel_vel_PmlY_IC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_PmlY_IC!\n");
	if(_cl_kernel_vel_PmlX_IIC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_PmlX_IIC!\n");
	if(_cl_kernel_vel_PmlY_IIC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_PmlY_IIC!\n");
	if(_cl_kernel_vel_PmlZ_IIC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_vel_PmlZ_IIC!\n");

	if(_cl_kernel_stress_norm_xy_IC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_norm_xy_IC!\n");
	if(_cl_kernel_stress_xz_yz_IC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_xz_yz_IC!\n");
	if(_cl_kernel_stress_resetVars == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_resetVars!\n");
	if(_cl_kernel_stress_norm_PmlX_IC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_norm_PmlX_IC!\n");
	if(_cl_kernel_stress_norm_PmlY_IC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_norm_PmlY_IC!\n");
	if(_cl_kernel_stress_xy_PmlX_IC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_xy_PmlX_IC!\n");
	if(_cl_kernel_stress_xy_PmlY_IC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_xy_PmlY_IC!\n");
	if(_cl_kernel_stress_xz_PmlX_IC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_xz_PmlX_IC!\n");
	if(_cl_kernel_stress_xz_PmlY_IC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_xz_PmlY_IC!\n");
	if(_cl_kernel_stress_yz_PmlX_IC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_yz_PmlX_IC!\n");
	if(_cl_kernel_stress_norm_xy_II == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_norm_xy_II!\n");
	if(_cl_kernel_stress_xz_yz_IIC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_xz_yz_IIC!\n");
	if(_cl_kernel_stress_norm_PmlX_IIC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_norm_PmlX_IIC!\n");
	if(_cl_kernel_stress_norm_PmlY_II == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_norm_PmlY_II!\n");
	if(_cl_kernel_stress_norm_PmlZ_IIC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_norm_PmlZ_IIC!\n");
	if(_cl_kernel_stress_xy_PmlX_IIC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_xy_PmlX_IIC!\n");
	if(_cl_kernel_stress_xy_PmlY_IIC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_xy_PmlY_IIC!\n");
	if(_cl_kernel_stress_xy_PmlZ_II == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_xy_PmlZ_II!\n");
	if(_cl_kernel_stress_xz_PmlX_IIC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_xz_PmlX_IIC!\n");
	if(_cl_kernel_stress_xz_PmlY_IIC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_xz_PmlY_IIC!\n");
	if(_cl_kernel_stress_xz_PmlZ_IIC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_xz_PmlZ_IIC!\n");
	if(_cl_kernel_stress_yz_PmlX_IIC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_yz_PmlX_IIC!\n");
	if(_cl_kernel_stress_yz_PmlY_IIC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_yz_PmlY_IIC!\n");
	if(_cl_kernel_stress_yz_PmlZ_IIC == NULL)
		fprintf(stderr, "Failed to create kernel _cl_kernel_stress_yz_PmlZ_IIC!\n");

	printf("[OpenCL] device initialization success!\n");
}

//===========================================================================
/*
    Called from disfd.f90 as one time data transfer to GPU before
    the time series loop starts
*/    

void one_time_data_vel (int* index_xyz_source, int ixsX, int ixsY, int ixsZ,
                            float* famp, int fampX, int fampY,
                             float* ruptm, int ruptmX, 
                             float* riset, int risetX,
                             float* sparam2, int sparam2X ) 
{
    cl_int errNum;

    printf("ixsX: %d \n", ixsX);
    printf("ixsY: %d \n", ixsY);
    printf("ixsZ: %d \n", ixsZ);
    printf("fampX: %d \n", fampX);
    printf("fampY: %d \n", fampY);
    printf("ruptmX: %d \n", ruptmX);
    printf("risetX: %d \n", risetX);
    printf("sparam2X: %d \n", sparam2X);

    //cudaRes = cudaMalloc((int **)&nd1_velD, sizeof(int) * 18);
    //CHECK_ERROR(cudaRes, "Allocate Device Memory1, nd1_vel");
    index_xyz_sourceD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(int) * (ixsX) * (ixsY) * (ixsZ), NULL, NULL);
    fampD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (fampX) * (fampY), NULL, NULL);
    ruptmD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (ruptmX), NULL, NULL);
    risetD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (risetX), NULL, NULL);
    sparam2D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (sparam2X), NULL, NULL);
	CHECK_NULL_ERROR(index_xyz_sourceD, "index_xyz_sourceD");
	CHECK_NULL_ERROR(fampD, "fampD");
	CHECK_NULL_ERROR(ruptmD, "ruptmD");
	CHECK_NULL_ERROR(risetD, "risetD");
	CHECK_NULL_ERROR(sparam2D, "sparam2D");

    errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], index_xyz_sourceD, CL_TRUE, 0, sizeof(int) * (ixsX) * (ixsY) *(ixsZ), index_xyz_source, 0, NULL, NULL);
    CHECK_ERROR(errNum, "InputDataCopyHostToDevice, index_xyz_source");
    errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], fampD, CL_TRUE, 0, sizeof(float) * (fampX) * (fampY), famp, 0, NULL, NULL);
    CHECK_ERROR(errNum, "InputDataCopyHostToDevice, fampx");
    errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], ruptmD, CL_TRUE, 0, sizeof(float) * (ruptmX), ruptm, 0, NULL, NULL);
    CHECK_ERROR(errNum, "InputDataCopyHostToDevice, ruptm");
    errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], risetD, CL_TRUE, 0, sizeof(float) * (risetX), riset, 0, NULL, NULL);
    CHECK_ERROR(errNum, "InputDataCopyHostToDevice, riset");
    errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], sparam2D, CL_TRUE, 0, sizeof(float) * (sparam2X), sparam2, 0, NULL, NULL);
    CHECK_ERROR(errNum, "InputDataCopyHostToDevice, sparam2");
}

void free_device_memC_opencl(int *lbx, int *lby)
{
	// timing information
	printf("[OpenCL] id = %d, vel, H2D =, %.3f, D2H =, %.3f, comp =, %.3f\n", procID, totalTimeH2DV, totalTimeD2HV, totalTimeCompV);
	printf("[OpenCL] id = %d, str, H2D =, %.3f, D2H =, %.3f, comp =, %.3f\n", procID, totalTimeH2DS, totalTimeD2HS, totalTimeCompS);
	//    printf("[Opencl] vel_once h2d = %.3f\n", onceTime);
	//    printf("[Opencl] str_once h2d = %.3f\n", onceTime2);
	Print(&kernelTimerStress[0], "Stress Kernel 0 Time");
	Print(&kernelTimerVelocity[0], "Velocity Kernel 0 Time");
	Print(&h2dTimerStress[0], "Stress H2D 0 Time");
	Print(&h2dTimerVelocity[0], "Velocity H2D 0 Time");
	Print(&d2hTimerStress[0], "Stress D2H 0 Time");
	Print(&d2hTimerVelocity[0], "Velocity D2H 0 Time");

	Print(&kernelTimerStress[1], "Stress Kernel 1 Time");
	Print(&kernelTimerVelocity[1], "Velocity Kernel 1 Time");
	Print(&h2dTimerStress[1], "Stress H2D 1 Time");
	Print(&h2dTimerVelocity[1], "Velocity H2D 1 Time");
	Print(&d2hTimerStress[1], "Stress D2H 1 Time");
	Print(&d2hTimerVelocity[1], "Velocity D2H 1 Time");
#ifdef DISFD_PAPI
	papi_print_all_events();
	papi_stop_all_events();
#endif
#ifdef DISFD_GPU_MARSHALING
clReleaseMemObject(sdx51D);
clReleaseMemObject(sdx52D);
clReleaseMemObject(sdx41D);
clReleaseMemObject(sdx42D);
clReleaseMemObject(sdy51D);
clReleaseMemObject(sdy52D);
clReleaseMemObject(sdy41D);
clReleaseMemObject(sdy42D);
clReleaseMemObject(rcx51D);
clReleaseMemObject(rcx52D);
clReleaseMemObject(rcx41D);
clReleaseMemObject(rcx42D);
clReleaseMemObject(rcy51D);
clReleaseMemObject(rcy52D);
clReleaseMemObject(rcy41D);
clReleaseMemObject(rcy42D);

clReleaseMemObject(sdx1D);
clReleaseMemObject(sdy1D);
clReleaseMemObject(sdx2D);
clReleaseMemObject(sdy2D);
clReleaseMemObject(rcx1D);
clReleaseMemObject(rcy1D);
clReleaseMemObject(rcx2D);
clReleaseMemObject(rcy2D);

clReleaseMemObject( cixD);
clReleaseMemObject( ciyD);
clReleaseMemObject( chxD);
clReleaseMemObject( chyD);

clReleaseMemObject(index_xyz_sourceD);
clReleaseMemObject(fampD);
clReleaseMemObject(sparam2D);
clReleaseMemObject(risetD);
clReleaseMemObject(ruptmD);
if(sutmArrD) clReleaseMemObject(sutmArrD);
//cl_mem sutmArrD;
#endif

	clReleaseMemObject(nd1_velD);
	clReleaseMemObject(nd1_txyD);
	clReleaseMemObject(nd1_txzD);
	clReleaseMemObject(nd1_tyyD);
	clReleaseMemObject(nd1_tyzD);
	clReleaseMemObject(rhoD);
	clReleaseMemObject(drvh1D);
	clReleaseMemObject(drti1D);
	clReleaseMemObject(drth1D);
	clReleaseMemObject(idmat1D);
	clReleaseMemObject(dxi1D);
	clReleaseMemObject(dyi1D);
	clReleaseMemObject(dzi1D);
	clReleaseMemObject(dxh1D);
	clReleaseMemObject(dyh1D);
	clReleaseMemObject(dzh1D);
	clReleaseMemObject(t1xxD);
	clReleaseMemObject(t1xyD);
	clReleaseMemObject(t1xzD);
	clReleaseMemObject(t1yyD);
	clReleaseMemObject(t1yzD);
	clReleaseMemObject(t1zzD);
	clReleaseMemObject(v1xD);    //output
	clReleaseMemObject(v1yD);
	clReleaseMemObject(v1zD);

	if (lbx[1] >= lbx[0])
	{
		clReleaseMemObject(damp1_xD);
		clReleaseMemObject(t1xx_pxD);
		clReleaseMemObject(t1xy_pxD);
		clReleaseMemObject(t1xz_pxD);
		clReleaseMemObject(t1yy_pxD);
		clReleaseMemObject(qt1xx_pxD);
		clReleaseMemObject(qt1xy_pxD);
		clReleaseMemObject(qt1xz_pxD);
		clReleaseMemObject(qt1yy_pxD);
		clReleaseMemObject(v1x_pxD);
		clReleaseMemObject(v1y_pxD);
		clReleaseMemObject(v1z_pxD);
	}

	if (lby[1] >= lby[0])
	{
		clReleaseMemObject(damp1_yD);
		clReleaseMemObject(t1xx_pyD);
		clReleaseMemObject(t1xy_pyD);
		clReleaseMemObject(t1yy_pyD);
		clReleaseMemObject(t1yz_pyD);
		clReleaseMemObject(qt1xx_pyD);
		clReleaseMemObject(qt1xy_pyD);
		clReleaseMemObject(qt1yy_pyD);
		clReleaseMemObject(qt1yz_pyD);
		clReleaseMemObject(v1x_pyD);
		clReleaseMemObject(v1y_pyD);
		clReleaseMemObject(v1z_pyD);
	}

	clReleaseMemObject(qt1xxD);
	clReleaseMemObject(qt1xyD);
	clReleaseMemObject(qt1xzD);
	clReleaseMemObject(qt1yyD);
	clReleaseMemObject(qt1yzD);
	clReleaseMemObject(qt1zzD);

	clReleaseMemObject(clamdaD);
	clReleaseMemObject(cmuD);
	clReleaseMemObject(epdtD);
	clReleaseMemObject(qwpD);
	clReleaseMemObject(qwsD);
	clReleaseMemObject(qwt1D);
	clReleaseMemObject(qwt2D);
	//-------------------------------------
	clReleaseMemObject(nd2_velD);
	clReleaseMemObject(nd2_txyD);
	clReleaseMemObject(nd2_txzD);
	clReleaseMemObject(nd2_tyyD);
	clReleaseMemObject(nd2_tyzD);

	clReleaseMemObject(drvh2D);
	clReleaseMemObject(drti2D);
	clReleaseMemObject(drth2D);
	clReleaseMemObject(idmat2D);
	clReleaseMemObject(damp2_zD);
	clReleaseMemObject(dxi2D);
	clReleaseMemObject(dyi2D);
	clReleaseMemObject(dzi2D);
	clReleaseMemObject(dxh2D);
	clReleaseMemObject(dyh2D);
	clReleaseMemObject(dzh2D);
	clReleaseMemObject(t2xxD);
	clReleaseMemObject(t2xyD);
	clReleaseMemObject(t2xzD);
	clReleaseMemObject(t2yyD);
	clReleaseMemObject(t2yzD);
	clReleaseMemObject(t2zzD);

	clReleaseMemObject(qt2xxD);
	clReleaseMemObject(qt2xyD);
	clReleaseMemObject(qt2xzD);
	clReleaseMemObject(qt2yyD);
	clReleaseMemObject(qt2yzD);
	clReleaseMemObject(qt2zzD);

	if (lbx[1] >= lbx[0])
	{
		clReleaseMemObject(damp2_xD);

		clReleaseMemObject(t2xx_pxD);
		clReleaseMemObject(t2xy_pxD);
		clReleaseMemObject(t2xz_pxD);
		clReleaseMemObject(t2yy_pxD);
		clReleaseMemObject(qt2xx_pxD);
		clReleaseMemObject(qt2xy_pxD);
		clReleaseMemObject(qt2xz_pxD);
		clReleaseMemObject(qt2yy_pxD);

		clReleaseMemObject(v2x_pxD);
		clReleaseMemObject(v2y_pxD);
		clReleaseMemObject(v2z_pxD);
	}

	if (lby[1] >= lby[0])
	{
		clReleaseMemObject(damp2_yD);

		clReleaseMemObject(t2xx_pyD);
		clReleaseMemObject(t2xy_pyD);
		clReleaseMemObject(t2yy_pyD);
		clReleaseMemObject(t2yz_pyD);

		clReleaseMemObject(qt2xx_pyD);
		clReleaseMemObject(qt2xy_pyD);
		clReleaseMemObject(qt2yy_pyD);
		clReleaseMemObject(qt2yz_pyD);

		clReleaseMemObject(v2x_pyD);
		clReleaseMemObject(v2y_pyD);
		clReleaseMemObject(v2z_pyD);
	}

	clReleaseMemObject(t2xx_pzD);
	clReleaseMemObject(t2xz_pzD);
	clReleaseMemObject(t2yz_pzD);
	clReleaseMemObject(t2zz_pzD);

	clReleaseMemObject(qt2xx_pzD);
	clReleaseMemObject(qt2xz_pzD);
	clReleaseMemObject(qt2yz_pzD);
	clReleaseMemObject(qt2zz_pzD);

	clReleaseMemObject(v2xD);		//output
	clReleaseMemObject(v2yD);
	clReleaseMemObject(v2zD);

	clReleaseMemObject(v2x_pzD);
	clReleaseMemObject(v2y_pzD);
	clReleaseMemObject(v2z_pzD);

	//printf("[OpenCL] memory space is freed.\n");
	return;
}

// opencl release
void release_cl(int *deviceID) 
{
	int i;
	// release the MPI-ACC related marshaling kernels
#ifdef DISFD_GPU_MARSHALING
	clReleaseKernel(_cl_kernel_vel_sdx51);
	clReleaseKernel(_cl_kernel_vel_sdx52);
	clReleaseKernel(_cl_kernel_vel_sdx41);
	clReleaseKernel(_cl_kernel_vel_sdx42);
	clReleaseKernel(_cl_kernel_vel_sdy51);
	clReleaseKernel(_cl_kernel_vel_sdy52);
	clReleaseKernel(_cl_kernel_vel_sdy41);
	clReleaseKernel(_cl_kernel_vel_sdy42);
	clReleaseKernel(_cl_kernel_vel_rcx51);
	clReleaseKernel(_cl_kernel_vel_rcx52);
	clReleaseKernel(_cl_kernel_vel_rcx41);
	clReleaseKernel(_cl_kernel_vel_rcx42);
	clReleaseKernel(_cl_kernel_vel_rcy51);
	clReleaseKernel(_cl_kernel_vel_rcy52);
	clReleaseKernel(_cl_kernel_vel_rcy41);
	clReleaseKernel(_cl_kernel_vel_rcy42);
	clReleaseKernel(_cl_kernel_vel_sdx1);
	clReleaseKernel(_cl_kernel_vel_sdy1);
	clReleaseKernel(_cl_kernel_vel_sdx2);
	clReleaseKernel(_cl_kernel_vel_sdy2);
	clReleaseKernel(_cl_kernel_vel_rcx1);
	clReleaseKernel(_cl_kernel_vel_rcy1);
	clReleaseKernel(_cl_kernel_vel_rcx2);
	clReleaseKernel(_cl_kernel_vel_rcy2);
	clReleaseKernel(_cl_kernel_vel1_interpl_3vbtm);
	clReleaseKernel(_cl_kernel_vel3_interpl_3vbtm);
	clReleaseKernel(_cl_kernel_vel4_interpl_3vbtm);
	clReleaseKernel(_cl_kernel_vel5_interpl_3vbtm);
	clReleaseKernel(_cl_kernel_vel6_interpl_3vbtm);
	clReleaseKernel(_cl_kernel_vel7_interpl_3vbtm);
	clReleaseKernel(_cl_kernel_vel8_interpl_3vbtm);
	clReleaseKernel(_cl_kernel_vel9_interpl_3vbtm);
	clReleaseKernel(_cl_kernel_vel11_interpl_3vbtm);
	clReleaseKernel(_cl_kernel_vel13_interpl_3vbtm);
	clReleaseKernel(_cl_kernel_vel14_interpl_3vbtm);
	clReleaseKernel(_cl_kernel_vel15_interpl_3vbtm);
	clReleaseKernel(_cl_kernel_vel1_vxy_image_layer);
	clReleaseKernel(_cl_kernel_vel2_vxy_image_layer);
	clReleaseKernel(_cl_kernel_vel3_vxy_image_layer);
	clReleaseKernel(_cl_kernel_vel4_vxy_image_layer);
//	clReleaseKernel(_cl_kernel_vel_vxy_image_layer1);
	clReleaseKernel(_cl_kernel_vel_vxy_image_layer_sdx);
	clReleaseKernel(_cl_kernel_vel_vxy_image_layer_sdy);
	clReleaseKernel(_cl_kernel_vel_vxy_image_layer_rcx);
	clReleaseKernel(_cl_kernel_vel_vxy_image_layer_rcy);
	clReleaseKernel(_cl_kernel_vel_add_dcs);
	clReleaseKernel(_cl_kernel_stress_sdx41);
	clReleaseKernel(_cl_kernel_stress_sdx42);
	clReleaseKernel(_cl_kernel_stress_sdx51);
	clReleaseKernel(_cl_kernel_stress_sdx52);
	clReleaseKernel(_cl_kernel_stress_sdy41);
	clReleaseKernel(_cl_kernel_stress_sdy42);
	clReleaseKernel(_cl_kernel_stress_sdy51);
	clReleaseKernel(_cl_kernel_stress_sdy52);
	clReleaseKernel(_cl_kernel_stress_rcx41);
	clReleaseKernel(_cl_kernel_stress_rcx42);
	clReleaseKernel(_cl_kernel_stress_rcx51);
	clReleaseKernel(_cl_kernel_stress_rcx52);
	clReleaseKernel(_cl_kernel_stress_rcy41);
	clReleaseKernel(_cl_kernel_stress_rcy42);
	clReleaseKernel(_cl_kernel_stress_rcy51);
	clReleaseKernel(_cl_kernel_stress_rcy52);
	clReleaseKernel(_cl_kernel_stress_interp_stress);
	clReleaseKernel(_cl_kernel_stress_interp1);
	clReleaseKernel(_cl_kernel_stress_interp2);
	clReleaseKernel(_cl_kernel_stress_interp3);
#endif
	// release the opencl context
	clReleaseKernel(_cl_kernel_velocity_inner_IC   );
	clReleaseKernel(_cl_kernel_velocity_inner_IIC  );
	clReleaseKernel(_cl_kernel_vel_PmlX_IC         );
	clReleaseKernel(_cl_kernel_vel_PmlY_IC         );
	clReleaseKernel(_cl_kernel_vel_PmlX_IIC        );
	clReleaseKernel(_cl_kernel_vel_PmlY_IIC        );
	clReleaseKernel(_cl_kernel_vel_PmlZ_IIC        );
	clReleaseKernel(_cl_kernel_stress_norm_xy_IC   );
	clReleaseKernel(_cl_kernel_stress_xz_yz_IC     );
	clReleaseKernel(_cl_kernel_stress_resetVars    );
	clReleaseKernel(_cl_kernel_stress_norm_PmlX_IC );
	clReleaseKernel(_cl_kernel_stress_norm_PmlY_IC );
	clReleaseKernel(_cl_kernel_stress_xy_PmlX_IC   );
	clReleaseKernel(_cl_kernel_stress_xy_PmlY_IC   );
	clReleaseKernel(_cl_kernel_stress_xz_PmlX_IC   );
	clReleaseKernel(_cl_kernel_stress_xz_PmlY_IC   );
	clReleaseKernel(_cl_kernel_stress_yz_PmlX_IC   );
	clReleaseKernel(_cl_kernel_stress_yz_PmlY_IC   );
	clReleaseKernel(_cl_kernel_stress_norm_xy_II   );
	clReleaseKernel(_cl_kernel_stress_xz_yz_IIC    );
	clReleaseKernel(_cl_kernel_stress_norm_PmlX_IIC);
	clReleaseKernel(_cl_kernel_stress_norm_PmlY_II );
	clReleaseKernel(_cl_kernel_stress_norm_PmlZ_IIC);
	clReleaseKernel(_cl_kernel_stress_xy_PmlX_IIC  );
	clReleaseKernel(_cl_kernel_stress_xy_PmlY_IIC  );
	clReleaseKernel(_cl_kernel_stress_xy_PmlZ_II   );
	clReleaseKernel(_cl_kernel_stress_xz_PmlX_IIC  );
	clReleaseKernel(_cl_kernel_stress_xz_PmlY_IIC  );
	clReleaseKernel(_cl_kernel_stress_xz_PmlZ_IIC  );
	clReleaseKernel(_cl_kernel_stress_yz_PmlX_IIC  );
	clReleaseKernel(_cl_kernel_stress_yz_PmlY_IIC  );
	clReleaseKernel(_cl_kernel_stress_yz_PmlZ_IIC  );

	clReleaseProgram(_cl_program);
	//clReleaseCommandQueue(_cl_commandQueue);
	for(i = 0; i < NUM_COMMAND_QUEUES; i++)
	{
		clReleaseCommandQueue(_cl_commandQueues[i]);
	}
	clReleaseContext(_cl_context);
	free(_cl_devices);
	//printf("[OpenCL] context all released!\n");
}

// called from disfd.f90 only once
void allocate_gpu_memC_opencl(int   *lbx,
		int   *lby,
		int   *nmat,		//dimension #, int
		int	  *mw1_pml1,	//int
		int	  *mw2_pml1,	//int
		int	  *nxtop,		//int
		int	  *nytop,		//int
		int   *nztop,
		int	  *mw1_pml,		//int
		int   *mw2_pml,		//int
		int	  *nxbtm,		//int
		int	  *nybtm,		//int
		int	  *nzbtm,
		int   *nzbm1,
		int   *nll)
{
	//printf("[OpenCL] allocation..........");
	int nv2, nti, nth;
	// debug -----------
	// printf("lbx[1] = %d, lbx[0] = %d\n", lbx[1], lbx[0]);
	// printf("lby[1] = %d, lby[0] = %d\n", lby[1], lby[0]);
	// printf("nmat = %d\n", *nmat);
	// printf("mw1_pml1 = %d, mw2_pml1 = %d\n", *mw1_pml1, *mw2_pml1);
	// printf("mw1_pml = %d, mw2_pml = %d\n", *mw1_pml, *mw2_pml);
	// printf("nxtop = %d, nytop = %d, nztop = %d\n", *nxtop, *nytop, *nztop);
	// printf("nxbtm = %d, nybtm = %d, nzbtm = %d\n", *nxbtm, *nybtm, *nzbtm);
	// printf("nzbm1 = %d, nll = %d\n", *nzbm1, *nll);

	// timing ---------
	totalTimeH2DV = 0.0f;
	totalTimeD2HV = 0.0f;
	totalTimeH2DS = 0.0f;
	totalTimeD2HS = 0.0f;
	totalTimeCompV = 0.0f;
	totalTimeCompS = 0.0f;

	//for inner_I
	nd1_velD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(int) * 18, NULL, NULL);
	nd1_txyD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(int) * 18, NULL, NULL);
	nd1_txzD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(int) * 18, NULL, NULL);
	nd1_tyyD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(int) * 18, NULL, NULL);
	nd1_tyzD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(int) * 18, NULL, NULL);
	CHECK_NULL_ERROR(nd1_velD, "nd1_velD");
	CHECK_NULL_ERROR(nd1_txyD, "nd1_txyD");
	CHECK_NULL_ERROR(nd1_txzD, "nd1_txzD");
	CHECK_NULL_ERROR(nd1_tyyD, "nd1_tyyD");
	CHECK_NULL_ERROR(nd1_tyzD, "nd1_tyzD");

	rhoD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * (*nmat), NULL, NULL);
	CHECK_NULL_ERROR(rhoD, "rhoD");

	drvh1D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * (*mw1_pml1) * 2, NULL, NULL);
	drti1D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * (*mw1_pml1) * 2, NULL, NULL);
	drth1D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * (*mw1_pml1) * 2, NULL, NULL);
	CHECK_NULL_ERROR(drvh1D, "drvh1D");
	CHECK_NULL_ERROR(drti1D, "drti1D");
	CHECK_NULL_ERROR(drth1D, "drth1D");

	if (lbx[1] >= lbx[0])
	{
		damp1_xD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * (*nztop + 1) * (*nytop) * (lbx[1] - lbx[0] + 1), NULL, NULL);
		CHECK_NULL_ERROR(damp1_xD, "damp1_xD");
	}

	if (lby[1] >= lby[0])
	{
		damp1_yD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * (*nztop + 1) * (*nxtop) * (lby[1] - lby[0] + 1), NULL, NULL);
		CHECK_NULL_ERROR(damp1_yD, "damp1_yD");
	}

	idmat1D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(int) * (*nztop + 2) * (*nxtop + 1) * (*nytop + 1), NULL, NULL);
	CHECK_NULL_ERROR(idmat1D, "idmat1D");
	dxi1D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * 4 * (*nxtop), NULL, NULL);
	dyi1D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * 4 * (*nytop), NULL, NULL);
	dzi1D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * 4 * (*nztop + 1), NULL, NULL);
	dxh1D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * 4 * (*nxtop), NULL, NULL);
	dyh1D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * 4 * (*nytop), NULL, NULL);
	dzh1D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * 4 * (*nztop + 1), NULL, NULL);
	CHECK_NULL_ERROR(dxi1D, "dxi1D");
	CHECK_NULL_ERROR(dyi1D, "dyi1D");
	CHECK_NULL_ERROR(dzi1D, "dzi1D");
	CHECK_NULL_ERROR(dxh1D, "dxh1D");
	CHECK_NULL_ERROR(dyh1D, "dyh1D");
	CHECK_NULL_ERROR(dzh1D, "dzh1D");

	t1xxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * (*nxtop + 3) * (*nytop), NULL, NULL);
	t1xyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * (*nxtop + 3) * (*nytop + 3), NULL, NULL);
	t1xzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop + 1) * (*nxtop + 3) * (*nytop), NULL, NULL);
	t1yyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * (*nxtop) * (*nytop + 3), NULL, NULL);
	t1yzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop + 1) * (*nxtop) * (*nytop + 3), NULL, NULL);
	t1zzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * (*nxtop) * (*nytop), NULL, NULL);
	CHECK_NULL_ERROR(t1xxD, "t1xxD");
	CHECK_NULL_ERROR(t1xyD, "t1xyD");
	CHECK_NULL_ERROR(t1xzD, "t1xzD");
	CHECK_NULL_ERROR(t1yyD, "t1yyD");
	CHECK_NULL_ERROR(t1yzD, "t1yzD");
	CHECK_NULL_ERROR(t1zzD, "t1zzD");

	if (lbx[1] >= lbx[0])
	{
		nti = (lbx[1] - lbx[0] + 1) * (*mw1_pml) + lbx[1];
		nth = (lbx[1] - lbx[0] + 1) * (*mw1_pml) + 1 - lbx[0];

		t1xx_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * (nti) * (*nytop), NULL, NULL);
		t1xy_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * nth * (*nytop), NULL, NULL);
		t1xz_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop+1) * nth * (*nytop), NULL, NULL);
		t1yy_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * nti * (*nytop), NULL, NULL);
		CHECK_NULL_ERROR(t1xx_pxD, "t1xx_pxD");
		CHECK_NULL_ERROR(t1xy_pxD, "t1xy_pxD");
		CHECK_NULL_ERROR(t1xz_pxD, "t1xz_pxD");
		CHECK_NULL_ERROR(t1yy_pxD, "t1yy_pxD");

		qt1xx_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * (nti) * (*nytop), NULL, NULL);
		qt1xy_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * nth * (*nytop), NULL, NULL);
		qt1xz_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop+1) * nth * (*nytop), NULL, NULL);
		qt1yy_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * nti * (*nytop), NULL, NULL);
		CHECK_NULL_ERROR(qt1xx_pxD, "qt1xx_pxD");
		CHECK_NULL_ERROR(qt1xy_pxD, "qt1xy_pxD");
		CHECK_NULL_ERROR(qt1xz_pxD, "qt1xz_pxD");
		CHECK_NULL_ERROR(qt1yy_pxD, "qt1yy_pxD");
	}

	if (lby[1] >= lby[0])
	{
		nti = (lby[1] - lby[0] + 1) * (*mw1_pml) + lby[1];
		nth = (lby[1] - lby[0] + 1) * (*mw1_pml) + 1 - lby[0];

		t1xx_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * (*nxtop) * nti, NULL, NULL);
		t1xy_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * (*nxtop) * nth, NULL, NULL);
		t1yy_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * (*nxtop) * nti, NULL, NULL);
		t1yz_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop+1) * (*nxtop) * nth, NULL, NULL);
		CHECK_NULL_ERROR(t1xx_pyD, "t1xx_pyD");
		CHECK_NULL_ERROR(t1xy_pyD, "t1xy_pyD");
		CHECK_NULL_ERROR(t1yy_pyD, "t1yy_pyD");
		CHECK_NULL_ERROR(t1yz_pyD, "t1yz_pyD");

		qt1xx_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * (*nxtop) * nti, NULL, NULL);
		qt1xy_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * (*nxtop) * nth, NULL, NULL);
		qt1yy_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * (*nxtop) * nti, NULL, NULL);
		qt1yz_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop+1) * (*nxtop) * nth, NULL, NULL);
		CHECK_NULL_ERROR(qt1xx_pyD, "qt1xx_pyD");
		CHECK_NULL_ERROR(qt1xy_pyD, "qt1xy_pyD");
		CHECK_NULL_ERROR(qt1yy_pyD, "qt1yy_pyD");
		CHECK_NULL_ERROR(qt1yz_pyD, "qt1yz_pyD");
	}

	qt1xxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * (*nxtop) * (*nytop), NULL, NULL);
	qt1xyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * (*nxtop) * (*nytop), NULL, NULL);
	qt1xzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop+1) * (*nxtop) * (*nytop), NULL, NULL);
	qt1yyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * (*nxtop) * (*nytop), NULL, NULL);
	qt1yzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop+1) * (*nxtop) * (*nytop), NULL, NULL);
	qt1zzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * (*nxtop) * (*nytop), NULL, NULL);
	CHECK_NULL_ERROR(qt1xxD, "qt1xxD");
	CHECK_NULL_ERROR(qt1xyD, "qt1xyD");
	CHECK_NULL_ERROR(qt1xzD, "qt1xzD");
	CHECK_NULL_ERROR(qt1yyD, "qt1yyD");
	CHECK_NULL_ERROR(qt1yzD, "qt1yzD");
	CHECK_NULL_ERROR(qt1zzD, "qt1zzD");

	clamdaD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * (*nmat), NULL, NULL);
	cmuD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * (*nmat), NULL, NULL);
	epdtD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * (*nll), NULL, NULL);
	qwpD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * (*nmat), NULL, NULL);
	qwsD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * (*nmat), NULL, NULL);
	qwt1D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * (*nll), NULL, NULL);
	qwt2D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * (*nll), NULL, NULL);
	CHECK_NULL_ERROR(clamdaD, "clamdaD");
	CHECK_NULL_ERROR(cmuD, "cmuD");
	CHECK_NULL_ERROR(epdtD, "epdtD");
	CHECK_NULL_ERROR(qwpD, "qwpD");
	CHECK_NULL_ERROR(qwsD, "qwsD");
	CHECK_NULL_ERROR(qwt1D, "qwt1D");
	CHECK_NULL_ERROR(qwt2D, "qwt2D");

	v1xD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), NULL, NULL);
	v1yD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), NULL, NULL);
	v1zD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), NULL, NULL);
	CHECK_NULL_ERROR(v1xD, "v1xD");
	CHECK_NULL_ERROR(v1yD, "v1yD");
	CHECK_NULL_ERROR(v1zD, "v1zD");

	if (lbx[1] >= lbx[0])
	{
		nv2 = (lbx[1] - lbx[0] + 1) * (*mw1_pml);

		v1x_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * nv2 * (*nytop), NULL, NULL);
		v1y_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * nv2 * (*nytop), NULL, NULL);
		v1z_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * nv2 * (*nytop), NULL, NULL);
		CHECK_NULL_ERROR(v1x_pxD, "v1x_pxD");
		CHECK_NULL_ERROR(v1y_pxD, "v1y_pxD");
		CHECK_NULL_ERROR(v1z_pxD, "v1z_pxD");
	}

	if (lby[1] >= lby[0])
	{
		nv2 = (lby[1] - lby[0] + 1) * (*mw1_pml);
		v1x_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * (*nxtop) * nv2, NULL, NULL);
		v1y_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * (*nxtop) * nv2, NULL, NULL);
		v1z_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nztop) * (*nxtop) * nv2, NULL, NULL);
		CHECK_NULL_ERROR(v1x_pyD, "v1x_pyD");
		CHECK_NULL_ERROR(v1y_pyD, "v1y_pyD");
		CHECK_NULL_ERROR(v1z_pyD, "v1z_pyD");
	}

	//for inner_II-----------------------------------------------------------------------------------------
	nd2_velD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(int) * 18, NULL, NULL);
	nd2_txyD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(int) * 18, NULL, NULL);
	nd2_txzD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(int) * 18, NULL, NULL); 
	nd2_tyyD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(int) * 18, NULL, NULL);
	nd2_tyzD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(int) * 18, NULL, NULL);
	CHECK_NULL_ERROR(nd2_velD, "nd2_velD");
	CHECK_NULL_ERROR(nd2_txyD, "nd2_txyD");
	CHECK_NULL_ERROR(nd2_txzD, "nd2_txzD");
	CHECK_NULL_ERROR(nd2_tyyD, "nd2_tyyD");
	CHECK_NULL_ERROR(nd2_tyzD, "nd2_tyzD");

	drvh2D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * (*mw2_pml1) * 2, NULL, NULL);
	drti2D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * (*mw2_pml1) * 2, NULL, NULL);
	drth2D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * (*mw2_pml1) * 2, NULL, NULL);
	CHECK_NULL_ERROR(drvh2D, "drvh2D");
	CHECK_NULL_ERROR(drti2D, "drti2D");
	CHECK_NULL_ERROR(drth2D, "drth2D");

	idmat2D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(int) * (*nzbtm + 1) * (*nxbtm + 1) * (*nybtm + 1), NULL, NULL);
	CHECK_NULL_ERROR(idmat2D, "idmat2D");

	if (lbx[1] >= lbx[0])
	{
		damp2_xD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * (*nzbtm) * (*nybtm) * (lbx[1] - lbx[0] + 1), NULL, NULL);
		CHECK_NULL_ERROR(damp2_xD, "damp2_xD");
	}

	if (lby[1] >= lby[0])
	{
		damp2_yD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * (*nzbtm) * (*nxbtm) * (lby[1] - lby[0] + 1), NULL, NULL);
		CHECK_NULL_ERROR(damp2_yD, "damp2_yD");
	}
	damp2_zD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * (*nxbtm) * (*nybtm), NULL, NULL);
	CHECK_NULL_ERROR(damp2_zD, "damp2_zD");

	dxi2D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * 4 * (*nxbtm), NULL, NULL);
	dyi2D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * 4 * (*nybtm), NULL, NULL);
	dzi2D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * 4 * (*nzbtm), NULL, NULL);
	dxh2D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * 4 * (*nxbtm), NULL, NULL);
	dyh2D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * 4 * (*nybtm), NULL, NULL);
	dzh2D = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY, sizeof(float) * 4 * (*nzbtm), NULL, NULL);
	CHECK_NULL_ERROR(dxi2D, "dxi2D");
	CHECK_NULL_ERROR(dyi2D, "dyi2D");
	CHECK_NULL_ERROR(dzi2D, "dzi2D");
	CHECK_NULL_ERROR(dxh2D, "dxh2D");
	CHECK_NULL_ERROR(dyh2D, "dyh2D");
	CHECK_NULL_ERROR(dxh2D, "dzh2D");

	t2xxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * (*nxbtm + 3) * (*nybtm), NULL, NULL);
	t2xyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * (*nxbtm + 3) * (*nybtm + 3), NULL, NULL);
	t2xzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm), NULL, NULL);
	t2yyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * (*nxbtm) * (*nybtm + 3), NULL, NULL);
	t2yzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm + 1) * (*nxbtm) * (*nybtm + 3), NULL, NULL);
	t2zzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm + 1) * (*nxbtm) * (*nybtm), NULL, NULL);
	CHECK_NULL_ERROR(t2xxD, "t2xxD");
	CHECK_NULL_ERROR(t2xyD, "t2xyD");
	CHECK_NULL_ERROR(t2xzD, "t2xzD");
	CHECK_NULL_ERROR(t2yyD, "t2yyD");
	CHECK_NULL_ERROR(t2yzD, "t2yzD");
	CHECK_NULL_ERROR(t2zzD, "t2zzD");

	qt2xxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * (*nxbtm) * (*nybtm), NULL, NULL);
	qt2xyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * (*nxbtm) * (*nybtm), NULL, NULL);
	qt2xzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * (*nxbtm) * (*nybtm), NULL, NULL);
	qt2yyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * (*nxbtm) * (*nybtm), NULL, NULL);
	qt2yzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * (*nxbtm) * (*nybtm), NULL, NULL);
	qt2zzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * (*nxbtm) * (*nybtm), NULL, NULL);
	CHECK_NULL_ERROR(qt2xxD, "qt2xxD");
	CHECK_NULL_ERROR(qt2xyD, "qt2xyD");
	CHECK_NULL_ERROR(qt2xzD, "qt2xzD");
	CHECK_NULL_ERROR(qt2yyD, "qt2yyD");
	CHECK_NULL_ERROR(qt2yzD, "qt2yzD");
	CHECK_NULL_ERROR(qt2zzD, "qt2zzD");


	if (lbx[1] >= lbx[0])
	{
		nti = (lbx[1] - lbx[0] + 1) * (*mw2_pml) + lbx[1];
		nth = (lbx[1] - lbx[0] + 1) * (*mw2_pml) + 1 - lbx[0];

		t2xx_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * nti * (*nybtm), NULL, NULL);
		t2xy_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * nth * (*nybtm), NULL, NULL);
		t2xz_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * nth * (*nybtm), NULL, NULL);
		t2yy_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * nti * (*nybtm), NULL, NULL);
		CHECK_NULL_ERROR(t2xx_pxD, "t2xx_pxD");
		CHECK_NULL_ERROR(t2xy_pxD, "t2xy_pxD");
		CHECK_NULL_ERROR(t2xz_pxD, "t2xz_pxD");
		CHECK_NULL_ERROR(t2yy_pxD, "t2yy_pxD");

		qt2xx_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * nti * (*nybtm), NULL, NULL);
		qt2xy_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * nth * (*nybtm), NULL, NULL);
		qt2xz_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * nth * (*nybtm), NULL, NULL);
		qt2yy_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * nti * (*nybtm), NULL, NULL);
		CHECK_NULL_ERROR(qt2xx_pxD, "qt2xx_pxD");
		CHECK_NULL_ERROR(qt2xy_pxD, "qt2xy_pxD");
		CHECK_NULL_ERROR(qt2xz_pxD, "qt2xz_pxD");
		CHECK_NULL_ERROR(qt2yy_pxD, "qt2yy_pxD");
	}

	if (lby[1] >= lby[0])
	{
		nti = (lby[1] - lby[0] + 1) * (*mw2_pml) + lby[1];
		nth = (lby[1] - lby[0] + 1) * (*mw2_pml) + 1 - lby[0];

		t2xx_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * (*nxbtm) * nti, NULL, NULL);
		t2xy_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * (*nxbtm) * nth, NULL, NULL);
		t2yy_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * (*nxbtm) * nti, NULL, NULL);
		t2yz_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * (*nxbtm) * nth, NULL, NULL);
		CHECK_NULL_ERROR(t2xx_pyD, "t2xx_pyD");
		CHECK_NULL_ERROR(t2xy_pyD, "t2xy_pyD");
		CHECK_NULL_ERROR(t2yy_pyD, "t2yy_pyD");
		CHECK_NULL_ERROR(t2yz_pyD, "t2yz_pyD");

		qt2xx_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * (*nxbtm) * nti, NULL, NULL);
		qt2xy_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * (*nxbtm) * nth, NULL, NULL);
		qt2yy_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * (*nxbtm) * nti, NULL, NULL);
		qt2yz_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * (*nxbtm) * nth, NULL, NULL);
		CHECK_NULL_ERROR(qt2xx_pyD, "qt2xx_pyD");
		CHECK_NULL_ERROR(qt2xy_pyD, "qt2xy_pyD");
		CHECK_NULL_ERROR(qt2yy_pyD, "qt2yy_pyD");
		CHECK_NULL_ERROR(qt2yz_pyD, "qt2yz_pyD");
	}

	t2xx_pzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*mw2_pml) * (*nxbtm) * (*nybtm), NULL, NULL);
	t2xz_pzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*mw2_pml1) * (*nxbtm) * (*nybtm), NULL, NULL);
	t2yz_pzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*mw2_pml1) * (*nxbtm) * (*nybtm), NULL, NULL);
	t2zz_pzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*mw2_pml) * (*nxbtm) * (*nybtm), NULL, NULL);
	CHECK_NULL_ERROR(t2xx_pzD, "t2xx_pzD");
	CHECK_NULL_ERROR(t2xz_pzD, "t2xz_pzD");
	CHECK_NULL_ERROR(t2yz_pzD, "t2yz_pzD");
	CHECK_NULL_ERROR(t2zz_pzD, "t2zz_pzD");

	qt2xx_pzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*mw2_pml) * (*nxbtm) * (*nybtm), NULL, NULL);
	qt2xz_pzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*mw2_pml1) * (*nxbtm) * (*nybtm), NULL, NULL);
	qt2yz_pzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*mw2_pml1) * (*nxbtm) * (*nybtm), NULL, NULL);
	qt2zz_pzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*mw2_pml) * (*nxbtm) * (*nybtm), NULL, NULL);
	CHECK_NULL_ERROR(qt2xx_pzD, "qt2xx_pzD");
	CHECK_NULL_ERROR(qt2xz_pzD, "qt2xz_pzD");
	CHECK_NULL_ERROR(qt2yz_pzD, "qt2yz_pzD");
	CHECK_NULL_ERROR(qt2zz_pzD, "qt2zz_pzD");

	v2xD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3), NULL, NULL);
	v2yD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3), NULL, NULL);
	v2zD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3), NULL, NULL);
	CHECK_NULL_ERROR(v2xD, "v2xD");
	CHECK_NULL_ERROR(v2yD, "v2yD");
	CHECK_NULL_ERROR(v2zD, "v2zD");

	if (lbx[1] >= lbx[0])
	{
		nv2 = (lbx[1] - lbx[0] + 1) * (*mw2_pml);
		v2x_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * nv2 * (*nybtm), NULL, NULL);
		v2y_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * nv2 * (*nybtm), NULL, NULL);
		v2z_pxD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * nv2 * (*nybtm), NULL, NULL);
		CHECK_NULL_ERROR(v2x_pxD, "v2x_pxD");
		CHECK_NULL_ERROR(v2y_pxD, "v2y_pxD");
		CHECK_NULL_ERROR(v2z_pxD, "v2z_pxD");
	}

	if (lby[1] >= lby[0])
	{
		nv2 = (lby[1] - lby[0] + 1) * (*mw2_pml);
		v2x_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * (*nxbtm) * nv2, NULL, NULL);
		v2y_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * (*nxbtm) * nv2, NULL, NULL);
		v2z_pyD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*nzbtm) * (*nxbtm) * nv2, NULL, NULL);
		CHECK_NULL_ERROR(v2x_pyD, "v2x_pyD");
		CHECK_NULL_ERROR(v2y_pyD, "v2y_pyD");
		CHECK_NULL_ERROR(v2z_pyD, "v2z_pyD");
	}

	v2x_pzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*mw2_pml) * (*nxbtm) * (*nybtm), NULL, NULL);
	v2y_pzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*mw2_pml) * (*nxbtm) * (*nybtm), NULL, NULL);
	v2z_pzD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (*mw2_pml) * (*nxbtm) * (*nybtm), NULL, NULL);
	CHECK_NULL_ERROR(v2x_pzD, "v2x_pzD");
	CHECK_NULL_ERROR(v2y_pzD, "v2y_pzD");
	CHECK_NULL_ERROR(v2z_pzD, "v2z_pzD");

// MPI-ACC
#ifdef DISFD_GPU_MARSHALING
sdx51D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nztop) * (*nytop) * (5), NULL, NULL);
sdx52D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nzbtm) * (*nybtm) * (5), NULL, NULL);
sdx41D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nztop) * (*nytop) * (4), NULL, NULL);
sdx42D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nzbtm) * (*nybtm) * (4), NULL, NULL);

sdy51D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nztop) * (*nxtop) * (5), NULL, NULL);
sdy52D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nzbtm) * (*nxbtm) * (5), NULL, NULL);
sdy41D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nztop) * (*nxtop) * (4), NULL, NULL);
sdy42D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nzbtm) * (*nxbtm) * (4), NULL, NULL);

rcx51D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nztop) * (*nytop) * (5), NULL, NULL);
rcx52D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nzbtm) * (*nybtm) * (5), NULL, NULL);
rcx41D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nztop) * (*nytop) * (4), NULL, NULL);
rcx42D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nzbtm) * (*nybtm) * (4), NULL, NULL);

rcy51D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nztop) * (*nxtop) * (5), NULL, NULL);
rcy52D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nzbtm) * (*nxbtm) * (5), NULL, NULL);
rcy41D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nztop) * (*nxtop) * (4), NULL, NULL);
rcy42D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nzbtm) * (*nxbtm) * (4), NULL, NULL);

sdy1D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nxtop + 6), NULL, NULL);
sdy2D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nxtop + 6), NULL, NULL);
rcy1D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nxtop + 6), NULL, NULL);
rcy2D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nxtop + 6), NULL, NULL);

sdx1D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nytop + 6), NULL, NULL);
sdx2D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nytop + 6), NULL, NULL);
rcx1D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nytop + 6), NULL, NULL);
rcx2D = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE,  sizeof(float) * (*nytop + 6), NULL, NULL);

//
// ch/i x/y
cixD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY,  sizeof(float) * (*nxbtm +6+1)*8, NULL, NULL);
ciyD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY,  sizeof(float) * (*nybtm +6+1)*8, NULL, NULL);
chxD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY,  sizeof(float) * (*nxbtm +6+1)*8, NULL, NULL);
chyD = clCreateBuffer(_cl_context, CL_MEM_READ_ONLY,  sizeof(float) * (*nybtm +6+1)*8, NULL, NULL);


CHECK_NULL_ERROR(sdx51D, "sdx51D");
CHECK_NULL_ERROR(sdx52D, "sdx52D");
CHECK_NULL_ERROR(sdx41D, "sdx41D");
CHECK_NULL_ERROR(sdx42D, "sdx42D");
CHECK_NULL_ERROR(sdy51D, "sdy51D");
CHECK_NULL_ERROR(sdy52D, "sdy52D");
CHECK_NULL_ERROR(sdy41D, "sdy41D");
CHECK_NULL_ERROR(sdy42D, "sdy42D");
CHECK_NULL_ERROR(rcx51D, "rcx51D");
CHECK_NULL_ERROR(rcx52D, "rcx52D");
CHECK_NULL_ERROR(rcx41D, "rcx41D");
CHECK_NULL_ERROR(rcx42D, "rcx42D");
CHECK_NULL_ERROR(rcy51D, "rcy51D");
CHECK_NULL_ERROR(rcy52D, "rcy52D");
CHECK_NULL_ERROR(rcy41D, "rcy41D");
CHECK_NULL_ERROR(rcy42D, "rcy42D");
CHECK_NULL_ERROR(sdy1D, "sdy1D");
CHECK_NULL_ERROR(sdy2D, "sdy2D");
CHECK_NULL_ERROR(rcy1D, "rcy1D");
CHECK_NULL_ERROR(rcy2D, "rcy2D");
CHECK_NULL_ERROR(sdx1D, "sdx1D");
CHECK_NULL_ERROR(sdx2D, "sdx2D");
CHECK_NULL_ERROR(rcx1D, "rcx1D");
CHECK_NULL_ERROR(rcx2D, "rcx2D");
CHECK_NULL_ERROR(cixD, "cixD");
CHECK_NULL_ERROR(ciyD, "ciyD");
CHECK_NULL_ERROR(chxD, "chxD");
CHECK_NULL_ERROR(chyD, "chyD");
	//printf("done!\n");

	init_gpuarr_metadata (*nxtop, *nytop, *nztop, *nxbtm, *nybtm, *nzbtm);
#endif
	return;
}

// copy data h2d before iterations
void cpy_h2d_velocityInputsCOneTimecl(int   *lbx,
		int   *lby,
		int   *nd1_vel,
		float *rho,
		float *drvh1,
		float *drti1,
		float *damp1_x,
		float *damp1_y,
		int	*idmat1,
		float *dxi1,
		float *dyi1,
		float *dzi1,
		float *dxh1,
		float *dyh1,
		float *dzh1,
		float *t1xx,
		float *t1xy,
		float *t1xz,
		float *t1yy,
		float *t1yz,
		float *t1zz,
		float *v1x_px,
		float *v1y_px,
		float *v1z_px,
		float *v1x_py,
		float *v1y_py,
		float *v1z_py,
		int	*nd2_vel,
		float *drvh2,
		float *drti2,
		int	*idmat2,
		float *damp2_x,
		float *damp2_y,
		float *damp2_z,
		float *dxi2,
		float *dyi2,
		float *dzi2,
		float *dxh2,
		float *dyh2,
		float *dzh2,
		float *t2xx,
		float *t2xy,
		float *t2xz,
		float *t2yy,
		float *t2yz,
		float *t2zz,
		float *v2x_px,
		float *v2y_px,
		float *v2z_px,
		float *v2x_py,
		float *v2y_py,
		float *v2z_py,
		float *v2x_pz,
		float *v2y_pz,
		float *v2z_pz,
                                                  float* cix,
                                                  float* ciy,
                                                  float* chx,
                                                  float* chy,
		int   *nmat,		//dimension #, int
		int	*mw1_pml1,	//int
		int	*mw2_pml1,	//int
		int	*nxtop,		//int
		int	*nytop,		//int
		int   *nztop,
		int	*mw1_pml,	//int
		int   *mw2_pml,	//int
		int	*nxbtm,		//int
		int	*nybtm,		//int
		int	*nzbtm,
		int   *nzbm1)
{
	//printf("[OpenCL] initial h2d cpy for velocity ........");
	// printf("lbx[0] = %d, lbx[1] = %d\n", lbx[0], lbx[1]);
	// printf("lby[0] = %d, lby[1] = %d\n", lby[0], lby[1]);

	cl_int errNum;
	int nv2;

	//for inner_I
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], nd1_velD, CL_TRUE, 0, sizeof(int) * 18, nd1_vel, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, nd1_vel");

	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], rhoD, CL_TRUE, 0, sizeof(float) * (*nmat), rho, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, rho");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], drvh1D, CL_TRUE, 0, sizeof(float) * (*mw1_pml1) * 2, drvh1, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, drvh1");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], drti1D, CL_TRUE, 0, sizeof(float) * (*mw1_pml1) * 2, drti1, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, drti1");

	if (lbx[1] >= lbx[0])
	{
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], damp1_xD, CL_TRUE, 0, sizeof(float) * (*nztop + 1) * (*nytop) * (lbx[1] - lbx[0] + 1), damp1_x, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, damp1_x");
	}

	if (lby[1] >= lby[0])
	{
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], damp1_yD, CL_TRUE, 0, sizeof(float) * (*nztop + 1) * (*nxtop) * (lby[1] - lby[0] + 1), damp1_y, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, damp1_y");
	}

	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], idmat1D, CL_TRUE, 0, sizeof(int) * (*nztop + 2) * (*nxtop + 1) * (*nytop + 1), idmat1, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, idmat1");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], dxi1D, CL_TRUE, 0, sizeof(float) * 4 * (*nxtop), dxi1, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, dxi1");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], dyi1D, CL_TRUE, 0, sizeof(float) * 4 * (*nytop), dyi1, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, dyi1");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], dzi1D, CL_TRUE, 0, sizeof(float) * 4 * (*nztop + 1), dzi1, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, dzi1");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], dxh1D, CL_TRUE, 0, sizeof(float) * 4 * (*nxtop), dxh1, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, dxh1");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], dyh1D, CL_TRUE, 0, sizeof(float) * 4 * (*nytop), dyh1, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, dyh1");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], dzh1D, CL_TRUE, 0, sizeof(float) * 4 * (*nztop + 1), dzh1, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, dzh1");

	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1xxD, CL_TRUE, 0, sizeof(float) * (*nztop) * (*nxtop + 3) * (*nytop), t1xx, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t1xx");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1xyD, CL_TRUE, 0, sizeof(float) * (*nztop) * (*nxtop + 3) * (*nytop + 3), t1xy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t1xy");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1xzD, CL_TRUE, 0, sizeof(float) * (*nztop + 1) * (*nxtop + 3) * (*nytop), t1xz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t1xz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1yyD, CL_TRUE, 0, sizeof(float) * (*nztop) * (*nxtop) * (*nytop + 3), t1yy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t1yy");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1yzD, CL_TRUE, 0, sizeof(float) * (*nztop + 1) * (*nxtop) * (*nytop + 3), t1yz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t1yz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1zzD, CL_TRUE, 0, sizeof(float) * (*nztop) * (*nxtop) * (*nytop), t1zz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t1zz");

	if (lbx[1] >= lbx[0])
	{
		nv2 = (lbx[1] - lbx[0] + 1) * (*mw1_pml);
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], v1x_pxD, CL_TRUE, 0, sizeof(float) * (*nztop) * nv2 * (*nytop), v1x_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, v1x_px");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], v1y_pxD, CL_TRUE, 0, sizeof(float) * (*nztop) * nv2 * (*nytop), v1y_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, v1y_px");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], v1z_pxD, CL_TRUE, 0, sizeof(float) * (*nztop) * nv2 * (*nytop), v1z_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, v1z_px");
	}

	if (lby[1] >= lby[0])
	{
		nv2 = (lby[1] - lby[0] + 1) * (*mw1_pml);
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], v1x_pyD, CL_TRUE, 0, sizeof(float) * (*nztop) * (*nxtop) * nv2, v1x_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, v1x_py");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], v1y_pyD, CL_TRUE, 0, sizeof(float) * (*nztop) * (*nxtop) * nv2, v1y_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, v1y_py");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], v1z_pyD, CL_TRUE, 0, sizeof(float) * (*nztop) * (*nxtop) * nv2, v1z_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, v1z_py");
	}


	//for inner_II
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], nd2_velD, CL_TRUE, 0, sizeof(int) * 18, nd2_vel, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, nd2_vel");

	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], drvh2D, CL_TRUE, 0, sizeof(float) * (*mw2_pml1) * 2, drvh2, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, drvh2");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], drti2D, CL_TRUE, 0, sizeof(float) * (*mw2_pml1) * 2, drti2, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, drti2");

	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], idmat2D, CL_TRUE, 0, sizeof(int) * (*nzbtm + 1) * (*nxbtm + 1) * (*nybtm +1), idmat2, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, idmat2");

	if (lbx[1] >= lbx[0])
	{
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], damp2_xD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nybtm) * (lbx[1] - lbx[0] + 1), damp2_x, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, damp2_x");
	}

	if (lby[1] >= lby[0])
	{
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], damp2_yD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * (lby[1] - lby[0] + 1), damp2_y, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, damp2_y");
	}
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], damp2_zD, CL_TRUE, 0, sizeof(float) * (*nxbtm) * (*nybtm), damp2_z, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, damp2_z");

	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], dxi2D, CL_TRUE, 0, sizeof(float) * 4 * (*nxbtm), dxi2, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, dxi2");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], dyi2D, CL_TRUE, 0, sizeof(float) * 4 * (*nybtm), dyi2, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, dyi2");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], dzi2D, CL_TRUE, 0, sizeof(float) * 4 * (*nzbtm), dzi2, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, dzi2");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], dxh2D, CL_TRUE, 0, sizeof(float) * 4 * (*nxbtm), dxh2, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, dxh2");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], dyh2D, CL_TRUE, 0, sizeof(float) * 4 * (*nybtm), dyh2, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, dyh2");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], dzh2D, CL_TRUE, 0, sizeof(float) * 4 * (*nzbtm), dzh2, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, dzh2");

	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2xxD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm + 3) * (*nybtm), t2xx, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t2xx");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2xyD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm + 3) * (*nybtm + 3), t2xy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t2xy");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2xzD, CL_TRUE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm), t2xz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t2xz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2yyD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * (*nybtm + 3), t2yy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t2yy");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2yzD, CL_TRUE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm) * (*nybtm + 3), t2yz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t2yz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2zzD, CL_TRUE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm) * (*nybtm), t2zz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t2zz");

	if (lbx[1] >= lbx[0])
	{
		nv2 = (lbx[1] - lbx[0] + 1) * (*mw2_pml);
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], v2x_pxD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * nv2 * (*nybtm), v2x_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, v2x_px");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], v2y_pxD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * nv2 * (*nybtm), v2y_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, v2y_px");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], v2z_pxD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * nv2 * (*nybtm), v2z_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, v2z_px");
	}

	if (lby[1] >= lby[0])
	{
		nv2 = (lby[1] - lby[0] + 1) * (*mw2_pml);
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], v2x_pyD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * nv2, v2x_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, v2x_py");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], v2y_pyD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * nv2, v2y_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, v2y_py");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], v2z_pyD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * nv2, v2z_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, v2z_py");
	}

	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], v2x_pzD, CL_TRUE, 0, sizeof(float) * (*mw2_pml) * (*nxbtm) * (*nybtm), v2x_pz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, v2x_pz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], v2y_pzD, CL_TRUE, 0, sizeof(float) * (*mw2_pml) * (*nxbtm) * (*nybtm), v2y_pz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, v2y_pz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], v2z_pzD, CL_TRUE, 0, sizeof(float) * (*mw2_pml) * (*nxbtm) * (*nybtm), v2z_pz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, v2z_pz");

	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], cixD, CL_TRUE, 0, sizeof(float) * (*nxbtm + 6 +1) * (8), cix, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, cix");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], ciyD, CL_TRUE, 0, sizeof(float) * (*nybtm + 6 +1) * (8), ciy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, ciy");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], chxD, CL_TRUE, 0, sizeof(float) * (*nxbtm + 6 +1) * (8), chx, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, chx");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], chyD, CL_TRUE, 0, sizeof(float) * (*nybtm + 6 +1) * (8), chy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, chy");
	//printf("done!\n");
	return;
}

void cpy_h2d_stressInputsCOneTimecl(int   *lbx,
		int   *lby,
		int   *nd1_txy,
		int   *nd1_txz,
		int   *nd1_tyy,
		int   *nd1_tyz,
		float *drti1,
		float *drth1,
		float *damp1_x,
		float *damp1_y,
		int	*idmat1,
		float *dxi1,
		float *dyi1,
		float *dzi1,
		float *dxh1,
		float *dyh1,
		float *dzh1,
		float *v1x,
		float *v1y,
		float *v1z,
		float *t1xx_px,
		float *t1xy_px,
		float *t1xz_px,
		float *t1yy_px,
		float *qt1xx_px,
		float *qt1xy_px,
		float *qt1xz_px,
		float *qt1yy_px,
		float *t1xx_py,
		float *t1xy_py,
		float *t1yy_py,
		float *t1yz_py,
		float *qt1xx_py,
		float *qt1xy_py,
		float *qt1yy_py,
		float *qt1yz_py,
		float *qt1xx,
		float *qt1xy,
		float *qt1xz,
		float *qt1yy,
		float *qt1yz,
		float *qt1zz,
		float *clamda,
		float *cmu,
		float *epdt,
		float *qwp,
		float *qws,
		float *qwt1,
		float *qwt2,
		int   *nd2_txy,
		int   *nd2_txz,
		int   *nd2_tyy,
		int   *nd2_tyz,
		float *drti2,
		float *drth2,
		int	*idmat2,
		float *damp2_x,
		float *damp2_y,
		float *damp2_z,
		float *dxi2,
		float *dyi2,
		float *dzi2,
		float *dxh2,
		float *dyh2,
		float *dzh2,
		float *v2x,
		float *v2y,
		float *v2z,
		float *qt2xx,
		float *qt2xy,
		float *qt2xz,
		float *qt2yy,
		float *qt2yz,
		float *qt2zz,
		float *t2xx_px,
		float *t2xy_px,
		float *t2xz_px,
		float *t2yy_px,
		float *qt2xx_px,
		float *qt2xy_px,
		float *qt2xz_px,
		float *qt2yy_px,
		float *t2xx_py,
		float *t2xy_py,
		float *t2yy_py,
		float *t2yz_py,
		float *qt2xx_py,
		float *qt2xy_py,
		float *qt2yy_py,
		float *qt2yz_py,
		float *t2xx_pz,
		float *t2xz_pz,
		float *t2yz_pz,
		float *t2zz_pz,
		float *qt2xx_pz,
		float *qt2xz_pz,
		float *qt2yz_pz,
		float *qt2zz_pz,
		int   *nmat,		//dimension #, int
		int	*mw1_pml1,	//int
		int	*mw2_pml1,	//int
		int	*nxtop,		//int
		int	*nytop,		//int
		int   *nztop,
		int	*mw1_pml,	//int
		int   *mw2_pml,	//int
		int	*nxbtm,		//int
		int	*nybtm,		//int
		int	*nzbtm,
		int   *nll)
{
	//printf("[OpenCL] initial h2d cpy for stress ........");
	// printf("lbx[0] = %d, lbx[1] = %d\n", lbx[0], lbx[1]);
	// printf("lby[0] = %d, lby[1] = %d\n", lby[0], lby[1]);

	cl_int errNum;
	int nti, nth;

	//for inner_I
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], nd1_txyD, CL_TRUE, 0, sizeof(int) * 18, nd1_txy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, nd1_txy");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], nd1_txzD, CL_TRUE, 0, sizeof(int) * 18, nd1_txz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, nd1_txz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], nd1_tyyD, CL_TRUE, 0, sizeof(int) * 18, nd1_tyy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, nd1_tyy");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], nd1_tyzD, CL_TRUE, 0, sizeof(int) * 18, nd1_tyz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, nd1_tyz");

	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], drti1D, CL_TRUE, 0, sizeof(float) * (*mw1_pml1) * 2, drti1, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, drti1");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], drth1D, CL_TRUE, 0, sizeof(float) * (*mw1_pml1) * 2, drth1, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, drth1");

	if (lbx[1] >= lbx[0])
	{
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], damp1_xD, CL_TRUE, 0, sizeof(float) * (*nztop + 1) * (*nytop) * (lbx[1] - lbx[0] + 1), damp1_x, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, damp1_x");
	}

	if (lby[1] >= lby[0])
	{
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], damp1_yD, CL_TRUE, 0, sizeof(float) * (*nztop + 1) * (*nxtop) * (lby[1] - lby[0] + 1), damp1_y, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, damp1_y");
	}

	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], idmat1D, CL_TRUE, 0, sizeof(int) * (*nztop + 2) * (*nxtop + 1) * (*nytop + 1), idmat1, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, idmat1");

	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], dxi1D, CL_TRUE, 0, sizeof(float) * 4 * (*nxtop), dxi1, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, dxi1");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], dyi1D, CL_TRUE, 0, sizeof(float) * 4 * (*nytop), dyi1, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, dyi1");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], dzi1D, CL_TRUE, 0, sizeof(float) * 4 * (*nztop + 1), dzi1, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, dzi1");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], dxh1D, CL_TRUE, 0, sizeof(float) * 4 * (*nxtop), dxh1, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, dxh1");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], dyh1D, CL_TRUE, 0, sizeof(float) * 4 * (*nytop), dyh1, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, dyh1");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], dzh1D, CL_TRUE, 0, sizeof(float) * 4 * (*nztop + 1), dzh1, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, dzh1");

	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], v1xD, CL_TRUE, 0, sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), v1x, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, v1x");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], v1yD, CL_TRUE, 0, sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), v1y, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, v1y");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], v1zD, CL_TRUE, 0, sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), v1z, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, v1z");

	if (lbx[1] >= lbx[0])
	{
		nti = (lbx[1] - lbx[0] + 1) * (*mw1_pml) + lbx[1];
		nth = (lbx[1] - lbx[0] + 1) * (*mw1_pml) + 1 - lbx[0];

		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1xx_pxD, CL_TRUE, 0, sizeof(float) * (*nztop) * (nti) * (*nytop), t1xx_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, t1xx_px");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1xy_pxD, CL_TRUE, 0, sizeof(float) * (*nztop) * nth * (*nytop), t1xy_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, t1xy_px");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1xz_pxD, CL_TRUE, 0, sizeof(float) * (*nztop+1) * nth * (*nytop), t1xz_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, t1xz_px");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1yy_pxD, CL_TRUE, 0, sizeof(float) * (*nztop) * nti * (*nytop), t1yy_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, t1yy_px");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], qt1xx_pxD, CL_TRUE, 0, sizeof(float) * (*nztop) * (nti) * (*nytop), qt1xx_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt1xx_px");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], qt1xy_pxD, CL_TRUE, 0, sizeof(float) * (*nztop) * nth * (*nytop), qt1xy_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt1xy_px");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], qt1xz_pxD, CL_TRUE, 0, sizeof(float) * (*nztop+1) * nth * (*nytop), qt1xz_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt1xz_px");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], qt1yy_pxD, CL_TRUE, 0, sizeof(float) * (*nztop) * nti * (*nytop), qt1yy_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt1yy_px");
	}

	if (lby[1] >= lby[0])
	{
		nti = (lby[1] - lby[0] + 1) * (*mw1_pml) + lby[1];
		nth = (lby[1] - lby[0] + 1) * (*mw1_pml) + 1 - lby[0];
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1xx_pyD, CL_TRUE, 0, sizeof(float) * (*nztop) * (*nxtop) * nti, t1xx_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, t1xx_py");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1xy_pyD, CL_TRUE, 0, sizeof(float) * (*nztop) * (*nxtop) * nth, t1xy_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, t1xy_py");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1yy_pyD, CL_TRUE, 0, sizeof(float) * (*nztop) * (*nxtop) * nti, t1yy_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, t1yy_py");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1yz_pyD, CL_TRUE, 0, sizeof(float) * (*nztop+1) * (*nxtop) * nth, t1yz_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, t1yz_py");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], qt1xx_pyD, CL_TRUE, 0, sizeof(float) * (*nztop) * (*nxtop) * nti, qt1xx_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt1xx_py");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], qt1xy_pyD, CL_TRUE, 0, sizeof(float) * (*nztop) * (*nxtop) * nth, qt1xy_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt1xy_py");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], qt1yy_pyD, CL_TRUE, 0, sizeof(float) * (*nztop) * (*nxtop) * nti, qt1yy_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt1yy_py");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], qt1yz_pyD, CL_TRUE, 0, sizeof(float) * (*nztop+1) * (*nxtop) * nth, qt1yz_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt1yz_py");
	}

	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], qt1xxD, CL_TRUE, 0, sizeof(float) * (*nztop) * (*nxtop) * (*nytop), qt1xx, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt1xx");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], qt1xyD, CL_TRUE, 0, sizeof(float) * (*nztop) * (*nxtop) * (*nytop), qt1xy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt1xy");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], qt1xzD, CL_TRUE, 0, sizeof(float) * (*nztop+1) * (*nxtop) * (*nytop), qt1xz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt1xz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], qt1yyD, CL_TRUE, 0, sizeof(float) * (*nztop) * (*nxtop) * (*nytop), qt1yy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt1yy");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], qt1yzD, CL_TRUE, 0, sizeof(float) * (*nztop+1) * (*nxtop) * (*nytop), qt1yz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt1yz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], qt1zzD, CL_TRUE, 0, sizeof(float) * (*nztop) * (*nxtop) * (*nytop), qt1zz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt1zz");

	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], clamdaD, CL_TRUE, 0, sizeof(float) * (*nmat), clamda, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, clamda");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], cmuD, CL_TRUE, 0, sizeof(float) * (*nmat), cmu, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, cmu");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], epdtD, CL_TRUE, 0, sizeof(float) * (*nll), epdt, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, epdt");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], qwpD, CL_TRUE, 0, sizeof(float) * (*nmat), qwp, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qwp");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], qwsD, CL_TRUE, 0, sizeof(float) * (*nmat), qws, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qws");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], qwt1D, CL_TRUE, 0, sizeof(float) * (*nll), qwt1, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qwt1");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], qwt2D, CL_TRUE, 0, sizeof(float) * (*nll), qwt2, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qwt2");

	//for inner_II
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], nd2_txyD, CL_TRUE, 0, sizeof(int) * 18, nd2_txy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, nd2_txy");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], nd2_txzD, CL_TRUE, 0, sizeof(int) * 18, nd2_txz, 0, NULL, NULL); 
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, nd2_txz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], nd2_tyyD, CL_TRUE, 0, sizeof(int) * 18, nd2_tyy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, nd2_tyy");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], nd2_tyzD, CL_TRUE, 0, sizeof(int) * 18, nd2_tyz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, nd2_tyz");

	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], drti2D, CL_TRUE, 0, sizeof(float) * (*mw2_pml1) * 2, drti2, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, drti2");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], drth2D, CL_TRUE, 0, sizeof(float) * (*mw2_pml1) * 2, drth2, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, drth2");

	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], idmat2D, CL_TRUE, 0, sizeof(int) * (*nzbtm + 1) * (*nxbtm + 1) * (*nybtm +1), idmat2, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, idmat2");

	if (lbx[1] >= lbx[0])
	{
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], damp2_xD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nybtm) * (lbx[1] - lbx[0] + 1), damp2_x, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, damp2_x");
	}

	if (lby[1] >= lby[0])
	{
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], damp2_yD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * (lby[1] - lby[0] + 1), damp2_y, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, damp2_y");
	}
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], damp2_zD, CL_TRUE, 0, sizeof(float) * (*nxbtm) * (*nybtm), damp2_z, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, damp2_z");

	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], dxi2D, CL_TRUE, 0, sizeof(float) * 4 * (*nxbtm), dxi2, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, dxi2");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], dyi2D, CL_TRUE, 0, sizeof(float) * 4 * (*nybtm), dyi2, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, dyi2");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], dzi2D, CL_TRUE, 0, sizeof(float) * 4 * (*nzbtm), dzi2, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, dzi2");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], dxh2D, CL_TRUE, 0, sizeof(float) * 4 * (*nxbtm), dxh2, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, dxh2");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], dyh2D, CL_TRUE, 0, sizeof(float) * 4 * (*nybtm), dyh2, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, dyh2");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], dzh2D, CL_TRUE, 0, sizeof(float) * 4 * (*nzbtm), dzh2, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, dzh2");

	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], v2xD, CL_TRUE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3), v2x, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, v2x");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], v2yD, CL_TRUE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3), v2y, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, v2y");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], v2zD, CL_TRUE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3), v2z, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, v2z");

	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], qt2xxD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * (*nybtm), qt2xx, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt2xx");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], qt2xyD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * (*nybtm), qt2xy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt2xy");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], qt2xzD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * (*nybtm), qt2xz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt2xz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], qt2yyD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * (*nybtm), qt2yy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt2yy");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], qt2yzD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * (*nybtm), qt2yz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt2yz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], qt2zzD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * (*nybtm), qt2zz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt2zz");

	if (lbx[1] >= lbx[0])
	{
		nti = (lbx[1] - lbx[0] + 1) * (*mw2_pml) + lbx[1];
		nth = (lbx[1] - lbx[0] + 1) * (*mw2_pml) + 1 - lbx[0];
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2xx_pxD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * nti * (*nybtm), t2xx_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, t2xx_px");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2xy_pxD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * nth * (*nybtm), t2xy_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, t2xy_px");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2xz_pxD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * nth * (*nybtm), t2xz_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, t2xz_px");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2yy_pxD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * nti * (*nybtm), t2yy_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, t2yy_px");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], qt2xx_pxD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * nti * (*nybtm), qt2xx_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt2xx_px");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], qt2xy_pxD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * nth * (*nybtm), qt2xy_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt2xy_px");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], qt2xz_pxD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * nth * (*nybtm), qt2xz_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt2xz_px");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], qt2yy_pxD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * nti * (*nybtm), qt2yy_px, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt2yy_px");
	}

	if (lby[1] >= lby[0])
	{
		nti = (lby[1] - lby[0] + 1) * (*mw2_pml) + lby[1];
		nth = (lby[1] - lby[0] + 1) * (*mw2_pml) + 1 - lby[0];
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2xx_pyD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * nti, t2xx_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, t2xx_py");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2xy_pyD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * nth, t2xy_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, t2xy_py");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2yy_pyD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * nti, t2yy_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, t2yy_py");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2yz_pyD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * nth, t2yz_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, t2yz_py");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], qt2xx_pyD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * nti, qt2xx_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt2xx_py");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], qt2xy_pyD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * nth, qt2xy_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt2xy_py");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], qt2yy_pyD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * nti, qt2yy_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt2yy_py");
		errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], qt2yz_pyD, CL_TRUE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * nth, qt2yz_py, 0, NULL, NULL);
		CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt2yz_py");
	}

	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2xx_pzD, CL_TRUE, 0, sizeof(float) * (*mw2_pml) * (*nxbtm) * (*nybtm), t2xx_pz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, t2xx_pz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2xz_pzD, CL_TRUE, 0, sizeof(float) * (*mw2_pml1) * (*nxbtm) * (*nybtm), t2xz_pz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, t2xz_pz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2yz_pzD, CL_TRUE, 0, sizeof(float) * (*mw2_pml1) * (*nxbtm) * (*nybtm), t2yz_pz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, t2yz_pz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2zz_pzD, CL_TRUE, 0, sizeof(float) * (*mw2_pml) * (*nxbtm) * (*nybtm), t2zz_pz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, t2zz_pz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], qt2xx_pzD, CL_TRUE, 0, sizeof(float) * (*mw2_pml) * (*nxbtm) * (*nybtm), qt2xx_pz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt2xx_pz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], qt2xz_pzD, CL_TRUE, 0, sizeof(float) * (*mw2_pml1) * (*nxbtm) * (*nybtm), qt2xz_pz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt2xz_pz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], qt2yz_pzD, CL_TRUE, 0, sizeof(float) * (*mw2_pml1) * (*nxbtm) * (*nybtm), qt2yz_pz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt2yz_pz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], qt2zz_pzD, CL_TRUE, 0, sizeof(float) * (*mw2_pml) * (*nxbtm) * (*nybtm), qt2zz_pz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, qt2zz_pz");

	//printf("done!\n");
	return;
}

void cpy_h2d_velocityInputsC_opencl(float *t1xx,
		float *t1xy,
		float *t1xz,
		float *t1yy,
		float *t1yz,
		float *t1zz,
		float *t2xx,
		float *t2xy,
		float *t2xz,
		float *t2yy,
		float *t2yz,
		float *t2zz,
		int	*nxtop,		
		int	*nytop,		
		int *nztop,
		int	*nxbtm,		
		int	*nybtm,		
		int	*nzbtm)
{
	//printf("[OpenCL] h2d cpy for input ........");

	cl_int errNum;
	int i;

	//for inner_I
#ifdef DISFD_EAGER_DATA_TRANSFER
	Start(&h2dTimerVelocity[0]);
#endif
	printf("[OpenCL H2D] vel1 0 size: %lu\n", 
			sizeof(float) * (*nztop) * (*nxtop) * (*nytop) +
			sizeof(float) * (*nztop + 1) * (*nxtop) * (*nytop + 3) +
			sizeof(float) * (*nztop) * (*nxtop) * (*nytop + 3) +
			sizeof(float) * (*nztop + 1) * (*nxtop + 3) * (*nytop) +
			sizeof(float) * (*nztop) * (*nxtop + 3) * (*nytop + 3) +
			sizeof(float) * (*nztop) * (*nxtop + 3) * (*nytop)
		  );
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1xxD, CL_FALSE, 0, sizeof(float) * (*nztop) * (*nxtop + 3) * (*nytop), t1xx, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t1xx");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1xyD, CL_FALSE, 0, sizeof(float) * (*nztop) * (*nxtop + 3) * (*nytop + 3), t1xy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t1xy");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1xzD, CL_FALSE, 0, sizeof(float) * (*nztop + 1) * (*nxtop + 3) * (*nytop), t1xz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t1xz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1yyD, CL_FALSE, 0, sizeof(float) * (*nztop) * (*nxtop) * (*nytop + 3), t1yy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t1yy");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1yzD, CL_FALSE, 0, sizeof(float) * (*nztop + 1) * (*nxtop) * (*nytop + 3), t1yz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t1yz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1zzD, CL_FALSE, 0, sizeof(float) * (*nztop) * (*nxtop) * (*nytop), t1zz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t1zz");

#ifdef DISFD_EAGER_DATA_TRANSFER
	errNum = clFinish(_cl_commandQueues[0]);
	CHECK_ERROR(errNum, "H2DS");
	Stop(&h2dTimerVelocity[0]);
	//for inner_II
	Start(&h2dTimerVelocity[1]);
#endif

	printf("[OpenCL H2D] vel1 1 size: %lu\n", 
			sizeof(float) * (*nzbtm + 1) * (*nxbtm) * (*nybtm) +
			sizeof(float) * (*nzbtm + 1) * (*nxbtm) * (*nybtm + 3) +
			sizeof(float) * (*nzbtm) * (*nxbtm) * (*nybtm + 3) +
			sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm) +
			sizeof(float) * (*nzbtm) * (*nxbtm + 3) * (*nybtm + 3) +
			sizeof(float) * (*nzbtm) * (*nxbtm + 3) * (*nybtm)
		  );
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2xxD, CL_FALSE, 0, sizeof(float) * (*nzbtm) * (*nxbtm + 3) * (*nybtm), t2xx, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t2xx");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2xyD, CL_FALSE, 0, sizeof(float) * (*nzbtm) * (*nxbtm + 3) * (*nybtm + 3), t2xy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t2xy");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2xzD, CL_FALSE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm), t2xz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t2xz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2yyD, CL_FALSE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * (*nybtm + 3), t2yy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t2yy");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2yzD, CL_FALSE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm) * (*nybtm + 3), t2yz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t2yz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2zzD, CL_FALSE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm) * (*nybtm), t2zz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice1, t2zz");
#ifdef DISFD_EAGER_DATA_TRANSFER
	errNum = clFinish(_cl_commandQueues[1]);
	CHECK_ERROR(errNum, "H2DS");
	Stop(&h2dTimerVelocity[1]);
#endif

#ifdef DISFD_H2D_SYNC_KERNEL 
	for(i = 0; i < NUM_COMMAND_QUEUES; i++) {
		Start(&h2dTimerVelocity[i]);
	}
	for(i = 0; i < NUM_COMMAND_QUEUES; i++) {
		errNum = clFinish(_cl_commandQueues[i]);
		Stop(&h2dTimerVelocity[i]);
		if(errNum != CL_SUCCESS) {
			fprintf(stderr, "Vel H2D Error!\n");
		}
	}
#endif
	//printf("done!\n");

	return;
}

void cpy_h2d_velocityOutputsC_opencl(float *v1x,
		float *v1y,
		float *v1z,
		float *v2x,
		float *v2y,
		float *v2z,
		int	*nxtop,	
		int	*nytop,
		int   *nztop,
		int	*nxbtm,
		int	*nybtm,
		int	*nzbtm)
{
	//printf("[OpenCL] h2d cpy for output ........");

	cl_int errNum;
	int i;

#ifdef DISFD_EAGER_DATA_TRANSFER
	Start(&h2dTimerVelocity[0]);
#endif
	//for inner_I
	printf("[OpenCL H2D] vel2 0 size: %lu\n", 
			sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3) +
			sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3) +
			sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3)
		  );
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], v1xD, CL_FALSE, 0, sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), v1x, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, v1x");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], v1yD, CL_FALSE, 0, sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), v1y, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, v1y");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], v1zD, CL_FALSE, 0, sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), v1z, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, v1z");

#ifdef DISFD_EAGER_DATA_TRANSFER
	errNum = clFinish(_cl_commandQueues[0]);
	CHECK_ERROR(errNum, "H2DS");
	Stop(&h2dTimerVelocity[0]);
	//for inner_II
	Start(&h2dTimerVelocity[1]);
#endif

	printf("[OpenCL H2D] vel2 1 size: %lu\n",
			sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3) +
			sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3) +
			sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3)
		  );
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], v2xD, CL_FALSE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3), v2x, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, v2x");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], v2yD, CL_FALSE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3), v2y, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, v2y");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], v2zD, CL_FALSE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3), v2z, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice1, v2z");
#ifdef DISFD_EAGER_DATA_TRANSFER
	errNum = clFinish(_cl_commandQueues[1]);
	CHECK_ERROR(errNum, "H2DS");
	Stop(&h2dTimerVelocity[1]);
#endif

#ifdef DISFD_H2D_SYNC_KERNEL 
	for(i = 0; i < NUM_COMMAND_QUEUES; i++) {
		Start(&h2dTimerVelocity[i]);
	}
	for(i = 0; i < NUM_COMMAND_QUEUES; i++) {
		errNum = clFinish(_cl_commandQueues[i]);
		Stop(&h2dTimerVelocity[i]);
		if(errNum != CL_SUCCESS) {
			fprintf(stderr, "Vel H2D Error!\n");
		}
	}
#endif
	//printf("done!\n");
	return;
}

void cpy_d2h_velocityOutputsC_opencl(float *v1x, 
		float *v1y,
		float *v1z,
		float *v2x,
		float *v2y,
		float *v2z,
		int	*nxtop,
		int	*nytop,
		int   *nztop,
		int	*nxbtm,
		int	*nybtm,
		int	*nzbtm)
{
	//printf("[OpenCL] d2h cpy for output ........");

	cl_int errNum;
	int i;
	printf("[OpenCL D2H] vel 0 size: %lu\n", 
			sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3) + 
			sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3) + 
			sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3)
		  );
	//for inner_I
#ifdef DISFD_EAGER_DATA_TRANSFER
	Start(&d2hTimerVelocity[0]);
#endif
	errNum = clEnqueueReadBuffer(_cl_commandQueues[0], v1xD, CL_FALSE, 0, sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), v1x, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyDeviceToHost1, v1x");
	errNum = clEnqueueReadBuffer(_cl_commandQueues[0], v1yD, CL_FALSE, 0, sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), v1y, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyDeviceToHost1, v1y");
	errNum = clEnqueueReadBuffer(_cl_commandQueues[0], v1zD, CL_FALSE, 0, sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), v1z, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyDeviceToHost1, v1z");

#ifdef DISFD_EAGER_DATA_TRANSFER
	errNum = clFinish(_cl_commandQueues[0]);
	CHECK_ERROR(errNum, "H2DS");
	Stop(&d2hTimerVelocity[0]);
	//for inner_II
	Start(&d2hTimerVelocity[1]);
#endif

	printf("[OpenCL D2H] vel 1 size: %lu\n", 
			sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3) + 
			sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3) + 
			sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3)
		  );
	errNum = clEnqueueReadBuffer(_cl_commandQueues[1], v2xD, CL_FALSE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3), v2x, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyDeviceToHost1, v2x");
	errNum = clEnqueueReadBuffer(_cl_commandQueues[1], v2yD, CL_FALSE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3), v2y, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyDeviceToHost1, v2y");
	errNum = clEnqueueReadBuffer(_cl_commandQueues[1], v2zD, CL_FALSE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3), v2z, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyDeviceToHost1, vzz");

#ifdef DISFD_EAGER_DATA_TRANSFER
	errNum = clFinish(_cl_commandQueues[1]);
	CHECK_ERROR(errNum, "H2DS");
	Stop(&d2hTimerVelocity[1]);
#endif

	//#ifdef DISFD_H2D_SYNC_KERNEL
	for(i = 0; i < NUM_COMMAND_QUEUES; i++) {
		Start(&d2hTimerVelocity[i]);
	}
	for(i = 0; i < NUM_COMMAND_QUEUES; i++) {
		errNum = clFinish(_cl_commandQueues[i]);
		Stop(&d2hTimerVelocity[i]);
		if(errNum != CL_SUCCESS) {
			fprintf(stderr, "Vel H2D Error!\n");
		}
	}
	//#endif
	//printf("done!\n");
	return;
}

void compute_velocityC_opencl(int *nztop, int *nztm1, float *ca, int *lbx,
		int *lby, int *nd1_vel, float *rhoM, float *drvh1M, float *drti1M,
		float *damp1_xM, float *damp1_yM, int *idmat1M, float *dxi1M, float *dyi1M,
		float *dzi1M, float *dxh1M, float *dyh1M, float *dzh1M, float *t1xxM,
		float *t1xyM, float *t1xzM, float *t1yyM, float *t1yzM, float *t1zzM,
		void **v1xMp, void **v1yMp, void **v1zMp, float *v1x_pxM, float *v1y_pxM,
		float *v1z_pxM, float *v1x_pyM, float *v1y_pyM, float *v1z_pyM, 
		int *nzbm1, int *nd2_vel, float *drvh2M, float *drti2M, 
		int *idmat2M, float *damp2_xM, float *damp2_yM, float *damp2_zM,
		float *dxi2M, float *dyi2M, float *dzi2M, float *dxh2M, float *dyh2M,
		float *dzh2M, float *t2xxM, float *t2xyM, float *t2xzM, float *t2yyM,
		float *t2yzM, float *t2zzM, void **v2xMp, void **v2yMp, void **v2zMp,
		float *v2x_pxM, float *v2y_pxM, float *v2z_pxM, float *v2x_pyM, 
		float *v2y_pyM, float *v2z_pyM, float *v2x_pzM, float *v2y_pzM,
		float *v2z_pzM, int *nmat,	int *mw1_pml1, int *mw2_pml1, 
		int *nxtop, int *nytop, int *mw1_pml, int *mw2_pml,
		int *nxbtm, int *nybtm, int *nzbtm, int *myid)
{
	static int counter = 0;
	int i;
	//printf("[OpenCL] velocity computation:\n"); 
	cl_int errNum;
	//define the dimensions of different kernels
	int blockSizeX = 16;
	int blockSizeY = 16;

	float *v1xM, *v1yM, *v1zM, *v2xM, *v2yM, *v2zM;

	// extract specific input/output pointers
	v1xM=(float *) *v1xMp;
	v1yM=(float *) *v1yMp;
	v1zM=(float *) *v1zMp;
	v2xM=(float *) *v2xMp;
	v2yM=(float *) *v2yMp;
	v2zM=(float *) *v2zMp;

	procID = *myid;

	gettimeofday(&t1, NULL);
	//Start(&h2dTimerVelocity);
#ifndef DISFD_GPU_MARSHALING
	cpy_h2d_velocityInputsC_opencl(t1xxM, t1xyM, t1xzM, t1yyM, t1yzM, t1zzM, t2xxM, t2xyM, t2xzM, t2yyM, t2yzM, t2zzM, nxtop, nytop, nztop, nxbtm, nybtm, nzbtm);

	cpy_h2d_velocityOutputsC_opencl(v1xM, v1yM, v1zM, v2xM, v2yM, v2zM, nxtop, nytop, nztop, nxbtm, nybtm, nzbtm);
#endif
	//Stop(&h2dTimerVelocity);
	gettimeofday(&t2, NULL);
	tmpTime = 1000.0 * (t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec) / 1000.0;
	totalTimeH2DV += tmpTime;

	gettimeofday(&t1, NULL);
	for(i = 0; i < NUM_COMMAND_QUEUES; i++) {
		Start(&kernelTimerVelocity[i]);
		printf("DISFD Q%d Velocity Set Kernel Args\n", i);
		if(counter < 1) {
#ifdef SNUCL_PERF_MODEL_OPTIMIZATION
			clSetCommandQueueProperty(_cl_commandQueues[i], 
#ifdef SNUCL_PERF_MODEL_OPTIMIZATION
					CL_QUEUE_AUTO_DEVICE_SELECTION | 
					CL_QUEUE_ITERATIVE | 
					CL_QUEUE_IO_INTENSIVE | 
					//CL_QUEUE_COMPUTE_INTENSIVE |
#endif
					CL_QUEUE_PROFILING_ENABLE, 
					true,
					NULL);
#endif
		}
	}
#ifdef DISFD_PAPI
	papi_start_all_events();
#endif
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
#ifdef DISFD_USE_ROW_MAJOR_DATA
	int gridSizeX1 = (*nztm1 + 1)/blockSizeX + 1;
	int gridSizeY1 = (nd1_vel[9] - nd1_vel[8])/blockSizeY + 1;
#else
	int gridSizeX1 = (nd1_vel[3] - nd1_vel[2])/blockSizeX + 1;
	int gridSizeY1 = (nd1_vel[9] - nd1_vel[8])/blockSizeY + 1;
#endif 
	size_t dimGrid1[3] = {gridSizeX1, gridSizeY1, 1};
	//OpenCL code
	errNum = clSetKernelArg(_cl_kernel_velocity_inner_IC, 0, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 1, sizeof(int), nztm1);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 2, sizeof(float), ca);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 3, sizeof(cl_mem), &nd1_velD);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 4, sizeof(cl_mem), &rhoD);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 5, sizeof(cl_mem), &idmat1D);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 6, sizeof(cl_mem), &dxi1D);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 7, sizeof(cl_mem), &dyi1D);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 8, sizeof(cl_mem), &dzi1D);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 9, sizeof(cl_mem), &dxh1D);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 10, sizeof(cl_mem), &dyh1D);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 11, sizeof(cl_mem), &dzh1D);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 12, sizeof(cl_mem), &t1xxD);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 13, sizeof(cl_mem), &t1xyD);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 14, sizeof(cl_mem), &t1xzD);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 15, sizeof(cl_mem), &t1yyD);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 16, sizeof(cl_mem), &t1yzD);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 17, sizeof(cl_mem), &t1zzD);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 18, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 19, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 20, sizeof(cl_mem), &v1xD);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 21, sizeof(cl_mem), &v1yD);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IC, 22, sizeof(cl_mem), &v1zD);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_velocity_inner_IC arguments!\n");
	}
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid1[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid1[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid1[2]*localWorkSize[2];
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[0], _cl_kernel_velocity_inner_IC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_velocity_inner_IC for execution!\n");
	}
	//errNum = clFinish(_cl_commandQueues[0]);
	//if(errNum != CL_SUCCESS) {
	//    fprintf(stderr, "Error: finishing velocity for execution!\n");
	//}

#ifdef DISFD_USE_ROW_MAJOR_DATA
	int gridSizeX2 = (*nztop - 1)/blockSizeX + 1;
	int gridSizeY2 = (nd1_vel[5] - nd1_vel[0])/blockSizeX + 1;
#else
	int gridSizeX2 = (nd1_vel[5] - nd1_vel[0])/blockSizeX + 1;
	int gridSizeY2 = (lbx[1] - lbx[0])/blockSizeY + 1;
#endif
	size_t dimGrid2[3] = {gridSizeX2, gridSizeY2, 1};
	//	printf("myid = %d, grid2 = (%d, %d)\n", *myid, gridSizeX2, gridSizeY2);

	if (lbx[1] >= lbx[0])
	{
		errNum = clSetKernelArg(_cl_kernel_vel_PmlX_IC, 0, sizeof(float), ca);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 1, sizeof(int), &lbx[0]);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 2, sizeof(int), &lbx[1]);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 3, sizeof(cl_mem), &nd1_velD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 4, sizeof(cl_mem), &rhoD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 5, sizeof(cl_mem), &drvh1D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 6, sizeof(cl_mem), &drti1D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 7, sizeof(cl_mem), &damp1_xD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 8, sizeof(cl_mem), &idmat1D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 9, sizeof(cl_mem), &dxi1D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 10, sizeof(cl_mem), &dyi1D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 11, sizeof(cl_mem), &dzi1D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 12, sizeof(cl_mem), &dxh1D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 13, sizeof(cl_mem), &dyh1D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 14, sizeof(cl_mem), &dzh1D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 15, sizeof(cl_mem), &t1xxD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 16, sizeof(cl_mem), &t1xyD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 17, sizeof(cl_mem), &t1xzD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 18, sizeof(cl_mem), &t1yyD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 19, sizeof(cl_mem), &t1yzD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 20, sizeof(cl_mem), &t1zzD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 21, sizeof(int), mw1_pml1);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 22, sizeof(int), mw1_pml);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 23, sizeof(int), nxtop);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 24, sizeof(int), nytop);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 25, sizeof(int), nztop);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 26, sizeof(cl_mem), &v1xD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 27, sizeof(cl_mem), &v1yD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 28, sizeof(cl_mem), &v1zD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 29, sizeof(cl_mem), &v1x_pxD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 30, sizeof(cl_mem), &v1y_pxD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IC, 31, sizeof(cl_mem), &v1z_pxD);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: setting kernel _cl_kernel_vel_PmlX_IC arguments!\n");
		}
		localWorkSize[0] = dimBlock[0];
		localWorkSize[1] = dimBlock[1];
		localWorkSize[2] = dimBlock[2];
		globalWorkSize[0] = dimGrid2[0]*localWorkSize[0];
		globalWorkSize[1] = dimGrid2[1]*localWorkSize[1];
		globalWorkSize[2] = dimGrid2[2]*localWorkSize[2];
		errNum = clEnqueueNDRangeKernel(_cl_commandQueues[0], _cl_kernel_vel_PmlX_IC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_PmlX_IC for execution!\n");
		}
	}

#ifdef DISFD_USE_ROW_MAJOR_DATA
	int gridSizeX3 = (*nztop-1)/blockSizeX + 1;
	int gridSizeY3 = (nd1_vel[11] - nd1_vel[6])/blockSizeY + 1;

#else
	int gridSizeX3 = (lby[1] - lby[0])/blockSizeX + 1;
	int gridSizeY3 = (nd1_vel[11] - nd1_vel[6])/blockSizeY + 1;
#endif
	size_t dimGrid3[3] = {gridSizeX3, gridSizeY3, 1};

	//	printf("myid = %d, grid3 = (%d, %d)\n", *myid, gridSizeX3, gridSizeY3);
	if (lby[1] >= lby[0])
	{
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 0, sizeof(int), nztop);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 1, sizeof(float), ca);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 2, sizeof(int), &lby[0]);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 3, sizeof(int), &lby[1]);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 4, sizeof(cl_mem), &nd1_velD);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 5, sizeof(cl_mem), &rhoD);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 6, sizeof(cl_mem), &drvh1D);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 7, sizeof(cl_mem), &drti1D);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 8, sizeof(cl_mem), &idmat1D);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 9, sizeof(cl_mem), &damp1_yD);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 10, sizeof(cl_mem), &dxi1D);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 11, sizeof(cl_mem), &dyi1D);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 12, sizeof(cl_mem), &dzi1D);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 13, sizeof(cl_mem), &dxh1D);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 14, sizeof(cl_mem), &dyh1D);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 15, sizeof(cl_mem), &dzh1D);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 16, sizeof(cl_mem), &t1xxD);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 17, sizeof(cl_mem), &t1xyD);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 18, sizeof(cl_mem), &t1xzD);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 19, sizeof(cl_mem), &t1yyD);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 20, sizeof(cl_mem), &t1yzD);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 21, sizeof(cl_mem), &t1zzD);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 22, sizeof(int), mw1_pml1);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 23, sizeof(int), mw1_pml);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 24, sizeof(int), nxtop);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 25, sizeof(int), nytop);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 26, sizeof(cl_mem), &v1xD);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 27, sizeof(cl_mem), &v1yD);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 28, sizeof(cl_mem), &v1zD);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 29, sizeof(cl_mem), &v1x_pyD);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 30, sizeof(cl_mem), &v1y_pyD);
		clSetKernelArg(_cl_kernel_vel_PmlY_IC, 31, sizeof(cl_mem), &v1z_pyD);
		localWorkSize[0] = dimBlock[0];
		localWorkSize[1] = dimBlock[1];
		localWorkSize[2] = dimBlock[2];
		globalWorkSize[0] = dimGrid3[0]*localWorkSize[0];
		globalWorkSize[1] = dimGrid3[1]*localWorkSize[1];
		globalWorkSize[2] = dimGrid3[2]*localWorkSize[2];
		clEnqueueNDRangeKernel(_cl_commandQueues[0], _cl_kernel_vel_PmlY_IC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
	}

#ifdef DISFD_USE_ROW_MAJOR_DATA
	int gridSizeX4 = (nd2_vel[15] - 2)/blockSizeX + 1;
	int gridSizeY4 = (nd2_vel[9] - nd2_vel[8])/blockSizeY + 1;	
#else
	int gridSizeX4 = (nd2_vel[3] - nd2_vel[2])/blockSizeX + 1;
	int gridSizeY4 = (nd2_vel[9] - nd2_vel[8])/blockSizeY + 1;
#endif
	size_t dimGrid4[3] = {gridSizeX4, gridSizeY4, 1};
	//	printf("myid = %d, grid4 = (%d, %d)\n", *myid, gridSizeX4, gridSizeY4);

	errNum = clSetKernelArg(_cl_kernel_velocity_inner_IIC, 0, sizeof(float), ca);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IIC, 1, sizeof(cl_mem), &nd2_velD);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IIC, 2, sizeof(cl_mem), &rhoD);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IIC, 3, sizeof(cl_mem), &dxi2D);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IIC, 4, sizeof(cl_mem), &dyi2D);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IIC, 5, sizeof(cl_mem), &dzi2D);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IIC, 6, sizeof(cl_mem), &dxh2D);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IIC, 7, sizeof(cl_mem), &dyh2D);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IIC, 8, sizeof(cl_mem), &dzh2D);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IIC, 9, sizeof(cl_mem), &idmat2D);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IIC, 10, sizeof(cl_mem), &t2xxD);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IIC, 11, sizeof(cl_mem), &t2xyD);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IIC, 12, sizeof(cl_mem), &t2xzD);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IIC, 13, sizeof(cl_mem), &t2yyD);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IIC, 14, sizeof(cl_mem), &t2yzD);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IIC, 15, sizeof(cl_mem), &t2zzD);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IIC, 16, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IIC, 17, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IIC, 18, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IIC, 19, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IIC, 20, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_velocity_inner_IIC, 21, sizeof(cl_mem), &v2zD);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_velocity_inner_IIC arguments!\n");
	}
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid4[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid4[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid4[2]*localWorkSize[2];
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[1], _cl_kernel_velocity_inner_IIC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_velocity_inner_IIC for execution!\n");
	}

#ifdef DISFD_USE_ROW_MAJOR_DATA
	int gridSizeX5 = (*nzbm1 - 1)/blockSizeX + 1;
	int gridSizeY5 = (nd2_vel[5] - nd2_vel[0])/blockSizeY + 1;
#else 
	int gridSizeX5 = (nd2_vel[5] - nd2_vel[0])/blockSizeX + 1;
	int gridSizeY5 = (lbx[1] - lbx[0])/blockSizeY + 1;
#endif
	size_t dimGrid5[3] = {gridSizeX5, gridSizeY5, 1};
	//	printf("myid = %d, grid5 = (%d, %d)\n", *myid, gridSizeX5, gridSizeY5);

	if (lbx[1] >= lbx[0])
	{
		errNum = clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 0, sizeof(int), nzbm1);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 1, sizeof(float), ca);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 2, sizeof(int), &lbx[0]);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 3, sizeof(int), &lbx[1]);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 4, sizeof(cl_mem), &nd2_velD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 5, sizeof(cl_mem), &drvh2D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 6, sizeof(cl_mem), &drti2D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 7, sizeof(cl_mem), &rhoD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 8, sizeof(cl_mem), &damp2_xD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 9, sizeof(cl_mem), &idmat2D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 10, sizeof(cl_mem), &dxi2D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 11, sizeof(cl_mem), &dyi2D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 12, sizeof(cl_mem), &dzi2D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 13, sizeof(cl_mem), &dxh2D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 14, sizeof(cl_mem), &dyh2D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 15, sizeof(cl_mem), &dzh2D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 16, sizeof(cl_mem), &t2xxD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 17, sizeof(cl_mem), &t2xyD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 18, sizeof(cl_mem), &t2xzD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 19, sizeof(cl_mem), &t2yyD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 20, sizeof(cl_mem), &t2yzD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 21, sizeof(cl_mem), &t2zzD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 22, sizeof(int), mw2_pml1);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 23, sizeof(int), mw2_pml);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 24, sizeof(int), nxbtm);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 25, sizeof(int), nybtm);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 26, sizeof(int), nzbtm);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 27, sizeof(cl_mem), &v2xD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 28, sizeof(cl_mem), &v2yD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 29, sizeof(cl_mem), &v2zD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 30, sizeof(cl_mem), &v2x_pxD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 31, sizeof(cl_mem), &v2y_pxD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlX_IIC, 32, sizeof(cl_mem), &v2z_pxD);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: setting kernel _cl_kernel_vel_PmlX_IIC arguments!\n");
		}
		localWorkSize[0] = dimBlock[0];
		localWorkSize[1] = dimBlock[1];
		localWorkSize[2] = dimBlock[2];
		globalWorkSize[0] = dimGrid5[0]*localWorkSize[0];
		globalWorkSize[1] = dimGrid5[1]*localWorkSize[1];
		globalWorkSize[2] = dimGrid5[2]*localWorkSize[2];
		errNum = clEnqueueNDRangeKernel(_cl_commandQueues[1], _cl_kernel_vel_PmlX_IIC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_PmlX_IIC for execution!\n");
		}
	}

#ifdef DISFD_USE_ROW_MAJOR_DATA
	int gridSizeX6 = (*nzbm1 -1)/blockSizeX + 1;
	int gridSizeY6 = (nd2_vel[11] - nd2_vel[6])/blockSizeY + 1;
#else
	int gridSizeX6 = (lby[1] - lby[0])/blockSizeX + 1;
	int gridSizeY6 = (nd2_vel[11] - nd2_vel[6])/blockSizeY + 1;
#endif
	size_t dimGrid6[3] = {gridSizeX6, gridSizeY6, 1};
	//	printf("myid = %d, grid = (%d, %d)\n", *myid, gridSizeX6, gridSizeY6);

	if (lby[1] >= lby[0])
	{
		errNum = clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 0, sizeof(int), nzbm1);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 1, sizeof(float), ca);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 2, sizeof(int), &lby[0]);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 3, sizeof(int), &lby[1]);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 4, sizeof(cl_mem), &nd2_velD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 5, sizeof(cl_mem), &drvh2D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 6, sizeof(cl_mem), &drti2D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 7, sizeof(cl_mem), &rhoD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 8, sizeof(cl_mem), &damp2_yD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 9, sizeof(cl_mem), &idmat2D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 10, sizeof(cl_mem), &dxi2D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 11, sizeof(cl_mem), &dyi2D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 12, sizeof(cl_mem), &dzi2D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 13, sizeof(cl_mem), &dxh2D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 14, sizeof(cl_mem), &dyh2D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 15, sizeof(cl_mem), &dzh2D);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 16, sizeof(cl_mem), &t2xxD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 17, sizeof(cl_mem), &t2xyD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 18, sizeof(cl_mem), &t2xzD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 19, sizeof(cl_mem), &t2yyD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 20, sizeof(cl_mem), &t2yzD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 21, sizeof(cl_mem), &t2zzD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 22, sizeof(int), mw2_pml1);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 23, sizeof(int), mw2_pml);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 24, sizeof(int), nxbtm);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 25, sizeof(int), nybtm);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 26, sizeof(int), nzbtm);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 27, sizeof(cl_mem), &v2xD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 28, sizeof(cl_mem), &v2yD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 29, sizeof(cl_mem), &v2zD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 30, sizeof(cl_mem), &v2x_pyD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 31, sizeof(cl_mem), &v2y_pyD);
		errNum |= clSetKernelArg(_cl_kernel_vel_PmlY_IIC, 32, sizeof(cl_mem), &v2z_pyD);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: setting kernel _cl_kernel_vel_PmlY_IIC arguments!\n");
		}
		localWorkSize[0] = dimBlock[0];
		localWorkSize[1] = dimBlock[1];
		localWorkSize[2] = dimBlock[2];
		globalWorkSize[0] = dimGrid6[0]*localWorkSize[0];
		globalWorkSize[1] = dimGrid6[1]*localWorkSize[1];
		globalWorkSize[2] = dimGrid6[2]*localWorkSize[2];
		errNum = clEnqueueNDRangeKernel(_cl_commandQueues[1], _cl_kernel_vel_PmlY_IIC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_PmlY_IIC for execution!\n");
		}
	}

#ifdef DISFD_USE_ROW_MAJOR_DATA
	int gridSizeX7 = (*nzbm1 - 1)/blockSizeX + 1;
	int gridSizeY7 = (nd2_vel[11] - nd2_vel[6])/blockSizeY + 1;
#else
	int gridSizeX7 = (nd2_vel[5] - nd2_vel[0])/blockSizeX + 1;
	int gridSizeY7 = (nd2_vel[11] - nd2_vel[6])/blockSizeY + 1;
#endif
	size_t dimGrid7[3] = {gridSizeX7, gridSizeY7, 1};
	//	printf("myid = %d, grid7 = (%d, %d)\n", *myid, gridSizeX7, gridSizeY7);

	errNum = clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 0, sizeof(int), nzbm1);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 1, sizeof(float), ca);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 2, sizeof(cl_mem), &nd2_velD);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 3, sizeof(cl_mem), &drvh2D);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 4, sizeof(cl_mem), &drti2D);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 5, sizeof(cl_mem), &rhoD);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 6, sizeof(cl_mem), &damp2_zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 7, sizeof(cl_mem), &idmat2D);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 8, sizeof(cl_mem), &dxi2D);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 9, sizeof(cl_mem), &dyi2D);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 10, sizeof(cl_mem), &dzi2D);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 11, sizeof(cl_mem), &dxh2D);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 12, sizeof(cl_mem), &dyh2D);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 13, sizeof(cl_mem), &dzh2D);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 14, sizeof(cl_mem), &t2xxD);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 15, sizeof(cl_mem), &t2xyD);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 16, sizeof(cl_mem), &t2xzD);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 17, sizeof(cl_mem), &t2yyD);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 18, sizeof(cl_mem), &t2yzD);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 19, sizeof(cl_mem), &t2zzD);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 20, sizeof(int), mw2_pml1);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 21, sizeof(int), mw2_pml);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 22, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 23, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 24, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 25, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 26, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 27, sizeof(cl_mem), &v2zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 28, sizeof(cl_mem), &v2x_pzD);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 29, sizeof(cl_mem), &v2y_pzD);
	errNum |= clSetKernelArg(_cl_kernel_vel_PmlZ_IIC, 30, sizeof(cl_mem), &v2z_pzD);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_PmlZ_IIC arguments!\n");
	}
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid7[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid7[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid7[2]*localWorkSize[2];
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[1], _cl_kernel_vel_PmlZ_IIC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_PmlZ_IIC for execution!\n");
	}

	for(i = 0; i < NUM_COMMAND_QUEUES; i++) {
#ifdef SNUCL_PERF_MODEL_OPTIMIZATION
		if(counter < 1) {
			clSetCommandQueueProperty(_cl_commandQueues[i], 
#ifdef SNUCL_PERF_MODEL_OPTIMIZATION
#endif
					CL_QUEUE_PROFILING_ENABLE, 
					true,
					NULL);
		}
		else 
#endif
		{
			errNum = clFinish(_cl_commandQueues[i]);
		printf("DISFD Q%d Velocity Set Kernel Args\n", i);
			if(errNum != CL_SUCCESS)
			{
				fprintf(stderr, "Error: finishing stress velocity for execution!\n");
			}
		}
		printf("DISFD Q%d Velocity ReSet Kernel Args\n", i);
		Stop(&kernelTimerVelocity[i]);
	}
	counter++;
#ifdef DISFD_PAPI
	papi_accum_all_events();
	//papi_stop_all_events();
	papi_print_all_events();
	//papi_reset_all_events();
#endif
	gettimeofday(&t2, NULL);
	tmpTime = 1000.0 * (t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec) / 1000.0;
	totalTimeCompV += tmpTime;

	gettimeofday(&t1, NULL);
	//Start(&d2hTimerVelocity);
#if !defined(DISFD_GPU_MARSHALING) || defined(DISFD_DEBUG)
	cpy_d2h_velocityOutputsC_opencl(v1xM, v1yM, v1zM, v2xM, v2yM, v2zM, nxtop,	nytop, nztop, nxbtm, nybtm, nzbtm);
#endif
	//Stop(&d2hTimerVelocity);
	gettimeofday(&t2, NULL);
	tmpTime = 1000.0 * (t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec) / 1000.0;
	totalTimeD2HV += tmpTime;

#ifdef DISFD_DEBUG
	if(counter == 10) {
	int size = (*nztop + 2) * (*nxtop + 3) * (*nytop + 3); 
	write_output(v1xM, size, "OUTPUT_ARRAYS/v1xM.txt");
	write_output(v1yM, size, "OUTPUT_ARRAYS/v1yM.txt");
	write_output(v1zM, size, "OUTPUT_ARRAYS/v1zM.txt");
	size = (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3);
	write_output(v2xM, size, "OUTPUT_ARRAYS/v2xM.txt");
	write_output(v2yM, size, "OUTPUT_ARRAYS/v2yM.txt");
	write_output(v2zM, size, "OUTPUT_ARRAYS/v2zM.txt");
	}
#endif
	return;
}

void cpy_h2d_stressInputsC_opencl(float *v1x,
		float *v1y,
		float *v1z,
		float *v2x,
		float *v2y,
		float *v2z,
		int	*nxtop,
		int	*nytop,
		int  *nztop,
		int	*nxbtm,
		int	*nybtm,
		int	*nzbtm)
{
	//printf("[OpenCL] h2d cpy for input ........");
	int i;

	cl_int errNum;

	//for inner_I
#ifdef DISFD_EAGER_DATA_TRANSFER
	Start(&h2dTimerStress[0]);
#endif
	printf("[OpenCL H2D] str1 0 size: %lu\n",
			sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3) + 
			sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3) + 
			sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3)
		  );
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], v1xD, CL_FALSE, 0, sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), v1x, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice, v1x");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], v1yD, CL_FALSE, 0, sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), v1y, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice, v1y");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], v1zD, CL_FALSE, 0, sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), v1z, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice, v1z");

#ifdef DISFD_EAGER_DATA_TRANSFER
	errNum = clFinish(_cl_commandQueues[0]);
	CHECK_ERROR(errNum, "H2DS");
	Stop(&h2dTimerStress[0]);
	//for inner_II
	Start(&h2dTimerStress[1]);
#endif

	printf("[OpenCL H2D] str1 1 size: %lu\n",
			sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3) +
			sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3) +
			sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3)
		  );
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], v2xD, CL_FALSE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3), v2x, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice, v2x");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], v2yD, CL_FALSE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3), v2y, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice, v2y");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], v2zD, CL_FALSE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3), v2z, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice, v2z");

#ifdef DISFD_EAGER_DATA_TRANSFER
	errNum = clFinish(_cl_commandQueues[1]);
	CHECK_ERROR(errNum, "H2DS");
	Stop(&h2dTimerStress[1]);
#endif

#ifdef DISFD_H2D_SYNC_KERNEL 
	for(i = 0; i < NUM_COMMAND_QUEUES; i++) {
		Start(&h2dTimerStress[i]);
	}
	for(i = 0; i < NUM_COMMAND_QUEUES; i++) {
		errNum = clFinish(_cl_commandQueues[i]);
		Stop(&h2dTimerStress[i]);
		if(errNum != CL_SUCCESS) {
			fprintf(stderr, "Vel H2D Error!\n");
		}
	}
#endif
	//printf("done!\n");
	return;
}

void cpy_h2d_stressOutputsC_opencl(float *t1xx,
		float *t1xy,
		float *t1xz,
		float *t1yy,
		float *t1yz,
		float *t1zz,
		float *t2xx,
		float *t2xy,
		float *t2xz,
		float *t2yy,
		float *t2yz,
		float *t2zz,
		int	  *nxtop,
		int	  *nytop,
		int   *nztop,
		int	  *nxbtm,
		int	  *nybtm,
		int	  *nzbtm)
{
	//printf("[OpenCL] h2d cpy for output ........");
	cl_int errNum;
	int i;
	int nth, nti;

#ifdef DISFD_EAGER_DATA_TRANSFER
	Start(&h2dTimerStress[0]);
#endif
	printf("[OpenCL H2D] str2 0 size: %lu\n",
			sizeof(float) * (*nztop) * (*nxtop) * (*nytop) +
			sizeof(float) * (*nztop + 1) * (*nxtop) * (*nytop + 3) +
			sizeof(float) * (*nztop) * (*nxtop) * (*nytop + 3) +
			sizeof(float) * (*nztop + 1) * (*nxtop + 3) * (*nytop) +
			sizeof(float) * (*nztop) * (*nxtop + 3) * (*nytop + 3) +
			sizeof(float) * (*nztop) * (*nxtop + 3) * (*nytop)
		  );
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1xxD, CL_FALSE, 0, sizeof(float) * (*nztop) * (*nxtop + 3) * (*nytop), t1xx, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice, t1xx");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1xyD, CL_FALSE, 0, sizeof(float) * (*nztop) * (*nxtop + 3) * (*nytop + 3), t1xy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice, t1xy");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1xzD, CL_FALSE, 0, sizeof(float) * (*nztop + 1) * (*nxtop + 3) * (*nytop), t1xz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice, t1xz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1yyD, CL_FALSE, 0, sizeof(float) * (*nztop) * (*nxtop) * (*nytop + 3), t1yy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice, t1yy");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1yzD, CL_FALSE, 0, sizeof(float) * (*nztop + 1) * (*nxtop) * (*nytop + 3), t1yz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice, t1yz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[0], t1zzD, CL_FALSE, 0, sizeof(float) * (*nztop) * (*nxtop) * (*nytop), t1zz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice, t1zz");

#ifdef DISFD_EAGER_DATA_TRANSFER
	errNum = clFinish(_cl_commandQueues[0]);
	CHECK_ERROR(errNum, "H2DS");
	Stop(&h2dTimerStress[0]);
	//for inner_II
	Start(&h2dTimerStress[1]);
#endif

	printf("[OpenCL H2D] str2 1 size: %lu\n",
			sizeof(float) * (*nzbtm + 1) * (*nxbtm) * (*nybtm) +
			sizeof(float) * (*nzbtm + 1) * (*nxbtm) * (*nybtm + 3) +
			sizeof(float) * (*nzbtm) * (*nxbtm) * (*nybtm + 3) +
			sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm) +
			sizeof(float) * (*nzbtm) * (*nxbtm + 3) * (*nybtm + 3) +
			sizeof(float) * (*nzbtm) * (*nxbtm + 3) * (*nybtm)
		  );
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2xxD, CL_FALSE, 0, sizeof(float) * (*nzbtm) * (*nxbtm + 3) * (*nybtm), t2xx, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice, t2xx");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2xyD, CL_FALSE, 0, sizeof(float) * (*nzbtm) * (*nxbtm + 3) * (*nybtm + 3), t2xy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice, t2xy");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2xzD, CL_FALSE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm), t2xz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice, t2xz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2yyD, CL_FALSE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * (*nybtm + 3), t2yy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice, t2yy");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2yzD, CL_FALSE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm) * (*nybtm + 3), t2yz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice, t2yz");
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[1], t2zzD, CL_FALSE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm) * (*nybtm), t2zz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyHostToDevice, t2zz");

#ifdef DISFD_EAGER_DATA_TRANSFER
	errNum = clFinish(_cl_commandQueues[1]);
	CHECK_ERROR(errNum, "H2DS");
	Stop(&h2dTimerStress[1]);
#endif

#ifdef DISFD_H2D_SYNC_KERNEL 
	for(i = 0; i < NUM_COMMAND_QUEUES; i++) {
		Start(&h2dTimerStress[i]);
	}
	for(i = 0; i < NUM_COMMAND_QUEUES; i++) {
		errNum = clFinish(_cl_commandQueues[i]);
		Stop(&h2dTimerStress[i]);
		if(errNum != CL_SUCCESS) {
			fprintf(stderr, "Vel H2D Error!\n");
		}
	}
#endif
	//printf("done!\n");

	return;
}

void cpy_d2h_stressOutputsC_opencl(float *t1xx,
		float *t1xy,
		float *t1xz,
		float *t1yy,
		float *t1yz,
		float *t1zz,
		float *t2xx,
		float *t2xy,
		float *t2xz,
		float *t2yy,
		float *t2yz,
		float *t2zz,
		int	  *nxtop,
		int	  *nytop,
		int   *nztop,
		int	  *nxbtm,
		int	  *nybtm,
		int	  *nzbtm)
{
	//printf("[OpenCL] d2h cpy for output ........");
	cl_int errNum;
	int i;
	// printf("\nnxtop=%d, nytop=%d, nztop=%d\n", *nxtop, *nytop, *nztop);
	// printf("nxbtm=%d, nybtm=%d, nzbtm=%d\n", *nxbtm, *nybtm, *nzbtm);

	// printf("t1xxD: %d\n", sizeof(float) *  sizeof(float) * (*nztop) * (*nxtop + 3) * (*nytop));

#ifdef DISFD_EAGER_DATA_TRANSFER
	Start(&d2hTimerStress[0]);
#endif
	printf("[OpenCL D2H] str 0 size: %lu\n", 
			sizeof(float) * (*nztop) * (*nxtop) * (*nytop), t1zz + 
			sizeof(float) * (*nztop + 1) * (*nxtop) * (*nytop + 3) + 
			sizeof(float) * (*nztop) * (*nxtop) * (*nytop + 3) + 
			sizeof(float) * (*nztop + 1) * (*nxtop + 3) * (*nytop) + 
			sizeof(float) * (*nztop) * (*nxtop + 3) * (*nytop + 3) + 
			sizeof(float) * (*nztop) * (*nxtop + 3) * (*nytop)
		  );
	errNum = clEnqueueReadBuffer(_cl_commandQueues[0], t1xxD, CL_FALSE, 0, sizeof(float) * (*nztop) * (*nxtop + 3) * (*nytop), t1xx, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyDeviceToHost, t1xx");
	errNum = clEnqueueReadBuffer(_cl_commandQueues[0], t1xyD, CL_FALSE, 0, sizeof(float) * (*nztop) * (*nxtop + 3) * (*nytop + 3), t1xy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyDeviceToHost, t1xy");
	errNum = clEnqueueReadBuffer(_cl_commandQueues[0], t1xzD, CL_FALSE, 0, sizeof(float) * (*nztop + 1) * (*nxtop + 3) * (*nytop), t1xz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyDeviceToHost, t1xz");
	errNum = clEnqueueReadBuffer(_cl_commandQueues[0], t1yyD, CL_FALSE, 0, sizeof(float) * (*nztop) * (*nxtop) * (*nytop + 3), t1yy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyDeviceToHost, t1yy");
	errNum = clEnqueueReadBuffer(_cl_commandQueues[0], t1yzD, CL_FALSE, 0, sizeof(float) * (*nztop + 1) * (*nxtop) * (*nytop + 3), t1yz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyDeviceToHost, t1yz");
	errNum = clEnqueueReadBuffer(_cl_commandQueues[0], t1zzD, CL_FALSE, 0, sizeof(float) * (*nztop) * (*nxtop) * (*nytop), t1zz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyDeviceToHost, t1zz");

#ifdef DISFD_EAGER_DATA_TRANSFER
	errNum = clFinish(_cl_commandQueues[0]);
	CHECK_ERROR(errNum, "H2DS");
	Stop(&d2hTimerStress[0]);
	Start(&d2hTimerStress[1]);
#endif

	printf("[OpenCL D2H] str 1 size: %lu\n", 
			sizeof(float) * (*nzbtm + 1) * (*nxbtm) * (*nybtm) + 
			sizeof(float) * (*nzbtm + 1) * (*nxbtm) * (*nybtm + 3) + 
			sizeof(float) * (*nzbtm) * (*nxbtm) * (*nybtm + 3) + 
			sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm) + 
			sizeof(float) * (*nzbtm) * (*nxbtm + 3) * (*nybtm + 3) + 
			sizeof(float) * (*nzbtm) * (*nxbtm + 3) * (*nybtm)
		  );
	errNum = clEnqueueReadBuffer(_cl_commandQueues[1], t2xxD, CL_FALSE, 0, sizeof(float) * (*nzbtm) * (*nxbtm + 3) * (*nybtm), t2xx, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyDeviceToHost, t2xx");
	errNum = clEnqueueReadBuffer(_cl_commandQueues[1], t2xyD, CL_FALSE, 0, sizeof(float) * (*nzbtm) * (*nxbtm + 3) * (*nybtm + 3), t2xy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyDeviceToHost, t2xy");
	errNum = clEnqueueReadBuffer(_cl_commandQueues[1], t2xzD, CL_FALSE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm), t2xz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyDeviceToHost, t2xz");
	errNum = clEnqueueReadBuffer(_cl_commandQueues[1], t2yyD, CL_FALSE, 0, sizeof(float) * (*nzbtm) * (*nxbtm) * (*nybtm + 3), t2yy, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyDeviceToHost, t2yy");
	errNum = clEnqueueReadBuffer(_cl_commandQueues[1], t2yzD, CL_FALSE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm) * (*nybtm + 3), t2yz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyDeviceToHost, t2yz");
	errNum = clEnqueueReadBuffer(_cl_commandQueues[1], t2zzD, CL_FALSE, 0, sizeof(float) * (*nzbtm + 1) * (*nxbtm) * (*nybtm), t2zz, 0, NULL, NULL);
	CHECK_ERROR(errNum, "outputDataCopyDeviceToHost, t2zz");

#ifdef DISFD_EAGER_DATA_TRANSFER
	errNum = clFinish(_cl_commandQueues[1]);
	CHECK_ERROR(errNum, "H2DS");
	Stop(&d2hTimerStress[1]);
#endif

	//#ifdef DISFD_H2D_SYNC_KERNEL
	for(i = 0; i < NUM_COMMAND_QUEUES; i++) {
		Start(&d2hTimerStress[i]);
	}
	for(i = 0; i < NUM_COMMAND_QUEUES; i++) {
		errNum = clFinish(_cl_commandQueues[i]);
		Stop(&d2hTimerStress[i]);
		if(errNum != CL_SUCCESS) {
			fprintf(stderr, "Vel H2D Error!\n");
		}
	}
	//#endif
	//printf("done!\n");

	return;
}

void compute_stressC_opencl(int *nxb1, int *nyb1, int *nx1p1, int *ny1p1, int *nxtop, int *nytop, int *nztop, int *mw1_pml,
		int *mw1_pml1, int *nmat, int *nll, int *lbx, int *lby, int *nd1_txy, int *nd1_txz,
		int *nd1_tyy, int *nd1_tyz, int *idmat1M, float *ca, float *drti1M, float *drth1M, float *damp1_xM, float *damp1_yM,
		float *clamdaM, float *cmuM, float *epdtM, float *qwpM, float *qwsM, float *qwt1M, float *qwt2M, float *dxh1M,
		float *dyh1M, float *dzh1M, float *dxi1M, float *dyi1M, float *dzi1M, float *t1xxM, float *t1xyM, float *t1xzM, 
		float *t1yyM, float *t1yzM, float *t1zzM, float *qt1xxM, float *qt1xyM, float *qt1xzM, float *qt1yyM, float *qt1yzM, 
		float *qt1zzM, float *t1xx_pxM, float *t1xy_pxM, float *t1xz_pxM, float *t1yy_pxM, float *qt1xx_pxM, float *qt1xy_pxM,
		float *qt1xz_pxM, float *qt1yy_pxM, float *t1xx_pyM, float *t1xy_pyM, float *t1yy_pyM, float *t1yz_pyM, float *qt1xx_pyM,
		float *qt1xy_pyM, float *qt1yy_pyM, float *qt1yz_pyM, void **v1xMp, void **v1yMp, void **v1zMp,
		int *nxb2, int *nyb2, int *nxbtm, int *nybtm, int *nzbtm, int *mw2_pml, int *mw2_pml1, int *nd2_txy, int *nd2_txz, 
		int *nd2_tyy, int *nd2_tyz, int *idmat2M, 
		float *drti2M, float *drth2M, float *damp2_xM, float *damp2_yM, float *damp2_zM, 
		float *t2xxM, float *t2xyM, float *t2xzM, float *t2yyM, float *t2yzM, float *t2zzM, 
		float *qt2xxM, float *qt2xyM, float *qt2xzM, float *qt2yyM, float *qt2yzM, float *qt2zzM, 
		float *dxh2M, float *dyh2M, float *dzh2M, float *dxi2M, float *dyi2M, float *dzi2M, 
		float *t2xx_pxM, float *t2xy_pxM, float *t2xz_pxM, float *t2yy_pxM, float *t2xx_pyM, float *t2xy_pyM,
		float *t2yy_pyM, float *t2yz_pyM, float *t2xx_pzM, float *t2xz_pzM, float *t2yz_pzM, float *t2zz_pzM,
		float *qt2xx_pxM, float *qt2xy_pxM, float *qt2xz_pxM, float *qt2yy_pxM, float *qt2xx_pyM, float *qt2xy_pyM, 
		float *qt2yy_pyM, float *qt2yz_pyM, float *qt2xx_pzM, float *qt2xz_pzM, float *qt2yz_pzM, float *qt2zz_pzM,
		void **v2xMp, void **v2yMp, void **v2zMp, int *myid)
{
	//printf("[OpenCL] stress computation:\n"); 
	static int counter = 0;
	int i;
	cl_int errNum;

	float *v1xM, *v1yM, *v1zM, *v2xM, *v2yM, *v2zM;
	int blockSizeX = 16;
	int blockSizeY = 16;
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};

	v1xM = (float *) *v1xMp;
	v1yM = (float *) *v1yMp;
	v1zM = (float *) *v1zMp;
	v2xM = (float *) *v2xMp;
	v2yM = (float *) *v2yMp;
	v2zM = (float *) *v2zMp;

	gettimeofday(&t1, NULL);
	//Start(&h2dTimerStress);
#ifndef DISFD_GPU_MARSHALING
	cpy_h2d_stressInputsC_opencl(v1xM, v1yM, v1zM, v2xM, v2yM, v2zM, nxtop, nytop, nztop, nxbtm, nybtm, nzbtm);
	cpy_h2d_stressOutputsC_opencl(t1xxM, t1xyM, t1xzM, t1yyM, t1yzM, t1zzM, t2xxM, t2xyM, t2xzM,t2yyM, t2yzM, t2zzM, nxtop, nytop, nztop, nxbtm, nybtm, nzbtm);
#endif
	//Stop(&h2dTimerStress);
	gettimeofday(&t2, NULL);
	tmpTime = 1000.0 * (t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec) / 1000.0;
	totalTimeH2DS += tmpTime;

	gettimeofday(&t1, NULL);
	for(i = 0; i < NUM_COMMAND_QUEUES; i++) {
		Start(&kernelTimerStress[i]);
		printf("DISFD Q%d Stress Set Kernel Args\n", i);
		if(counter < 1) {
#ifdef SNUCL_PERF_MODEL_OPTIMIZATION
			clSetCommandQueueProperty(_cl_commandQueues[i], 
#ifdef SNUCL_PERF_MODEL_OPTIMIZATION
					CL_QUEUE_AUTO_DEVICE_SELECTION | 
					CL_QUEUE_ITERATIVE | 
					CL_QUEUE_IO_INTENSIVE | 
					//CL_QUEUE_COMPUTE_INTENSIVE |
#endif
					CL_QUEUE_PROFILING_ENABLE, 
					true,
					NULL);
#endif
		}
	}
#ifdef DISFD_USE_ROW_MAJOR_DATA
	int gridSizeX1 = (nd1_tyy[17] - nd1_tyy[12])/blockSizeX + 1;
	int gridSizeY1 = (nd1_tyy[9] - nd1_tyy[8])/blockSizeY + 1;
#else
	int gridSizeX1 = (nd1_tyy[3] - nd1_tyy[2])/blockSizeX + 1;
	int gridSizeY1 = (nd1_tyy[9] - nd1_tyy[8])/blockSizeY + 1;
#endif
	size_t dimGrid1[3] = {gridSizeX1, gridSizeY1, 1};

	errNum = clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 0, sizeof(int), nxb1);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 1, sizeof(int), nyb1);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 2, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 3, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 4, sizeof(cl_mem), &nd1_tyyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 5, sizeof(cl_mem), &idmat1D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 6, sizeof(float), ca);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 7, sizeof(cl_mem), &clamdaD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 8, sizeof(cl_mem), &cmuD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 9, sizeof(cl_mem), &epdtD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 10, sizeof(cl_mem), &qwpD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 11, sizeof(cl_mem), &qwsD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 12, sizeof(cl_mem), &qwt1D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 13, sizeof(cl_mem), &qwt2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 14, sizeof(cl_mem), &dxh1D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 15, sizeof(cl_mem), &dyh1D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 16, sizeof(cl_mem), &dxi1D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 17, sizeof(cl_mem), &dyi1D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 18, sizeof(cl_mem), &dzi1D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 19, sizeof(cl_mem), &t1xxD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 20, sizeof(cl_mem), &t1xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 21, sizeof(cl_mem), &t1yyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 22, sizeof(cl_mem), &t1zzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 23, sizeof(cl_mem), &qt1xxD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 24, sizeof(cl_mem), &qt1xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 25, sizeof(cl_mem), &qt1yyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 26, sizeof(cl_mem), &qt1zzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 27, sizeof(cl_mem), &v1xD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 28, sizeof(cl_mem), &v1yD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_IC, 29, sizeof(cl_mem), &v1zD);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_norm_xy_IC arguments!\n");
	}
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid1[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid1[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid1[2]*localWorkSize[2];
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[0], _cl_kernel_stress_norm_xy_IC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_norm_xy_IC for execution!\n");
	}

#ifdef DISFD_USE_ROW_MAJOR_DATA
	int gridSizeX2 = (nd1_tyz[17] - nd1_tyz[12])/blockSizeX + 1;
	int gridSizeY2 = (nd1_tyz[9] - nd1_tyz[8])/blockSizeY + 1;
#else
	int gridSizeX2 = (nd1_tyz[3] - nd1_tyz[2])/blockSizeX + 1;
	int gridSizeY2 = (nd1_tyz[9] - nd1_tyz[8])/blockSizeY + 1;
#endif
	size_t dimGrid2[3] = {gridSizeX2, gridSizeY2, 1};

	errNum = clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 0, sizeof(int), nxb1);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 1, sizeof(int), nyb1);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 2, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 3, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 4, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 5, sizeof(cl_mem), &nd1_tyzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 6, sizeof(cl_mem), &idmat1D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 7, sizeof(float), ca);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 8, sizeof(cl_mem), &cmuD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 9, sizeof(cl_mem), &epdtD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 10, sizeof(cl_mem), &qwsD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 11, sizeof(cl_mem), &qwt1D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 12, sizeof(cl_mem), &qwt2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 13, sizeof(cl_mem), &dxi1D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 14, sizeof(cl_mem), &dyi1D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 15, sizeof(cl_mem), &dzh1D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 16, sizeof(cl_mem), &v1xD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 17, sizeof(cl_mem), &v1yD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 18, sizeof(cl_mem), &v1zD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 19, sizeof(cl_mem), &t1xzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 20, sizeof(cl_mem), &t1yzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 21, sizeof(cl_mem), &qt1xzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IC, 22, sizeof(cl_mem), &qt1yzD);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_xz_yz_IC arguments!\n");
	}
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid2[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid2[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid2[2]*localWorkSize[2];
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[0], _cl_kernel_stress_xz_yz_IC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_xz_yz_IC for execution!\n");
	}

	int gridSizeX3Temp1 = ((*ny1p1) + 1)/blockSizeX + 1;
	int gridSizeX3Temp2 = ((*nytop) - 1)/blockSizeX + 1;
	int gridSizeY3Temp1 = ((*nxtop) - 1)/blockSizeY + 1;
	int gridSizeY3Temp2 = ((*nx1p1) + 1)/blockSizeY + 1;
	int gridSizeX3 = (gridSizeX3Temp1 > gridSizeX3Temp2) ? gridSizeX3Temp1 : gridSizeX3Temp2;
	int gridSizeY3 = (gridSizeY3Temp1 > gridSizeY3Temp2) ? gridSizeY3Temp1 : gridSizeY3Temp2;
	size_t dimGrid3[3] = {gridSizeX3, gridSizeY3, 1};

	errNum = clSetKernelArg(_cl_kernel_stress_resetVars, 0, sizeof(int), ny1p1);
	errNum |= clSetKernelArg(_cl_kernel_stress_resetVars, 1, sizeof(int), nx1p1);
	errNum |= clSetKernelArg(_cl_kernel_stress_resetVars, 2, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_stress_resetVars, 3, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_stress_resetVars, 4, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_stress_resetVars, 5, sizeof(cl_mem), &t1xzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_resetVars, 6, sizeof(cl_mem), &t1yzD);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_resetVars arguments!\n");
	}
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid3[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid3[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid3[2]*localWorkSize[2];
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[0], _cl_kernel_stress_resetVars, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_resetVars for execution!\n");
	}

	if (lbx[1] >= lbx[0])
	{
#ifdef DISFD_USE_ROW_MAJOR_DATA
		int gridSizeX4 = (nd1_tyy[17] - nd1_tyy[12])/blockSizeX + 1;
		int gridSizeY4 = (nd1_tyy[5] - nd1_tyy[0])/blockSizeY + 1;
#else
		int gridSizeX4 = (nd1_tyy[5] - nd1_tyy[0])/blockSizeX + 1;
		int gridSizeY4 = (lbx[1] - lbx[0])/blockSizeY + 1;
#endif
		size_t dimGrid4[3] = {gridSizeX4, gridSizeY4, 1};

		errNum = clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 0, sizeof(int), nxb1);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 1, sizeof(int), nyb1);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 2, sizeof(int), nxtop);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 3, sizeof(int), nytop);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 4, sizeof(int), nztop);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 5, sizeof(int), mw1_pml);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 6, sizeof(int), mw1_pml1);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 7, sizeof(int), &lbx[0]);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 8, sizeof(int), &lbx[1]);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 9, sizeof(cl_mem), &nd1_tyyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 10, sizeof(cl_mem), &idmat1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 11, sizeof(float), ca);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 12, sizeof(cl_mem), &drti1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 13, sizeof(cl_mem), &damp1_xD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 14, sizeof(cl_mem), &clamdaD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 15, sizeof(cl_mem), &cmuD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 16, sizeof(cl_mem), &epdtD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 17, sizeof(cl_mem), &qwpD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 18, sizeof(cl_mem), &qwsD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 19, sizeof(cl_mem), &qwt1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 20, sizeof(cl_mem), &qwt2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 21, sizeof(cl_mem), &dzi1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 22, sizeof(cl_mem), &dxh1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 23, sizeof(cl_mem), &dyh1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 24, sizeof(cl_mem), &v1xD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 25, sizeof(cl_mem), &v1yD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 26, sizeof(cl_mem), &v1zD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 27, sizeof(cl_mem), &t1xxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 28, sizeof(cl_mem), &t1yyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 29, sizeof(cl_mem), &t1zzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 30, sizeof(cl_mem), &t1xx_pxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 31, sizeof(cl_mem), &t1yy_pxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 32, sizeof(cl_mem), &qt1xxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 33, sizeof(cl_mem), &qt1yyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 34, sizeof(cl_mem), &qt1zzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 35, sizeof(cl_mem), &qt1xx_pxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IC, 36, sizeof(cl_mem), &qt1yy_pxD);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: setting kernel _cl_kernel_stress_norm_PmlX_IC arguments!\n");
		}
		localWorkSize[0] = dimBlock[0];
		localWorkSize[1] = dimBlock[1];
		localWorkSize[2] = dimBlock[2];
		globalWorkSize[0] = dimGrid4[0]*localWorkSize[0];
		globalWorkSize[1] = dimGrid4[1]*localWorkSize[1];
		globalWorkSize[2] = dimGrid4[2]*localWorkSize[2];
		errNum = clEnqueueNDRangeKernel(_cl_commandQueues[0], _cl_kernel_stress_norm_PmlX_IC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_norm_PmlX_IC for execution!\n");
		}
	}
	if (lby[1] >= lby[0])
	{
#ifdef DISFD_USE_ROW_MAJOR_DATA
		int gridSizeX5 = (nd1_tyy[17] - nd1_tyy[12])/blockSizeX + 1;
		int gridSizeY5 = (nd1_tyy[11] - nd1_tyy[6])/blockSizeY + 1;
#else
		int gridSizeX5 = (nd1_tyy[11] - nd1_tyy[6])/blockSizeX + 1;
		int gridSizeY5 = (lby[1] - lby[0])/blockSizeY + 1;
#endif
		size_t dimGrid5[3] = {gridSizeX5, gridSizeY5, 1};

		errNum = clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 0, sizeof(int), nxb1);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 1, sizeof(int), nyb1);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 2, sizeof(int), mw1_pml1);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 3, sizeof(int), nxtop);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 4, sizeof(int), nztop);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 5, sizeof(int), &lby[0]);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 6, sizeof(int), &lby[1]);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 7, sizeof(cl_mem), &nd1_tyyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 8, sizeof(cl_mem), &idmat1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 9, sizeof(float), ca);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 10, sizeof(cl_mem), &drti1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 11, sizeof(cl_mem), &damp1_yD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 12, sizeof(cl_mem), &clamdaD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 13, sizeof(cl_mem), &cmuD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 14, sizeof(cl_mem), &epdtD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 15, sizeof(cl_mem), &qwpD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 16, sizeof(cl_mem), &qwsD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 17, sizeof(cl_mem), &qwt1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 18, sizeof(cl_mem), &qwt2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 19, sizeof(cl_mem), &dxh1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 20, sizeof(cl_mem), &dyh1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 21, sizeof(cl_mem), &dzi1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 22, sizeof(cl_mem), &t1xxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 23, sizeof(cl_mem), &t1yyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 24, sizeof(cl_mem), &t1zzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 25, sizeof(cl_mem), &qt1xxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 26, sizeof(cl_mem), &qt1yyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 27, sizeof(cl_mem), &qt1zzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 28, sizeof(cl_mem), &t1xx_pyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 29, sizeof(cl_mem), &t1yy_pyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 30, sizeof(cl_mem), &qt1xx_pyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 31, sizeof(cl_mem), &qt1yy_pyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 32, sizeof(cl_mem), &v1xD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 33, sizeof(cl_mem), &v1yD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_IC, 34, sizeof(cl_mem), &v1zD);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: setting kernel _cl_kernel_stress_norm_PmlY_IC arguments!\n");
		}
		localWorkSize[0] = dimBlock[0];
		localWorkSize[1] = dimBlock[1];
		localWorkSize[2] = dimBlock[2];
		globalWorkSize[0] = dimGrid5[0]*localWorkSize[0];
		globalWorkSize[1] = dimGrid5[1]*localWorkSize[1];
		globalWorkSize[2] = dimGrid5[2]*localWorkSize[2];
		errNum = clEnqueueNDRangeKernel(_cl_commandQueues[0], _cl_kernel_stress_norm_PmlY_IC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_norm_PmlY_IC for execution!\n");
		}
	}

	if (lbx[1] >= lbx[0]) 
	{
#ifdef DISFD_USE_ROW_MAJOR_DATA
		int gridSizeX6 = (nd1_txy[17] - nd1_txy[12])/blockSizeX + 1;
		int gridSizeY6 = (nd1_txy[5] - nd1_txy[0])/blockSizeY + 1;
#else
		int gridSizeX6 = (nd1_txy[5] - nd1_txy[0])/blockSizeX + 1;
		int gridSizeY6 = (lbx[1] - lbx[0])/blockSizeY + 1;
#endif
		size_t dimGrid6[3] = {gridSizeX6, gridSizeY6, 1};

		errNum = clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 0, sizeof(int), nxb1);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 1, sizeof(int), nyb1);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 2, sizeof(int), mw1_pml);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 3, sizeof(int), mw1_pml1);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 4, sizeof(int), nxtop);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 5, sizeof(int), nytop);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 6, sizeof(int), nztop);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 7, sizeof(int), &lbx[0]);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 8, sizeof(int), &lbx[1]);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 9, sizeof(cl_mem), &nd1_txyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 10, sizeof(cl_mem), &idmat1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 11, sizeof(float), ca);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 12, sizeof(cl_mem), &drth1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 13, sizeof(cl_mem), &damp1_xD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 14, sizeof(cl_mem), &cmuD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 15, sizeof(cl_mem), &epdtD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 16, sizeof(cl_mem), &qwsD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 17, sizeof(cl_mem), &qwt1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 18, sizeof(cl_mem), &qwt2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 19, sizeof(cl_mem), &dxi1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 20, sizeof(cl_mem), &dyi1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 21, sizeof(cl_mem), &t1xyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 22, sizeof(cl_mem), &qt1xyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 23, sizeof(cl_mem), &t1xy_pxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 24, sizeof(cl_mem), &qt1xy_pxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 25, sizeof(cl_mem), &v1xD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IC, 26, sizeof(cl_mem), &v1yD);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: setting kernel _cl_kernel_stress_xy_PmlX_IC arguments!\n");
		}
		localWorkSize[0] = dimBlock[0];
		localWorkSize[1] = dimBlock[1];
		localWorkSize[2] = dimBlock[2];
		globalWorkSize[0] = dimGrid6[0]*localWorkSize[0];
		globalWorkSize[1] = dimGrid6[1]*localWorkSize[1];
		globalWorkSize[2] = dimGrid6[2]*localWorkSize[2];
		errNum = clEnqueueNDRangeKernel(_cl_commandQueues[0], _cl_kernel_stress_xy_PmlX_IC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_xy_PmlX_IC for execution!\n");
		}
	}

	if (lby[1] >= lby[0])
	{
#ifdef DISFD_USE_ROW_MAJOR_DATA
		int gridSizeX7 = (nd1_txy[17] - nd1_txy[12])/blockSizeX + 1;
		int gridSizeY7 = (nd1_txy[11] - nd1_txy[6])/blockSizeY + 1;
#else
		int gridSizeX7 = (nd1_txy[11] - nd1_txy[6])/blockSizeX + 1;
		int gridSizeY7 = (lby[1] - lby[0])/blockSizeY + 1;
#endif
		size_t dimGrid7[3] = {gridSizeX7, gridSizeY7, 1};

		errNum = clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 0, sizeof(int), nxb1);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 1, sizeof(int), nyb1);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 2, sizeof(int), mw1_pml1);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 3, sizeof(int), nxtop);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 4, sizeof(int), nztop);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 5, sizeof(int), &lby[0]);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 6, sizeof(int), &lby[1]);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 7, sizeof(cl_mem), &nd1_txyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 8, sizeof(cl_mem), &idmat1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 9, sizeof(float), ca);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 10, sizeof(cl_mem), &drth1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 11, sizeof(cl_mem), &damp1_yD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 12, sizeof(cl_mem), &cmuD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 13, sizeof(cl_mem), &epdtD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 14, sizeof(cl_mem), &qwsD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 15, sizeof(cl_mem), &qwt1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 16, sizeof(cl_mem), &qwt2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 17, sizeof(cl_mem), &dxi1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 18, sizeof(cl_mem), &dyi1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 19, sizeof(cl_mem), &t1xyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 20, sizeof(cl_mem), &qt1xyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 21, sizeof(cl_mem), &t1xy_pyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 22, sizeof(cl_mem), &qt1xy_pyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 23, sizeof(cl_mem), &v1xD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IC, 24, sizeof(cl_mem), &v1yD);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: setting kernel _cl_kernel_stress_xy_PmlY_IC arguments!\n");
		}
		localWorkSize[0] = dimBlock[0];
		localWorkSize[1] = dimBlock[1];
		localWorkSize[2] = dimBlock[2];
		globalWorkSize[0] = dimGrid7[0]*localWorkSize[0];
		globalWorkSize[1] = dimGrid7[1]*localWorkSize[1];
		globalWorkSize[2] = dimGrid7[2]*localWorkSize[2];
		errNum = clEnqueueNDRangeKernel(_cl_commandQueues[0], _cl_kernel_stress_xy_PmlY_IC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_xy_PmlY_IC for execution!\n");
		}
	}

	if (lbx[1] >= lbx[0])
	{
#ifdef DISFD_USE_ROW_MAJOR_DATA
		int gridSizeX8 = (nd1_txz[17] - nd1_txz[12])/blockSizeX + 1;
		int gridSizeY8 = (nd1_txz[5] - nd1_txz[0])/blockSizeX + 1;
#else
		int gridSizeX8 = (nd1_txz[5] - nd1_txz[0])/blockSizeX + 1;
		int gridSizeY8 = (lbx[1] - lbx[0])/blockSizeY + 1;
#endif
		size_t dimGrid8[3] = {gridSizeX8, gridSizeY8, 1};

		errNum = clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 0, sizeof(int), nxb1);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 1, sizeof(int), nyb1);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 2, sizeof(int), nxtop);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 3, sizeof(int), nytop);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 4, sizeof(int), nztop);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 5, sizeof(int), mw1_pml);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 6, sizeof(int), mw1_pml1);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 7, sizeof(int), &lbx[0]);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 8, sizeof(int), &lbx[1]);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 9, sizeof(cl_mem), &nd1_txzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 10, sizeof(cl_mem), &idmat1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 11, sizeof(float), ca);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 12, sizeof(cl_mem), &drth1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 13, sizeof(cl_mem), &damp1_xD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 14, sizeof(cl_mem), &cmuD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 15, sizeof(cl_mem), &epdtD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 16, sizeof(cl_mem), &qwsD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 17, sizeof(cl_mem), &qwt1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 18, sizeof(cl_mem), &qwt2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 19, sizeof(cl_mem), &dxi1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 20, sizeof(cl_mem), &dzh1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 21, sizeof(cl_mem), &t1xzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 22, sizeof(cl_mem), &qt1xzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 23, sizeof(cl_mem), &t1xz_pxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 24, sizeof(cl_mem), &qt1xz_pxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 25, sizeof(cl_mem), &v1xD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IC, 26, sizeof(cl_mem), &v1zD);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: setting kernel _cl_kernel_stress_xz_PmlX_IC arguments!\n");
		}
		localWorkSize[0] = dimBlock[0];
		localWorkSize[1] = dimBlock[1];
		localWorkSize[2] = dimBlock[2];
		globalWorkSize[0] = dimGrid8[0]*localWorkSize[0];
		globalWorkSize[1] = dimGrid8[1]*localWorkSize[1];
		globalWorkSize[2] = dimGrid8[2]*localWorkSize[2];
		errNum = clEnqueueNDRangeKernel(_cl_commandQueues[0], _cl_kernel_stress_xz_PmlX_IC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_xz_PmlX_IC for execution!\n");
		}
	}

	if (lby[1] >= lby[0])
	{
#ifdef DISFD_USE_ROW_MAJOR_DATA
		int gridSizeX9 = (nd1_txz[17] - nd1_txz[12])/blockSizeX + 1;
		int gridSizeY9 = (nd1_txz[9] - nd1_txz[8])/blockSizeY + 1;
#else
		int gridSizeX9 = (nd1_txz[9] - nd1_txz[8])/blockSizeX + 1;
		int gridSizeY9 = (lby[1] - lby[0])/blockSizeY + 1;
#endif
		size_t dimGrid9[3] = {gridSizeX9, gridSizeY9, 1};

		errNum = clSetKernelArg(_cl_kernel_stress_xz_PmlY_IC, 0, sizeof(int), nxb1);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IC, 1, sizeof(int), nyb1);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IC, 2, sizeof(int), nxtop);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IC, 3, sizeof(int), nztop);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IC, 4, sizeof(int), &lby[0]);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IC, 5, sizeof(int), &lby[1]);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IC, 6, sizeof(cl_mem), &nd1_txzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IC, 7, sizeof(cl_mem), &idmat1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IC, 8, sizeof(float), ca);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IC, 9, sizeof(cl_mem), &cmuD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IC, 10, sizeof(cl_mem), &epdtD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IC, 11, sizeof(cl_mem), &qwsD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IC, 12, sizeof(cl_mem), &qwt1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IC, 13, sizeof(cl_mem), &qwt2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IC, 14, sizeof(cl_mem), &dxi1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IC, 15, sizeof(cl_mem), &dzh1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IC, 16, sizeof(cl_mem), &t1xzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IC, 17, sizeof(cl_mem), &qt1xzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IC, 18, sizeof(cl_mem), &v1xD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IC, 19, sizeof(cl_mem), &v1zD);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: setting kernel _cl_kernel_stress_xz_PmlY_IC arguments!\n");
		}
		localWorkSize[0] = dimBlock[0];
		localWorkSize[1] = dimBlock[1];
		localWorkSize[2] = dimBlock[2];
		globalWorkSize[0] = dimGrid9[0]*localWorkSize[0];
		globalWorkSize[1] = dimGrid9[1]*localWorkSize[1];
		globalWorkSize[2] = dimGrid9[2]*localWorkSize[2];
		errNum = clEnqueueNDRangeKernel(_cl_commandQueues[0], _cl_kernel_stress_xz_PmlY_IC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_xz_PmlY_IC for execution!\n");
		}
	}

	if (lbx[1] >= lbx[0])
	{
#ifdef DISFD_USE_ROW_MAJOR_DATA
		int gridSizeX10 = (nd1_tyz[17] - nd1_tyz[12])/blockSizeX + 1;
		int gridSizeY10 = (nd1_tyz[3] - nd1_tyz[2])/blockSizeY + 1;
#else
		int gridSizeX10 = (nd1_tyz[3] - nd1_tyz[2])/blockSizeX + 1;
		int gridSizeY10 = (lbx[1] - lbx[0])/blockSizeY + 1;
#endif
		size_t dimGrid10[3] = {gridSizeX10, gridSizeY10, 1};

		errNum = clSetKernelArg(_cl_kernel_stress_yz_PmlX_IC, 0, sizeof(int), nxb1);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IC, 1, sizeof(int), nyb1);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IC, 2, sizeof(int), nztop);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IC, 3, sizeof(int), nxtop);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IC, 4, sizeof(int), &lbx[0]);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IC, 5, sizeof(int), &lbx[1]);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IC, 6, sizeof(cl_mem), &nd1_tyzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IC, 7, sizeof(cl_mem), &idmat1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IC, 8, sizeof(float), ca);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IC, 9, sizeof(cl_mem), &cmuD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IC, 10, sizeof(cl_mem), &epdtD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IC, 11, sizeof(cl_mem), &qwsD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IC, 12, sizeof(cl_mem), &qwt1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IC, 13, sizeof(cl_mem), &qwt2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IC, 14, sizeof(cl_mem), &dyi1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IC, 15, sizeof(cl_mem), &dzh1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IC, 16, sizeof(cl_mem), &t1yzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IC, 17, sizeof(cl_mem), &qt1yzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IC, 18, sizeof(cl_mem), &v1yD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IC, 19, sizeof(cl_mem), &v1zD);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: setting kernel _cl_kernel_stress_yz_PmlX_IC arguments!\n");
		}
		localWorkSize[0] = dimBlock[0];
		localWorkSize[1] = dimBlock[1];
		localWorkSize[2] = dimBlock[2];
		globalWorkSize[0] = dimGrid10[0]*localWorkSize[0];
		globalWorkSize[1] = dimGrid10[1]*localWorkSize[1];
		globalWorkSize[2] = dimGrid10[2]*localWorkSize[2];
		errNum = clEnqueueNDRangeKernel(_cl_commandQueues[0], _cl_kernel_stress_yz_PmlX_IC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_yz_PmlX_IC for execution!\n");
		}
	}

	if (lby[1] >= lby[0])
	{
#ifdef DISFD_USE_ROW_MAJOR_DATA
		int gridSizeX11 = (nd1_tyz[17] - nd1_tyz[12])/blockSizeX + 1;
		int gridSizeY11 = (nd1_tyz[11] - nd1_tyz[6])/blockSizeY + 1;
#else
		int gridSizeX11 = (nd1_tyz[11] - nd1_tyz[6])/blockSizeX + 1;
		int gridSizeY11 = (lby[1] - lby[0])/blockSizeY + 1;
#endif
		size_t dimGrid11[3] = {gridSizeX11, gridSizeY11, 1};

		errNum = clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 0, sizeof(int), nxb1);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 1, sizeof(int), nyb1);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 2, sizeof(int), mw1_pml1);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 3, sizeof(int), nxtop);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 4, sizeof(int), nztop);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 5, sizeof(int), &lby[0]);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 6, sizeof(int), &lby[1]);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 7, sizeof(cl_mem), &nd1_tyzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 8, sizeof(cl_mem), &idmat1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 9, sizeof(float), ca);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 10, sizeof(cl_mem), &drth1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 11, sizeof(cl_mem), &damp1_yD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 12, sizeof(cl_mem), &cmuD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 13, sizeof(cl_mem), &epdtD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 14, sizeof(cl_mem), &qwsD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 15, sizeof(cl_mem), &qwt1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 16, sizeof(cl_mem), &qwt2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 17, sizeof(cl_mem), &dyi1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 18, sizeof(cl_mem), &dzh1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 19, sizeof(cl_mem), &t1yzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 20, sizeof(cl_mem), &qt1yzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 21, sizeof(cl_mem), &t1yz_pyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 22, sizeof(cl_mem), &qt1yz_pyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 23, sizeof(cl_mem), &v1yD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IC, 24, sizeof(cl_mem), &v1zD);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: setting kernel _cl_kernel_stress_yz_PmlY_IC arguments!\n");
		}
		localWorkSize[0] = dimBlock[0];
		localWorkSize[1] = dimBlock[1];
		localWorkSize[2] = dimBlock[2];
		globalWorkSize[0] = dimGrid11[0]*localWorkSize[0];
		globalWorkSize[1] = dimGrid11[1]*localWorkSize[1];
		globalWorkSize[2] = dimGrid11[2]*localWorkSize[2];
		errNum = clEnqueueNDRangeKernel(_cl_commandQueues[0], _cl_kernel_stress_yz_PmlY_IC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_yz_PmlY_IC for execution!\n");
		}
	}

#ifdef DISFD_USE_ROW_MAJOR_DATA
	int gridSizeX12 = (nd2_tyy[15] - nd2_tyy[12])/blockSizeX + 1;
	int gridSizeY12 = (nd2_tyy[9] - nd2_tyy[8])/blockSizeY + 1;
#else
	int gridSizeX12 = (nd2_tyy[3] - nd2_tyy[2])/blockSizeX + 1;
	int gridSizeY12 = (nd2_tyy[9] - nd2_tyy[8])/blockSizeY + 1;
#endif
	size_t dimGrid12[3] = {gridSizeX12, gridSizeY12, 1};

	errNum = clSetKernelArg(_cl_kernel_stress_norm_xy_II, 0, sizeof(int), nxb2);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 1, sizeof(int), nyb2);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 2, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 3, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 4, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 5, sizeof(cl_mem), &nd2_tyyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 6, sizeof(cl_mem), &idmat2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 7, sizeof(cl_mem), &clamdaD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 8, sizeof(cl_mem), &cmuD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 9, sizeof(cl_mem), &epdtD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 10, sizeof(cl_mem), &qwpD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 11, sizeof(cl_mem), &qwsD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 12, sizeof(cl_mem), &qwt1D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 13, sizeof(cl_mem), &qwt2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 14, sizeof(cl_mem), &t2xxD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 15, sizeof(cl_mem), &t2xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 16, sizeof(cl_mem), &t2yyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 17, sizeof(cl_mem), &t2zzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 18, sizeof(cl_mem), &qt2xxD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 19, sizeof(cl_mem), &qt2xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 20, sizeof(cl_mem), &qt2yyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 21, sizeof(cl_mem), &qt2zzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 22, sizeof(cl_mem), &dxh2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 23, sizeof(cl_mem), &dyh2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 24, sizeof(cl_mem), &dxi2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 25, sizeof(cl_mem), &dyi2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 26, sizeof(cl_mem), &dzi2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 27, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 28, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_xy_II, 29, sizeof(cl_mem), &v2zD);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_norm_xy_II arguments!\n");
	}
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid12[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid12[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid12[2]*localWorkSize[2];
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[1], _cl_kernel_stress_norm_xy_II, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_norm_xy_II for execution!\n");
	}

#ifdef DISFD_USE_ROW_MAJOR_DATA
	int gridSizeX13 = (nd2_tyz[15] - nd2_tyz[12])/blockSizeX + 1;
	int gridSizeY13 = (nd2_tyz[9] - nd2_tyz[8])/blockSizeY + 1;
#else
	int gridSizeX13 = (nd2_tyz[3] - nd2_tyz[2])/blockSizeX + 1;
	int gridSizeY13 = (nd2_tyz[9] - nd2_tyz[8])/blockSizeY + 1;
#endif
	size_t dimGrid13[3] = {gridSizeX13, gridSizeY13, 1};

	errNum = clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 0, sizeof(int), nxb2);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 1, sizeof(int), nyb2);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 2, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 3, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 4, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 5, sizeof(cl_mem), &nd2_tyzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 6, sizeof(cl_mem), &idmat2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 7, sizeof(cl_mem), &cmuD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 8, sizeof(cl_mem), &epdtD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 9, sizeof(cl_mem), &qwsD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 10, sizeof(cl_mem), &qwt1D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 11, sizeof(cl_mem), &qwt2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 12, sizeof(cl_mem), &dxi2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 13, sizeof(cl_mem), &dyi2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 14, sizeof(cl_mem), &dzh2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 15, sizeof(cl_mem), &t2xzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 16, sizeof(cl_mem), &t2yzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 17, sizeof(cl_mem), &qt2xzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 18, sizeof(cl_mem), &qt2yzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 19, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 20, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_yz_IIC, 21, sizeof(cl_mem), &v2zD);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_xz_yz_IIC arguments!\n");
	}
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid13[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid13[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid13[2]*localWorkSize[2];
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[1], _cl_kernel_stress_xz_yz_IIC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_xz_yz_IIC for execution!\n");
	}

	if (lbx[1] >= lbx[0])
	{
#ifdef DISFD_USE_ROW_MAJOR_DATA
		int gridSizeX14 = (nd2_tyy[17] - nd2_tyy[12])/blockSizeX + 1;
		int gridSizeY14 = (nd2_tyy[5] - nd2_tyy[0])/blockSizeY + 1;
#else
		int gridSizeX14 = (nd2_tyy[5] - nd2_tyy[0])/blockSizeX + 1;
		int gridSizeY14 = (lbx[1] - lbx[0])/blockSizeY + 1;
#endif
		size_t dimGrid14[3] = {gridSizeX14, gridSizeY14, 1};

		errNum = clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 0, sizeof(int), nxb2);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 1, sizeof(int), nyb2);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 2, sizeof(int), mw2_pml);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 3, sizeof(int), mw2_pml1);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 4, sizeof(int), nztop);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 5, sizeof(int), nxbtm);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 6, sizeof(int), nybtm);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 7, sizeof(int), nzbtm);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 8, sizeof(int), &lbx[0]);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 9, sizeof(int), &lbx[1]);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 10, sizeof(cl_mem), &nd2_tyyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 11, sizeof(cl_mem), &idmat2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 12, sizeof(float), ca);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 13, sizeof(cl_mem), &drti2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 14, sizeof(cl_mem), &damp2_xD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 15, sizeof(cl_mem), &clamdaD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 16, sizeof(cl_mem), &cmuD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 17, sizeof(cl_mem), &epdtD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 18, sizeof(cl_mem), &qwpD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 19, sizeof(cl_mem), &qwsD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 20, sizeof(cl_mem), &qwt1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 21, sizeof(cl_mem), &qwt2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 22, sizeof(cl_mem), &dxh2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 23, sizeof(cl_mem), &dyh2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 24, sizeof(cl_mem), &dzi2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 25, sizeof(cl_mem), &t2xxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 26, sizeof(cl_mem), &t2yyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 27, sizeof(cl_mem), &t2zzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 28, sizeof(cl_mem), &qt2xxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 29, sizeof(cl_mem), &qt2yyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 30, sizeof(cl_mem), &qt2zzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 31, sizeof(cl_mem), &t2xx_pxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 32, sizeof(cl_mem), &t2yy_pxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 33, sizeof(cl_mem), &qt2xx_pxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 34, sizeof(cl_mem), &qt2yy_pxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 35, sizeof(cl_mem), &v2xD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 36, sizeof(cl_mem), &v2yD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlX_IIC, 37, sizeof(cl_mem), &v2zD);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: setting kernel _cl_kernel_stress_norm_PmlX_IIC arguments!\n");
		}
		localWorkSize[0] = dimBlock[0];
		localWorkSize[1] = dimBlock[1];
		localWorkSize[2] = dimBlock[2];
		globalWorkSize[0] = dimGrid14[0]*localWorkSize[0];
		globalWorkSize[1] = dimGrid14[1]*localWorkSize[1];
		globalWorkSize[2] = dimGrid14[2]*localWorkSize[2];
		errNum = clEnqueueNDRangeKernel(_cl_commandQueues[1], _cl_kernel_stress_norm_PmlX_IIC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_norm_PmlX_IIC for execution!\n");
		}
	}

	if (lby[1] >= lby[0])
	{
#ifdef DISFD_USE_ROW_MAJOR_DATA
		int gridSizeX15 = (nd2_tyy[17] - nd2_tyy[12])/blockSizeX + 1;
		int gridSizeY15 = (nd2_tyy[11] - nd2_tyy[6])/blockSizeY + 1;
#else
		int gridSizeX15 = (nd2_tyy[11] - nd2_tyy[6])/blockSizeX + 1;
		int gridSizeY15 = (lby[1] - lby[0])/blockSizeY + 1;
#endif
		size_t dimGrid15[3] = {gridSizeX15, gridSizeY15, 1};
		errNum = clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 0, sizeof(int), nxb2);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 1, sizeof(int), nyb2);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 2, sizeof(int), nztop);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 3, sizeof(int), nxbtm);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 4, sizeof(int), nzbtm);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 5, sizeof(int), mw2_pml1);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 6, sizeof(int), &lby[0]);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 7, sizeof(int), &lby[1]);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 8, sizeof(cl_mem), &nd2_tyyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 9, sizeof(cl_mem), &idmat2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 10, sizeof(float), ca);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 11, sizeof(cl_mem), &drti2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 12, sizeof(cl_mem), &damp2_yD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 13, sizeof(cl_mem), &clamdaD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 14, sizeof(cl_mem), &cmuD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 15, sizeof(cl_mem), &epdtD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 16, sizeof(cl_mem), &qwpD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 17, sizeof(cl_mem), &qwsD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 18, sizeof(cl_mem), &qwt1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 19, sizeof(cl_mem), &qwt2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 20, sizeof(cl_mem), &dxh2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 21, sizeof(cl_mem), &dyh2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 22, sizeof(cl_mem), &dzi2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 23, sizeof(cl_mem), &t2xxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 24, sizeof(cl_mem), &t2yyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 25, sizeof(cl_mem), &t2zzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 26, sizeof(cl_mem), &qt2xxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 27, sizeof(cl_mem), &qt2yyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 28, sizeof(cl_mem), &qt2zzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 29, sizeof(cl_mem), &t2xx_pyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 30, sizeof(cl_mem), &t2yy_pyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 31, sizeof(cl_mem), &qt2xx_pyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 32, sizeof(cl_mem), &qt2yy_pyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 33, sizeof(cl_mem), &v2xD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 34, sizeof(cl_mem), &v2yD);
		errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlY_II, 35, sizeof(cl_mem), &v2zD);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: setting kernel _cl_kernel_stress_norm_PmlY_II arguments!\n");
		}
		localWorkSize[0] = dimBlock[0];
		localWorkSize[1] = dimBlock[1];
		localWorkSize[2] = dimBlock[2];
		globalWorkSize[0] = dimGrid15[0]*localWorkSize[0];
		globalWorkSize[1] = dimGrid15[1]*localWorkSize[1];
		globalWorkSize[2] = dimGrid15[2]*localWorkSize[2];
		errNum = clEnqueueNDRangeKernel(_cl_commandQueues[1], _cl_kernel_stress_norm_PmlY_II, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_norm_PmlY_II for execution!\n");
		}
	}

#ifdef DISFD_USE_ROW_MAJOR_DATA
	int gridSizeX16 = (nd2_tyy[17] - nd2_tyy[16])/blockSizeX + 1;
	int gridSizeY16 = (nd2_tyy[11] - nd2_tyy[6])/blockSizeY + 1;
#else
	int gridSizeX16 = (nd2_tyy[5] - nd2_tyy[0])/blockSizeX + 1;
	int gridSizeY16 = (nd2_tyy[11] - nd2_tyy[6])/blockSizeY + 1;
#endif
	size_t dimGrid16[3] = {gridSizeX16, gridSizeY16, 1};

	errNum = clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 0, sizeof(int), nxb2);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 1, sizeof(int), nyb2);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 2, sizeof(int), mw2_pml);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 3, sizeof(int), mw2_pml1);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 4, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 5, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 6, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 7, sizeof(cl_mem), &nd2_tyyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 8, sizeof(cl_mem), &idmat2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 9, sizeof(float), ca);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 10, sizeof(cl_mem), &damp2_zD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 11, sizeof(cl_mem), &drth2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 12, sizeof(cl_mem), &clamdaD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 13, sizeof(cl_mem), &cmuD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 14, sizeof(cl_mem), &epdtD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 15, sizeof(cl_mem), &qwpD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 16, sizeof(cl_mem), &qwsD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 17, sizeof(cl_mem), &qwt1D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 18, sizeof(cl_mem), &qwt2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 19, sizeof(cl_mem), &dxh2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 20, sizeof(cl_mem), &dyh2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 21, sizeof(cl_mem), &dzi2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 22, sizeof(cl_mem), &t2xxD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 23, sizeof(cl_mem), &t2yyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 24, sizeof(cl_mem), &t2zzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 25, sizeof(cl_mem), &qt2xxD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 26, sizeof(cl_mem), &qt2yyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 27, sizeof(cl_mem), &qt2zzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 28, sizeof(cl_mem), &t2xx_pzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 29, sizeof(cl_mem), &t2zz_pzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 30, sizeof(cl_mem), &qt2xx_pzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 31, sizeof(cl_mem), &qt2zz_pzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 32, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 33, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_stress_norm_PmlZ_IIC, 34, sizeof(cl_mem), &v2zD);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_norm_PmlZ_IIC arguments!\n");
	}
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid16[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid16[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid16[2]*localWorkSize[2];
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[1], _cl_kernel_stress_norm_PmlZ_IIC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_norm_PmlZ_IIC for execution!\n");
	}

	if (lbx[1] >= lbx[0])
	{
#ifdef DISFD_USE_ROW_MAJOR_DATA
		int gridSizeX17 = (nd2_txy[17] - nd2_txy[12])/blockSizeX + 1;
		int gridSizeY17 = (nd2_txy[5] - nd2_txy[0])/blockSizeY + 1;
#else
		int gridSizeX17 = (nd2_txy[5] - nd2_txy[0])/blockSizeX + 1;
		int gridSizeY17 = (lbx[1] - lbx[0])/blockSizeY + 1;
#endif
		size_t dimGrid17[3] = {gridSizeX17, gridSizeY17, 1};
		errNum = clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 0, sizeof(int), nxb2);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 1, sizeof(int), nyb2);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 2, sizeof(int), mw2_pml);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 3, sizeof(int), mw2_pml1);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 4, sizeof(int), nxbtm);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 5, sizeof(int), nybtm);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 6, sizeof(int), nzbtm);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 7, sizeof(int), nztop);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 8, sizeof(int), &lbx[0]);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 9, sizeof(int), &lbx[1]);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 10, sizeof(cl_mem), &nd2_txyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 11, sizeof(cl_mem), &idmat2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 12, sizeof(float), ca);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 13, sizeof(cl_mem), &drth2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 14, sizeof(cl_mem), &damp2_xD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 15, sizeof(cl_mem), &cmuD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 16, sizeof(cl_mem), &epdtD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 17, sizeof(cl_mem), &qwsD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 18, sizeof(cl_mem), &qwt1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 19, sizeof(cl_mem), &qwt2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 20, sizeof(cl_mem), &dxi2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 21, sizeof(cl_mem), &dyi2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 22, sizeof(cl_mem), &t2xyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 23, sizeof(cl_mem), &qt2xyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 24, sizeof(cl_mem), &t2xy_pxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 25, sizeof(cl_mem), &qt2xy_pxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 26, sizeof(cl_mem), &v2xD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlX_IIC, 27, sizeof(cl_mem), &v2yD);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: setting kernel _cl_kernel_stress_xy_PmlX_IIC arguments!\n");
		}
		localWorkSize[0] = dimBlock[0];
		localWorkSize[1] = dimBlock[1];
		localWorkSize[2] = dimBlock[2];
		globalWorkSize[0] = dimGrid17[0]*localWorkSize[0];
		globalWorkSize[1] = dimGrid17[1]*localWorkSize[1];
		globalWorkSize[2] = dimGrid17[2]*localWorkSize[2];
		errNum = clEnqueueNDRangeKernel(_cl_commandQueues[1], _cl_kernel_stress_xy_PmlX_IIC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_xy_PmlX_IIC for execution!\n");
		}
	}

	if (lby[1] >= lby[0])
	{
#ifdef DISFD_USE_ROW_MAJOR_DATA
		int gridSizeX18 = (nd2_txy[17] - nd2_txy[12])/blockSizeX + 1;
		int gridSizeY18 = (nd2_txy[11] - nd2_txy[6])/blockSizeY + 1;
#else
		int gridSizeX18 = (nd2_txy[11] - nd2_txy[6])/blockSizeX + 1;
		int gridSizeY18 = (lby[1] - lby[0])/blockSizeY + 1;
#endif
		size_t dimGrid18[3] = {gridSizeX18, gridSizeY18, 1};

		errNum = clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 0, sizeof(int), nxb2);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 1, sizeof(int), nyb2);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 2, sizeof(int), mw2_pml1);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 3, sizeof(int), nztop);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 4, sizeof(int), nxbtm);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 5, sizeof(int), nzbtm);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 6, sizeof(int), &lby[0]);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 7, sizeof(int), &lby[1]);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 8, sizeof(cl_mem), &nd2_txyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 9, sizeof(cl_mem), &idmat2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 10, sizeof(float), ca);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 11, sizeof(cl_mem), &drth2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 12, sizeof(cl_mem), &damp2_yD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 13, sizeof(cl_mem), &cmuD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 14, sizeof(cl_mem), &epdtD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 15, sizeof(cl_mem), &qwsD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 16, sizeof(cl_mem), &qwt1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 17, sizeof(cl_mem), &qwt2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 18, sizeof(cl_mem), &dxi2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 19, sizeof(cl_mem), &dyi2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 20, sizeof(cl_mem), &t2xyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 21, sizeof(cl_mem), &qt2xyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 22, sizeof(cl_mem), &t2xy_pyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 23, sizeof(cl_mem), &qt2xy_pyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 24, sizeof(cl_mem), &v2xD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlY_IIC, 25, sizeof(cl_mem), &v2yD);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: setting kernel _cl_kernel_stress_xy_PmlY_IIC arguments!\n");
		}
		localWorkSize[0] = dimBlock[0];
		localWorkSize[1] = dimBlock[1];
		localWorkSize[2] = dimBlock[2];
		globalWorkSize[0] = dimGrid18[0]*localWorkSize[0];
		globalWorkSize[1] = dimGrid18[1]*localWorkSize[1];
		globalWorkSize[2] = dimGrid18[2]*localWorkSize[2];
		errNum = clEnqueueNDRangeKernel(_cl_commandQueues[1], _cl_kernel_stress_xy_PmlY_IIC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_xy_PmlY_IIC for execution!\n");
		}
	}

#ifdef DISFD_USE_ROW_MAJOR_DATA
	int gridSizeX19 = (nd2_txy[17] - nd2_txy[16])/blockSizeX + 1;
	int gridSizeY19 = (nd2_txy[9] - nd2_txy[8])/blockSizeY + 1;
#else
	int gridSizeX19 = (nd2_txy[3] - nd2_txy[2])/blockSizeX + 1;
	int gridSizeY19 = (nd2_txy[9] - nd2_txy[8])/blockSizeY + 1;
#endif
	size_t dimGrid19[3] = {gridSizeX19, gridSizeY19, 1};

	errNum = clSetKernelArg(_cl_kernel_stress_xy_PmlZ_II, 0, sizeof(int), nxb2);
	errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlZ_II, 1, sizeof(int), nyb2);
	errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlZ_II, 2, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlZ_II, 3, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlZ_II, 4, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlZ_II, 5, sizeof(cl_mem), &nd2_txyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlZ_II, 6, sizeof(cl_mem), &idmat2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlZ_II, 7, sizeof(cl_mem), &cmuD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlZ_II, 8, sizeof(cl_mem), &epdtD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlZ_II, 9, sizeof(cl_mem), &qwsD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlZ_II, 10, sizeof(cl_mem), &qwt1D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlZ_II, 11, sizeof(cl_mem), &qwt2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlZ_II, 12, sizeof(cl_mem), &dxi2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlZ_II, 13, sizeof(cl_mem), &dyi2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlZ_II, 14, sizeof(cl_mem), &t2xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlZ_II, 15, sizeof(cl_mem), &qt2xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlZ_II, 16, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xy_PmlZ_II, 17, sizeof(cl_mem), &v2yD);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_xy_PmlZ_II arguments!\n");
	}
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid19[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid19[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid19[2]*localWorkSize[2];
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[1], _cl_kernel_stress_xy_PmlZ_II, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_xy_PmlZ_II for execution!\n");
	}

	if (lbx[1] >= lbx[0])
	{
#ifdef DISFD_USE_ROW_MAJOR_DATA
		int gridSizeX20 = (nd2_txz[17] - nd2_txz[12])/blockSizeX + 1;
		int gridSizeY20 = (nd2_txz[5] - nd2_txz[0])/blockSizeY + 1;
#else
		int gridSizeX20 = (nd2_txz[5] - nd2_txz[0])/blockSizeX + 1;
		int gridSizeY20 = (lbx[1] - lbx[0])/blockSizeY + 1;
#endif
		size_t dimGrid20[3] = {gridSizeX20, gridSizeY20, 1};

		errNum = clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 0, sizeof(int), nxb2);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 1, sizeof(int), nyb2);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 2, sizeof(int), mw2_pml);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 3, sizeof(int), mw2_pml1);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 4, sizeof(int), nxbtm);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 5, sizeof(int), nybtm);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 6, sizeof(int), nzbtm);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 7, sizeof(int), nztop);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 8, sizeof(int), &lbx[0]);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 9, sizeof(int), &lbx[1]);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 10, sizeof(cl_mem), &nd2_txzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 11, sizeof(cl_mem), &idmat2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 12, sizeof(float), ca);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 13, sizeof(cl_mem), &drth2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 14, sizeof(cl_mem), &damp2_xD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 15, sizeof(cl_mem), &cmuD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 16, sizeof(cl_mem), &epdtD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 17, sizeof(cl_mem), &qwsD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 18, sizeof(cl_mem), &qwt1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 19, sizeof(cl_mem), &qwt2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 20, sizeof(cl_mem), &dxi2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 21, sizeof(cl_mem), &dzh2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 22, sizeof(cl_mem), &t2xzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 23, sizeof(cl_mem), &qt2xzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 24, sizeof(cl_mem), &t2xz_pxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 25, sizeof(cl_mem), &qt2xz_pxD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 26, sizeof(cl_mem), &v2xD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlX_IIC, 27, sizeof(cl_mem), &v2zD);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: setting kernel _cl_kernel_stress_xz_PmlX_IIC arguments!\n");
		}
		localWorkSize[0] = dimBlock[0];
		localWorkSize[1] = dimBlock[1];
		localWorkSize[2] = dimBlock[2];
		globalWorkSize[0] = dimGrid20[0]*localWorkSize[0];
		globalWorkSize[1] = dimGrid20[1]*localWorkSize[1];
		globalWorkSize[2] = dimGrid20[2]*localWorkSize[2];
		errNum = clEnqueueNDRangeKernel(_cl_commandQueues[1], _cl_kernel_stress_xz_PmlX_IIC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_xz_PmlX_IIC for execution!\n");
		}
	}

	if (lby[1] >= lby[0])
	{
#ifdef DISFD_USE_ROW_MAJOR_DATA
		int gridSizeX21 = (nd2_txz[15] - nd2_txz[12])/blockSizeX + 1;
		int gridSizeY21 = (nd2_txz[9] - nd2_txz[8])/blockSizeY + 1;
#else
		int gridSizeX21 = (nd2_txz[9] - nd2_txz[8])/blockSizeX + 1;
		int gridSizeY21 = (lby[1] - lby[0])/blockSizeY + 1;
#endif
		size_t dimGrid21[3] = {gridSizeX21, gridSizeY21, 1};

		errNum = clSetKernelArg(_cl_kernel_stress_xz_PmlY_IIC, 0, sizeof(int), nxb2);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IIC, 1, sizeof(int), nyb2);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IIC, 2, sizeof(int), nxbtm);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IIC, 3, sizeof(int), nzbtm);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IIC, 4, sizeof(int), nztop);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IIC, 5, sizeof(int), &lby[0]);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IIC, 6, sizeof(int), &lby[1]);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IIC, 7, sizeof(cl_mem), &nd2_txzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IIC, 8, sizeof(cl_mem), &idmat2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IIC, 9, sizeof(cl_mem), &cmuD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IIC, 10, sizeof(cl_mem), &epdtD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IIC, 11, sizeof(cl_mem), &qwsD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IIC, 12, sizeof(cl_mem), &qwt1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IIC, 13, sizeof(cl_mem), &qwt2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IIC, 14, sizeof(cl_mem), &dxi2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IIC, 15, sizeof(cl_mem), &dzh2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IIC, 16, sizeof(cl_mem), &v2xD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IIC, 17, sizeof(cl_mem), &v2zD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IIC, 18, sizeof(cl_mem), &t2xzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlY_IIC, 19, sizeof(cl_mem), &qt2xzD);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: setting kernel _cl_kernel_stress_xz_PmlY_IIC arguments!\n");
		}
		localWorkSize[0] = dimBlock[0];
		localWorkSize[1] = dimBlock[1];
		localWorkSize[2] = dimBlock[2];
		globalWorkSize[0] = dimGrid21[0]*localWorkSize[0];
		globalWorkSize[1] = dimGrid21[1]*localWorkSize[1];
		globalWorkSize[2] = dimGrid21[2]*localWorkSize[2];
		errNum = clEnqueueNDRangeKernel(_cl_commandQueues[1], _cl_kernel_stress_xz_PmlY_IIC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_xz_PmlY_IIC for execution!\n");
		}
	}

#ifdef DISFD_USE_ROW_MAJOR_DATA
	int gridSizeX22 = (nd2_txz[17] - nd2_txz[16])/blockSizeX + 1;
	int gridSizeY22 = (nd2_txz[11] - nd2_txz[6])/blockSizeY + 1;
#else
	int gridSizeX22 = (nd2_txz[5] - nd2_txz[0])/blockSizeX + 1;
	int gridSizeY22 = (nd2_txz[11] - nd2_txz[6])/blockSizeY + 1;
#endif
	size_t dimGrid22[3] = {gridSizeX22, gridSizeY22, 1};

	errNum = clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 0, sizeof(int), nxb2);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 1, sizeof(int), nyb2);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 2, sizeof(int), mw2_pml1);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 3, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 4, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 5, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 6, sizeof(cl_mem), &nd2_txzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 7, sizeof(cl_mem), &idmat2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 8, sizeof(float), ca);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 9, sizeof(cl_mem), &drti2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 10, sizeof(cl_mem), &damp2_zD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 11, sizeof(cl_mem), &cmuD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 12, sizeof(cl_mem), &epdtD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 13, sizeof(cl_mem), &qwsD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 14, sizeof(cl_mem), &qwt1D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 15, sizeof(cl_mem), &qwt2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 16, sizeof(cl_mem), &dxi2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 17, sizeof(cl_mem), &dzh2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 18, sizeof(cl_mem), &t2xzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 19, sizeof(cl_mem), &qt2xzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 20, sizeof(cl_mem), &t2xz_pzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 21, sizeof(cl_mem), &qt2xz_pzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 22, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_stress_xz_PmlZ_IIC, 23, sizeof(cl_mem), &v2zD);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_xz_PmlZ_IIC arguments!\n");
	}
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid22[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid22[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid22[2]*localWorkSize[2];
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[1], _cl_kernel_stress_xz_PmlZ_IIC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_xz_PmlZ_IIC for execution!\n");
	}

	if (lbx[1] >= lbx[0])
	{
#ifdef DISFD_USE_ROW_MAJOR_DATA
		int gridSizeX23 = (nd2_tyz[15] - nd2_tyz[12])/blockSizeX + 1;
		int gridSizeY23 = (nd2_tyz[3] - nd2_tyz[2])/blockSizeY + 1;
#else
		int gridSizeX23 = (nd2_tyz[3] - nd2_tyz[2])/blockSizeX + 1;
		int gridSizeY23 = (lbx[1] - lbx[0])/blockSizeY + 1;
#endif
		size_t dimGrid23[3] = {gridSizeX23, gridSizeY23, 1};
		errNum = clSetKernelArg(_cl_kernel_stress_yz_PmlX_IIC, 0, sizeof(int), nxb2);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IIC, 1, sizeof(int), nyb2);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IIC, 2, sizeof(int), nxbtm);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IIC, 3, sizeof(int), nzbtm);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IIC, 4, sizeof(int), nztop);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IIC, 5, sizeof(int), &lbx[0]);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IIC, 6, sizeof(int), &lbx[1]);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IIC, 7, sizeof(cl_mem), &nd2_tyzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IIC, 8, sizeof(cl_mem), &idmat2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IIC, 9, sizeof(cl_mem), &cmuD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IIC, 10, sizeof(cl_mem), &epdtD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IIC, 11, sizeof(cl_mem), &qwsD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IIC, 12, sizeof(cl_mem), &qwt1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IIC, 13, sizeof(cl_mem), &qwt2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IIC, 14, sizeof(cl_mem), &dyi2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IIC, 15, sizeof(cl_mem), &dzh2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IIC, 16, sizeof(cl_mem), &t2yzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IIC, 17, sizeof(cl_mem), &qt2yzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IIC, 18, sizeof(cl_mem), &v2yD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlX_IIC, 19, sizeof(cl_mem), &v2zD);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: setting kernel _cl_kernel_stress_yz_PmlX_IIC arguments!\n");
		}
		localWorkSize[0] = dimBlock[0];
		localWorkSize[1] = dimBlock[1];
		localWorkSize[2] = dimBlock[2];
		globalWorkSize[0] = dimGrid23[0]*localWorkSize[0];
		globalWorkSize[1] = dimGrid23[1]*localWorkSize[1];
		globalWorkSize[2] = dimGrid23[2]*localWorkSize[2];
		errNum = clEnqueueNDRangeKernel(_cl_commandQueues[1], _cl_kernel_stress_yz_PmlX_IIC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_yz_PmlX_IIC for execution!\n");
		}
	}

	if (lby[1] >= lby[0])
	{
#ifdef DISFD_USE_ROW_MAJOR_DATA
		int gridSizeX24 = (nd2_tyz[17] - nd2_tyz[12])/blockSizeX + 1;
		int gridSizeY24 = (nd2_tyz[11] - nd2_tyz[6])/blockSizeY + 1;
#else
		int gridSizeX24 = (nd2_tyz[11] - nd2_tyz[6])/blockSizeX + 1;
		int gridSizeY24 = (lby[1] - lby[0])/blockSizeY + 1;
#endif
		size_t dimGrid24[3] = {gridSizeX24, gridSizeY24, 1};

		errNum = clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 0, sizeof(int), nxb2);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 1, sizeof(int), nyb2);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 2, sizeof(int), mw2_pml1);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 3, sizeof(int), nxbtm);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 4, sizeof(int), nzbtm);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 5, sizeof(int), nztop);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 6, sizeof(int), &lby[0]);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 7, sizeof(int), &lby[1]);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 8, sizeof(cl_mem), &nd2_tyzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 9, sizeof(cl_mem), &idmat2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 10, sizeof(float), ca);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 11, sizeof(cl_mem), &drth2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 12, sizeof(cl_mem), &damp2_yD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 13, sizeof(cl_mem), &cmuD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 14, sizeof(cl_mem), &epdtD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 15, sizeof(cl_mem), &qwsD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 16, sizeof(cl_mem), &qwt1D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 17, sizeof(cl_mem), &qwt2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 18, sizeof(cl_mem), &dyi2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 19, sizeof(cl_mem), &dzh2D);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 20, sizeof(cl_mem), &t2yzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 21, sizeof(cl_mem), &qt2yzD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 22, sizeof(cl_mem), &t2yz_pyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 23, sizeof(cl_mem), &qt2yz_pyD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 24, sizeof(cl_mem), &v2yD);
		errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlY_IIC, 25, sizeof(cl_mem), &v2zD);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: setting kernel _cl_kernel_stress_yz_PmlY_IIC arguments!\n");
		}
		localWorkSize[0] = dimBlock[0];
		localWorkSize[1] = dimBlock[1];
		localWorkSize[2] = dimBlock[2];
		globalWorkSize[0] = dimGrid24[0]*localWorkSize[0];
		globalWorkSize[1] = dimGrid24[1]*localWorkSize[1];
		globalWorkSize[2] = dimGrid24[2]*localWorkSize[2];
		errNum = clEnqueueNDRangeKernel(_cl_commandQueues[1], _cl_kernel_stress_yz_PmlY_IIC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
		if(errNum != CL_SUCCESS)
		{
			fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_yz_PmlY_IIC for execution!\n");
		}
	}

#ifdef DISFD_USE_ROW_MAJOR_DATA
	int gridSizeX25 = (nd2_tyz[17] - nd2_tyz[16])/blockSizeX + 1;
	int gridSizeY25 = (nd2_tyz[11] - nd2_tyz[6])/blockSizeY + 1;
#else
	int gridSizeX25 = (nd2_tyz[5] - nd2_tyz[0])/blockSizeX + 1;
	int gridSizeY25 = (nd2_tyz[11] - nd2_tyz[6])/blockSizeY + 1;
#endif
	size_t dimGrid25[3] = {gridSizeX25, gridSizeY25, 1};

	errNum = clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 0, sizeof(int), nxb2);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 1, sizeof(int), nyb2);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 2, sizeof(int), mw2_pml1);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 3, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 4, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 5, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 6, sizeof(cl_mem), &nd2_tyzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 7, sizeof(cl_mem), &idmat2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 8, sizeof(float), ca);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 9, sizeof(cl_mem), &drti2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 10, sizeof(cl_mem), &damp2_zD);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 11, sizeof(cl_mem), &cmuD);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 12, sizeof(cl_mem), &epdtD);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 13, sizeof(cl_mem), &qwsD);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 14, sizeof(cl_mem), &qwt1D);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 15, sizeof(cl_mem), &qwt2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 16, sizeof(cl_mem), &dyi2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 17, sizeof(cl_mem), &dzh2D);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 18, sizeof(cl_mem), &t2yzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 19, sizeof(cl_mem), &qt2yzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 20, sizeof(cl_mem), &t2yz_pzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 21, sizeof(cl_mem), &qt2yz_pzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 22, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_stress_yz_PmlZ_IIC, 23, sizeof(cl_mem), &v2zD);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_yz_PmlZ_IIC arguments!\n");
	}
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid25[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid25[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid25[2]*localWorkSize[2];
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[1], _cl_kernel_stress_yz_PmlZ_IIC, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_yz_PmlZ_IIC for execution!\n");
	}

	for(i = 0; i < NUM_COMMAND_QUEUES; i++) {
#ifdef SNUCL_PERF_MODEL_OPTIMIZATION
		if(counter < 1) {
			clSetCommandQueueProperty(_cl_commandQueues[i], 
#ifdef SNUCL_PERF_MODEL_OPTIMIZATION
#endif
					CL_QUEUE_PROFILING_ENABLE, 
					true,
					NULL);
		}
		else 
#endif
		{
			errNum = clFinish(_cl_commandQueues[i]);
			if(errNum != CL_SUCCESS)
			{
				fprintf(stderr, "Error: finishing stress kernel for execution!\n");
			}
		}
		printf("DISFD Q%d Stress ReSet Kernel Args\n", i);
		Stop(&kernelTimerStress[i]);
	}
	counter++;
	// printf("[OpenCL] computating finished!\n");

	gettimeofday(&t2, NULL);
	tmpTime = 1000.0 * (t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec) / 1000.0;
	totalTimeCompS += tmpTime;

	gettimeofday(&t1, NULL);
	//Start(&d2hTimerStress);
#if !defined(DISFD_GPU_MARSHALING) || defined(DISFD_DEBUG)
	cpy_d2h_stressOutputsC_opencl(t1xxM, t1xyM, t1xzM, t1yyM, t1yzM, t1zzM, t2xxM, t2xyM, t2xzM, t2yyM, t2yzM, t2zzM, nxtop, nytop, nztop, nxbtm, nybtm, nzbtm);
#endif
	//Stop(&d2hTimerStress);
	gettimeofday(&t2, NULL);

	tmpTime = 1000.0 * (t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec) / 1000.0;
	totalTimeD2HS += tmpTime;

#ifdef DISFD_DEBUG
	if(counter == 10) {
	int size = (*nztop) * (*nxtop + 3) * (*nytop);
	write_output(t1xxM, size, "OUTPUT_ARRAYS/t1xxM.txt");
	size = (*nztop) * (*nxtop + 3) * (*nytop + 3);
	write_output(t1xyM, size, "OUTPUT_ARRAYS/t1xyM.txt");
	size = (*nztop + 1) * (*nxtop + 3) * (*nytop);
	write_output(t1xzM, size, "OUTPUT_ARRAYS/t1xzM.txt");
	size = (*nztop) * (*nxtop) * (*nytop + 3);
	write_output(t1yyM, size, "OUTPUT_ARRAYS/t1yyM.txt");
	size = (*nztop + 1) * (*nxtop) * (*nytop + 3);
	write_output(t1yzM, size, "OUTPUT_ARRAYS/t1yzM.txt");
	size = (*nztop) * (*nxtop) * (*nytop);
	write_output(t1zzM, size, "OUTPUT_ARRAYS/t1zzM.txt");
	size = (*nzbtm) * (*nxbtm + 3) * (*nybtm);
	write_output(t2xxM, size, "OUTPUT_ARRAYS/t2xxM.txt");
	size = (*nzbtm) * (*nxbtm + 3) * (*nybtm + 3);
	write_output(t2xyM, size, "OUTPUT_ARRAYS/t2xyM.txt");
	size = (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm);
	write_output(t2xzM, size, "OUTPUT_ARRAYS/t2xzM.txt");
	size = (*nzbtm) * (*nxbtm) * (*nybtm + 3);
	write_output(t2yyM, size, "OUTPUT_ARRAYS/t2yyM.txt");
	size = (*nzbtm + 1) * (*nxbtm) * (*nybtm + 3);
	write_output(t2yzM, size, "OUTPUT_ARRAYS/t2yzM.txt");
	size = (*nzbtm + 1) * (*nxbtm) * (*nybtm);
	write_output(t2zzM, size, "OUTPUT_ARRAYS/t2zzM.txt");
	}
#endif
	/************** correctness *********************/
	/*   
		 FILE *fp;
		 int i;
		 const char *filename = "v1x.txt";
		 const char *filename1 = "v1y.txt";
		 const char *filename2 = "v1z.txt";
		 if((fp = fopen(filename, "w+")) == NULL)
		 {
		 fprintf(stderr, "File Write Error!\n");
		 }
		 for(i =0; i<(*nztop+2)*(*nxtop+3)*(*nytop+3); i++)
		 {
		 fprintf(fp,"%f ", v1xM[i]);

		 }
		 fprintf(fp, "\n");
		 fclose(fp);
		 if((fp = fopen(filename1, "w+")) == NULL)
		 {
		 fprintf(stderr, "File Write Error!\n");
		 }
		 for(i =0; i<(*nztop+2)*(*nxtop+3)*(*nytop+3); i++)
		 {
		 fprintf(fp,"%f ", v1yM[i]);

		 }
		 fprintf(fp, "\n");
		 fclose(fp);
		 if((fp = fopen(filename2, "w+")) == NULL)
		 {
		 fprintf(stderr, "File Write Error!\n");
		 }
		 for(i =0; i<(*nztop+2)*(*nxtop+3)*(*nytop+3); i++)
		 {
		 fprintf(fp,"%f ", v1zM[i]);

		 }
		 fprintf(fp, "\n");
		 fclose(fp);

		 const char *filename3 = "x_t1xx.txt";
		 const char *filename4 = "x_t1xy.txt";
		 const char *filename5 = "x_t1xz.txt";

		 if((fp = fopen(filename3, "w+")) == NULL)
		 fprintf(stderr, "File write error!\n");

		 for(i = 0; i< (*nztop) * (*nxtop + 3) * (*nytop); i++ )
		 {
		 fprintf(fp, "%f ", t1xxM[i]);
		 }
		 fprintf(fp, "\n");
		 fclose(fp);
		 if((fp = fopen(filename4, "w+")) == NULL)
		 fprintf(stderr, "File write error!\n");

		 for(i = 0; i< (*nztop) * (*nxtop + 3) * (*nytop+3); i++ )
		 {
		 fprintf(fp, "%f ", t1xyM[i]);
		 }
		 fprintf(fp, "\n");
		 fclose(fp);
		 if((fp = fopen(filename5, "w+")) == NULL)
		 fprintf(stderr, "File write error!\n");
		 for(i = 0; i< (*nztop+1) * (*nxtop + 3) * (*nytop); i++ )
		 {
		 fprintf(fp, "%f ", t1xzM[i]);
		 }
		 fprintf(fp, "\n");
		 fclose(fp);
		 */
	return;
}
/* 
   Following functions are responsible for data marshalling on GPU
   after velocity computation.
   If MPI-ACC is not used, the marshalled arrays are transferred
   back to CPU for MPI communication.
   */
void sdx51_vel (float** sdx51, int* nytop, int* nztop, int* nxtop) {
	int which_qid = MARSHAL_CMDQ;
	double tstart, tend;
	double cpy_time=0, kernel_time=0;
	cl_int errNum;
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nztop)/blockSizeX + 1;
	int gridY = (*nytop)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];

	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx51, argIdx++, sizeof(cl_mem), &sdx51D);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx51, argIdx++, sizeof(cl_mem), &v1xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx51, argIdx++, sizeof(cl_mem), &v1yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx51, argIdx++, sizeof(cl_mem), &v1zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx51, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx51, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx51, argIdx++, sizeof(int), nxtop);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_sdx51 arguments!\n");
	}
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_sdx51, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_yz_PmlZ_IIC for execution!\n");
	}
	//vel_sdx51 <<< dimGrid, dimBlock >>> (sdx51D, v1xD, v1yD, v1zD, *nytop, *nztop, *nxtop);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, sdx51_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL sdx51_vel", kernel_time );

#if 0 //USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(*sdx51, sdx51D, sizeof(float) * (*nztop)* (*nytop)* 5, cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sdx51D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY sdx51_vel", cpy_time );
#endif   
}
void sdx52_vel (float** sdx52, int* nybtm, int* nzbtm, int* nxbtm) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nzbtm)/blockSizeX + 1;
	int gridY = (*nybtm)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	record_time(&tstart);
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx52, argIdx++, sizeof(cl_mem), &sdx52D);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx52, argIdx++, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx52, argIdx++, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx52, argIdx++, sizeof(cl_mem), &v2zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx52, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx52, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx52, argIdx++, sizeof(int), nxbtm);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_sdx52 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_sdx52, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_yz_PmlZ_IIC for execution!\n");
	}
	//vel_sdbtm <<< dimGrid, dimBlock >>> (sdx52D, v2xD, v2yD, v2zD,  *nybtm, *nzbtm, *nxbtm);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, sdy52_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL sdx52_vel", kernel_time );


#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(*sdx52, sdx52D, sizeof(float) * (*nzbtm)* (*nybtm)* 5, cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sdx52D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY sdx52_vel", cpy_time );
#endif    
}

void sdx41_vel (float** sdx41, int* nxtop, int* nytop, int* nztop, int* nxtm1) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nztop)/blockSizeX + 1;
	int gridY = (*nytop)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	record_time(&tstart);
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx41, argIdx++, sizeof(cl_mem), &sdx41D);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx41, argIdx++, sizeof(cl_mem), &v1xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx41, argIdx++, sizeof(cl_mem), &v1yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx41, argIdx++, sizeof(cl_mem), &v1zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx41, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx41, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx41, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx41, argIdx++, sizeof(int), nxtm1);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_sdx41 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_sdx41, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_sdx41 for execution!\n");
	}
	//vel_sdx41 <<< dimGrid, dimBlock >>> (sdx41D, v1xD, v1yD, v1zD, *nytop, *nztop, *nxtop, *nxtm1);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, sdx41_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL sdx41_vel", kernel_time );

#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(*sdx41, sdx41D, sizeof(float) * (*nztop)* (*nytop)* 4, cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sdx41D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY sdx41_vel", cpy_time );
#endif    
}

void sdx42_vel (float** sdx42, int* nxbtm, int* nybtm, int* nzbtm, int* nxbm1){ 
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nzbtm)/blockSizeX + 1;
	int gridY = (*nybtm)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};
	printf("Sdx42 Block ( %lu %lu %lu ), Grid: ( %lu %lu %lu ) \n", dimBlock[0], dimBlock[1], dimBlock[2],
							dimGrid[0], dimGrid[1], dimGrid[2]);
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx42, argIdx++, sizeof(cl_mem), &sdx42D);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx42, argIdx++, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx42, argIdx++, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx42, argIdx++, sizeof(cl_mem), &v2zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx42, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx42, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx42, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx42, argIdx++, sizeof(int), nxbm1);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_sdx42 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_sdx42, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_sdx42 for execution!\n");
	}

	//vel_sdx42 <<< dimGrid, dimBlock >>> (sdx42D, v2xD, v2yD, v2zD, *nybtm, *nzbtm, *nxbtm, *nxbm1);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, sdx42_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL sdx42_vel", kernel_time );

#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(*sdx42, sdx42D, sizeof(float) * (*nzbtm)* (*nybtm)* 4, cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sdx42D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY sdx42_vel", cpy_time );
#endif    
}

// --- sdy
void sdy51_vel (float** sdy51, int* nxtop, int* nytop, int* nztop) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	struct timeval start, end;
	cl_int errNum;
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nztop)/blockSizeX + 1;
	int gridY = (*nxtop)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];

	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy51, argIdx++, sizeof(cl_mem), &sdy51D);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy51, argIdx++, sizeof(cl_mem), &v1xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy51, argIdx++, sizeof(cl_mem), &v1yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy51, argIdx++, sizeof(cl_mem), &v1zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy51, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy51, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy51, argIdx++, sizeof(int), nxtop);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_sdy51 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_sdy51, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_sdy51 for execution!\n");
	}
	//vel_sdy51 <<< dimGrid, dimBlock >>> (sdy51D, v1xD, v1yD, v1zD, *nytop, *nztop, *nxtop);
	errNum=clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel failed sdy51_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL sdy51_vel", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(sdy51D_meta) ;
#endif    
#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(*sdy51, sdy51D, sizeof(float) * (*nztop)* (*nxtop)* 5, cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sdy51D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY sdy51_vel", cpy_time );
#endif    
}
void sdy52_vel (float** sdy52, int* nxbtm, int* nybtm, int* nzbtm) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nzbtm)/blockSizeX + 1;
	int gridY = (*nxbtm)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy52, argIdx++, sizeof(cl_mem), &sdy52D);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy52, argIdx++, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy52, argIdx++, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy52, argIdx++, sizeof(cl_mem), &v2zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy52, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy52, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy52, argIdx++, sizeof(int), nxbtm);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_sdy52 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_sdy52, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_sdy52 for execution!\n");
	}
	//vel_sdy52 <<< dimGrid, dimBlock >>> (sdy52D, v2xD, v2yD, v2zD,  *nybtm, *nzbtm, *nxbtm);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, sdy52_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL sdy52_vel", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(sdy52D_meta); 
#endif    
#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(*sdy52, sdy52D, sizeof(float)*(*nzbtm)* (*nxbtm)* 5, cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sdy52D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY sdy52_vel", cpy_time );
#endif    
}
void sdy41_vel (float** sdy41, int* nxtop, int* nytop, int* nztop, int* nytm1) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nztop)/blockSizeX + 1;
	int gridY = (*nxtop)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy41, argIdx++, sizeof(cl_mem), &sdy41D);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy41, argIdx++, sizeof(cl_mem), &v1xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy41, argIdx++, sizeof(cl_mem), &v1yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy41, argIdx++, sizeof(cl_mem), &v1zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy41, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy41, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy41, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy41, argIdx++, sizeof(int), nytm1);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_sdy41 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_sdy41, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_sdy41 for execution!\n");
	}
	//vel_sdy41 <<< dimGrid, dimBlock >>> (sdy41D, v1xD, v1yD, v1zD, *nytop, *nztop, *nxtop, *nytm1);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, sdy41_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL sdy41_vel", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(sdy41D_meta); 
#endif
#if 0// USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(*sdy41, sdy41D, sizeof(float) * (*nztop)* (*nxtop)* 4, cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sdy41D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY sdy41_vel", cpy_time );
#endif    
}

void sdy42_vel (float** sdy42, int* nxbtm, int* nybtm, int* nzbtm, int* nybm1) 
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nzbtm)/blockSizeX + 1;
	int gridY = (*nxbtm)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy42, argIdx++, sizeof(cl_mem), &sdy42D);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy42, argIdx++, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy42, argIdx++, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy42, argIdx++, sizeof(cl_mem), &v2zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy42, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy42, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy42, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy42, argIdx++, sizeof(int), nybm1);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_sdy42 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_sdy42, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_sdy42 for execution!\n");
	}
	//vel_sdy42 <<< dimGrid, dimBlock >>> (sdy42D, v2xD, v2yD, v2zD, *nybtm, *nzbtm, *nxbtm, *nybm1);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, sdy42_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL sdy42_vel", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(sdy42D_meta); 
#endif    
#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(*sdy42, sdy42D, sizeof(float) * (*nzbtm)* (*nxbtm)* 4, cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sdy42D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY sdy42_vel", cpy_time );
#endif    
}
//--- sdy's
// -- RCX/Y

void rcx51_vel (float** rcx51, int* nxtop, int* nytop, int* nztop, int* nx1p1, int* nx1p2) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	struct timeval start, end;
	cl_int errNum;
#if 0// USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(rcx51D, *rcx51, sizeof(float) * (*nztop)* (*nytop)* 5, cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, rcx51D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY rcx51_vel", cpy_time );
#endif
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nztop)/blockSizeX + 1;
	int gridY = (*nytop)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx51, argIdx++, sizeof(cl_mem), &rcx51D);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx51, argIdx++, sizeof(cl_mem), &v1xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx51, argIdx++, sizeof(cl_mem), &v1yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx51, argIdx++, sizeof(cl_mem), &v1zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx51, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx51, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx51, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx51, argIdx++, sizeof(int), nx1p1);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx51, argIdx++, sizeof(int), nx1p2);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_rcx51 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_rcx51, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_rcx51 for execution!\n");
	}
	//vel_rcx51 <<< dimGrid, dimBlock >>> (rcx51D, v1xD, v1yD, v1zD, *nytop, *nztop, *nxtop, *nx1p1, *nx1p2);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, rcx51_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL rcx51_vel", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(rcx51D_meta); 
#endif    
}

void rcx52_vel (float** rcx52, int* nxbtm, int* nybtm, int* nzbtm, int* nx2p1, int* nx2p2) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(rcx52D, *rcx52, sizeof(float) * (*nzbtm)* (*nybtm)* 5, cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevicE, rcx52D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY rcx52_vel", cpy_time );
#endif    
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nzbtm)/blockSizeX + 1;
	int gridY = (*nybtm)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx52, argIdx++, sizeof(cl_mem), &rcx52D);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx52, argIdx++, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx52, argIdx++, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx52, argIdx++, sizeof(cl_mem), &v2zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx52, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx52, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx52, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx52, argIdx++, sizeof(int), nx2p1);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx52, argIdx++, sizeof(int), nx2p2);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_rcx52 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_rcx52, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_rcx52 for execution!\n");
	}
	//vel_rcx52 <<< dimGrid, dimBlock >>> (rcx52D, v2xD, v2yD, v2zD,  *nybtm, *nzbtm, *nxbtm, *nx2p1, *nx2p2);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, rcx52_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL rcx52_vel", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(rcx52D_meta); 
#endif    
}

void rcx41_vel (float** rcx41, int* nxtop, int* nytop, int* nztop) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(rcx41D, *rcx41, sizeof(float) * (*nztop)* (*nytop)* 4, cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, rcx41D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY rcx41_vel", cpy_time );
#endif    
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nztop)/blockSizeX + 1;
	int gridY = (*nytop)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx41, argIdx++, sizeof(cl_mem), &rcx41D);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx41, argIdx++, sizeof(cl_mem), &v1xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx41, argIdx++, sizeof(cl_mem), &v1yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx41, argIdx++, sizeof(cl_mem), &v1zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx41, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx41, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx41, argIdx++, sizeof(int), nxtop);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_rcx41 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_rcx41, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_rcx41 for execution!\n");
	}
	//vel_rcx41 <<< dimGrid, dimBlock >>> (rcx41D, v1xD, v1yD, v1zD, *nytop, *nztop, *nxtop);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, rcx41_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL rcx41_vel", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(rcx41D_meta); 
#endif
}

void rcx42_vel (float** rcx42, int* nxbtm, int* nybtm, int* nzbtm) { 
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(rcx42D, *rcx42, sizeof(float) * (*nzbtm)* (*nybtm)* 4, cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, rcx42D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY rcx42_vel", cpy_time );
#endif    
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nzbtm)/blockSizeX + 1;
	int gridY = (*nybtm)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx42, argIdx++, sizeof(cl_mem), &rcx42D);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx42, argIdx++, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx42, argIdx++, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx42, argIdx++, sizeof(cl_mem), &v2zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx42, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx42, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx42, argIdx++, sizeof(int), nxbtm);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_rcx42 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_rcx42, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_rcx42 for execution!\n");
	}
	//  vel_rcx42 <<< dimGrid, dimBlock >>> (rcx42D, v2xD, v2yD, v2zD, *nybtm, *nzbtm, *nxbtm);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, rcx42_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL rcx42_vel", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(rcx42D_meta); 
#endif
}
// --- rcy
void rcy51_vel (float** rcy51, int* nxtop, int* nytop, int* nztop, int* ny1p1,int* ny1p2) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	struct timeval start, end;
	cl_int errNum;
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(rcy51D, *rcy51, sizeof(float) * (*nztop)* (*nxtop)* 5, cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, rcy51D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY rcy51_vel", cpy_time );
#endif    
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nztop)/blockSizeX + 1;
	int gridY = (*nxtop)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy51, argIdx++, sizeof(cl_mem), &rcy51D);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy51, argIdx++, sizeof(cl_mem), &v1xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy51, argIdx++, sizeof(cl_mem), &v1yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy51, argIdx++, sizeof(cl_mem), &v1zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy51, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy51, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy51, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy51, argIdx++, sizeof(int), ny1p1);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy51, argIdx++, sizeof(int), ny1p2);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_rcy51 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_rcy51, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_rcy51 for execution!\n");
	}
	//vel_rcy51 <<< dimGrid, dimBlock >>> (rcy51D, v1xD, v1yD, v1zD, *nytop, *nztop, *nxtop, *ny1p1, *ny1p2);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, rcy51_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL rcy51_vel", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(rcy51D_meta); 
#endif
}

void rcy52_vel (float** rcy52, int* nxbtm, int* nybtm, int* nzbtm, int* ny2p1, int* ny2p2) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(rcy52D, *rcy52, sizeof(float) * (*nzbtm)* (*nxbtm)* 5, cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, rcy52D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY rcy52_vel", cpy_time );
#endif    
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nzbtm)/blockSizeX + 1;
	int gridY = (*nxbtm)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy52, argIdx++, sizeof(cl_mem), &rcy52D);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy52, argIdx++, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy52, argIdx++, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy52, argIdx++, sizeof(cl_mem), &v2zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy52, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy52, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy52, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy52, argIdx++, sizeof(int), ny2p1);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy52, argIdx++, sizeof(int), ny2p2);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_rcy52 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_rcy52, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_rcy52 for execution!\n");
	}
	// vel_rcy52 <<< dimGrid, dimBlock >>> (rcy52D, v2xD, v2yD, v2zD,  *nybtm, *nzbtm, *nxbtm, *ny2p1, *ny2p2);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, rcy52_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL rcy52_vel", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(rcy52D_meta); 
#endif    
}
void rcy41_vel (float** rcy41, int* nxtop, int* nytop, int* nztop) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(rcy41D, *rcy41, sizeof(float) * (*nztop)* (*nxtop)* 4, cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, rcy41D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY rcy41_vel", cpy_time );
#endif    
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nztop)/blockSizeX + 1;
	int gridY = (*nxtop)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy41, argIdx++, sizeof(cl_mem), &rcy41D);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy41, argIdx++, sizeof(cl_mem), &v1xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy41, argIdx++, sizeof(cl_mem), &v1yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy41, argIdx++, sizeof(cl_mem), &v1zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy41, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy41, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy41, argIdx++, sizeof(int), nxtop);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_rcy41 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_rcy41, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_rcy41 for execution!\n");
	}
	//vel_rcy41 <<< dimGrid, dimBlock >>> (rcy41D, v1xD, v1yD, v1zD, *nytop, *nztop, *nxtop);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, rcy41_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL rcy41_vel", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(rcy41D_meta); 
#endif    
}
void rcy42_vel (float** rcy42, int* nxbtm, int* nybtm, int* nzbtm) { 
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(*rcy42, rcy42D, sizeof(float) * (*nzbtm)* (*nxbtm)* 4, cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, rcy42D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY rcy42_vel", cpy_time );
#endif    
	int blockSizeX = 8;        
	int blockSizeY = 8;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nzbtm)/blockSizeX + 1;
	int gridY = (*nxbtm)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy42, argIdx++, sizeof(cl_mem), &rcy42D);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy42, argIdx++, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy42, argIdx++, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy42, argIdx++, sizeof(cl_mem), &v2zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy42, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy42, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy42, argIdx++, sizeof(int), nxbtm);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_rcy42 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_rcy42, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_rcy42 for execution!\n");
	}
	//vel_rcy42 <<< dimGrid, dimBlock >>> (rcy42D, v2xD, v2yD, v2zD, *nybtm, *nzbtm, *nxbtm);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, rcy42_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL rcy42_vel", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(rcy42D_meta);
#endif    
}
// velocity_interp functions

// sdx/y/1/2
void sdx1_vel(float* sdx1, int* nxtop, int* nytop, int* nxbtm, int* nzbtm, int* ny2p1, int* ny2p2)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;
	cl_int errNum;

	localWorkSize[0] = 1;
	localWorkSize[1] = 1;
	localWorkSize[2] = 1;
	globalWorkSize[0] = 1;
	globalWorkSize[1] = 1;
	globalWorkSize[2] = 1;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx1, argIdx++, sizeof(cl_mem), &sdx1D);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx1, argIdx++, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx1, argIdx++, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx1, argIdx++, sizeof(cl_mem), &v2zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx1, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx1, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx1, argIdx++, sizeof(int), ny2p1);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx1, argIdx++, sizeof(int), ny2p2);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_sdx1 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_sdx1, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_sdx1 for execution!\n");
	}
	//vel_sdx1 <<< 1, 1>>> (sdx1D, v2xD, v2yD, v2zD, *nxbtm, *nzbtm, *ny2p1, *ny2p2);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, sdx1_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL sdx41_vel", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(sdx1D_meta); 
#endif
#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(sdx1, sdx1D, sizeof(float) * (*nytop+6), cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sdx1D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY sdx1_vel", cpy_time );
#endif    
}

void sdy1_vel(float* sdy1, int* nxtop, int* nytop, int* nxbtm, int* nzbtm, int* nx2p1, int* nx2p2)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;

	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy1, argIdx++, sizeof(cl_mem), &sdy1D);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy1, argIdx++, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy1, argIdx++, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy1, argIdx++, sizeof(cl_mem), &v2zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy1, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy1, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy1, argIdx++, sizeof(int), nx2p1);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy1, argIdx++, sizeof(int), nx2p2);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_sdy1 arguments!\n");
	}
	localWorkSize[0] = 1;
	localWorkSize[1] = 1;
	localWorkSize[2] = 1;
	globalWorkSize[0] = 1;
	globalWorkSize[1] = 1;
	globalWorkSize[2] = 1;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_sdy1, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_sdy1 for execution!\n");
	}
	//vel_sdy1 <<< 1, 1>>> (sdy1D, v2xD, v2yD, v2zD, *nxbtm, *nzbtm, *nx2p1, *nx2p2);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, sdy1_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL sdy1_vel", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(sdy1D_meta); 
#endif
#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(sdy1, sdy1D, sizeof(float) * (*nxtop+6), cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sdy1D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY sdy1_vel", cpy_time );
#endif    
}

void sdx2_vel(float* sdx2, int* nxtop, int* nytop, int* nxbm1, int* nxbtm, int* nzbtm, int* ny2p1, int* ny2p2)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;

	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx2, argIdx++, sizeof(cl_mem), &sdx2D);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx2, argIdx++, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx2, argIdx++, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx2, argIdx++, sizeof(cl_mem), &v2zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx2, argIdx++, sizeof(int), nxbm1);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx2, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx2, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx2, argIdx++, sizeof(int), ny2p1);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdx2, argIdx++, sizeof(int), ny2p2);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_sdx2 arguments!\n");
	}
	localWorkSize[0] = 1;
	localWorkSize[1] = 1;
	localWorkSize[2] = 1;
	globalWorkSize[0] = 1;
	globalWorkSize[1] = 1;
	globalWorkSize[2] = 1;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_sdx2, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_sdx2 for execution!\n");
	}
	//vel_sdx2 <<< 1, 1>>> (sdx2D, v2xD, v2yD, v2zD, *nxbm1, *nxbtm, *nzbtm, *ny2p1, *ny2p2);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, sdx2_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL sdx2_vel", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(sdx2D_meta); 
#endif
#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(sdx2, sdx2D, sizeof(float) * (*nytop+6), cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost1, sdx2D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY sdx2_vel", cpy_time );
#endif    
}

void sdy2_vel(float* sdy2, int* nxtop, int* nytop, int* nybm1, int* nybtm, int* nxbtm, int* nzbtm, int* nx2p1, int* nx2p2)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;

	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy2, argIdx++, sizeof(cl_mem), &sdy2D);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy2, argIdx++, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy2, argIdx++, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy2, argIdx++, sizeof(cl_mem), &v2zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy2, argIdx++, sizeof(int), nybm1);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy2, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy2, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy2, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy2, argIdx++, sizeof(int), nx2p1);
	errNum |= clSetKernelArg(_cl_kernel_vel_sdy2, argIdx++, sizeof(int), nx2p2);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_sdy2 arguments!\n");
	}
	localWorkSize[0] = 1;
	localWorkSize[1] = 1;
	localWorkSize[2] = 1;
	globalWorkSize[0] = 1;
	globalWorkSize[1] = 1;
	globalWorkSize[2] = 1;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_sdy2, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_sdy2 for execution!\n");
	}
	//    vel_sdy2 <<< 1, 1>>> (sdy2D, v2xD, v2yD, v2zD, *nybm1, *nybtm, *nxbtm, *nzbtm, *nx2p1, *nx2p2);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, sdy2_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL sdy2_vel", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(sdy2D_meta); 
#endif
#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(sdy2, sdy2D, sizeof(float) * (*nxtop+6), cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sdy2D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY sdy2_vel", cpy_time );
#endif    
}

// rcx/y/1/2
void rcx1_vel(float* rcx1, int* nxtop, int* nytop, int* nxbtm, int* nzbtm, int* ny2p1, int* ny2p2)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy( rcx1D, rcx1,  sizeof(float) * (*nytop+6), cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, rcx1D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY rcx1_vel", cpy_time );
#endif    
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx1, argIdx++, sizeof(cl_mem), &rcx1D);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx1, argIdx++, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx1, argIdx++, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx1, argIdx++, sizeof(cl_mem), &v2zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx1, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx1, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx1, argIdx++, sizeof(int), ny2p1);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx1, argIdx++, sizeof(int), ny2p2);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_rcx1 arguments!\n");
	}
	localWorkSize[0] = 1;
	localWorkSize[1] = 1;
	localWorkSize[2] = 1;
	globalWorkSize[0] = 1;
	globalWorkSize[1] = 1;
	globalWorkSize[2] = 1;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_rcx1, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_rcx1 for execution!\n");
	}
	//    vel_rcx1 <<< 1, 1>>> (rcx1D, v2xD, v2yD, v2zD, *nxbtm, *nzbtm, *ny2p1, *ny2p2);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, rcx1_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL rcx1_vel", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(rcx1D_meta); 
#endif
}

void rcy1_vel(float* rcy1, int* nxtop, int* nytop, int*nxbtm, int* nzbtm,  int* nx2p1, int* nx2p2)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(rcy1D, rcy1, sizeof(float) * (*nxtop+6), cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, rcy1D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY rcy1_vel", cpy_time );
#endif    

	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy1, argIdx++, sizeof(cl_mem), &rcy1D);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy1, argIdx++, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy1, argIdx++, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy1, argIdx++, sizeof(cl_mem), &v2zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy1, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy1, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy1, argIdx++, sizeof(int), nx2p1);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy1, argIdx++, sizeof(int), nx2p2);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_rcy1 arguments!\n");
	}
	localWorkSize[0] = 1;
	localWorkSize[1] = 1;
	localWorkSize[2] = 1;
	globalWorkSize[0] = 1;
	globalWorkSize[1] = 1;
	globalWorkSize[2] = 1;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_rcy1, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_rcy1 for execution!\n");
	}
	//    vel_rcy1 <<< 1, 1>>> (rcy1D, v2xD, v2yD, v2zD, *nxbtm, *nzbtm, *nx2p1, *nx2p2);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, rcy1_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL rcy1_vel", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(rcy1D_meta); 
#endif
}

void rcx2_vel(float* rcx2, int* nxtop, int* nytop, int* nxbtm, int* nzbtm, int* nx2p1, int* nx2p2, int* ny2p1, int* ny2p2)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(rcx2D, rcx2, sizeof(float) * (*nytop+6), cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, rcx2D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY rcx2_vel", cpy_time );
#endif    

	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx2, argIdx++, sizeof(cl_mem), &rcx2D);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx2, argIdx++, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx2, argIdx++, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx2, argIdx++, sizeof(cl_mem), &v2zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx2, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx2, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx2, argIdx++, sizeof(int), nx2p1);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx2, argIdx++, sizeof(int), nx2p2);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx2, argIdx++, sizeof(int), ny2p1);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcx2, argIdx++, sizeof(int), ny2p2);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_rcx2 arguments!\n");
	}
	localWorkSize[0] = 1;
	localWorkSize[1] = 1;
	localWorkSize[2] = 1;
	globalWorkSize[0] = 1;
	globalWorkSize[1] = 1;
	globalWorkSize[2] = 1;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_rcx2, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_rcx2 for execution!\n");
	}
	//    vel_rcx2 <<< 1, 1>>> (rcx2D, v2xD, v2yD, v2zD, *nxbtm, *nzbtm, *nx2p1, *nx2p2, *ny2p1, *ny2p2);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, rcx2_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL rcx2_vel", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(rcx2D_meta); 
#endif
}

void rcy2_vel(float* rcy2, int* nxtop, int* nytop, int* nxbtm, int*  nzbtm, int* nx2p1, int* nx2p2, int* ny2p1, int* ny2p2)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(rcy2D, rcy2, sizeof(float) * (*nxtop+6), cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, rcy2D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY rcy2_vel", cpy_time );
#endif    

	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy2, argIdx++, sizeof(cl_mem), &rcy2D);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy2, argIdx++, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy2, argIdx++, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy2, argIdx++, sizeof(cl_mem), &v2zD);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy2, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy2, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy2, argIdx++, sizeof(int), nx2p1);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy2, argIdx++, sizeof(int), nx2p2);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy2, argIdx++, sizeof(int), ny2p1);
	errNum |= clSetKernelArg(_cl_kernel_vel_rcy2, argIdx++, sizeof(int), ny2p2);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_rcy2 arguments!\n");
	}
	localWorkSize[0] = 1;
	localWorkSize[1] = 1;
	localWorkSize[2] = 1;
	globalWorkSize[0] = 1;
	globalWorkSize[1] = 1;
	globalWorkSize[2] = 1;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_rcy2, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_rcy2 for execution!\n");
	}
	//vel_rcy2 <<< 1, 1>>> (rcy2D, v2xD, v2yD, v2zD, *nxbtm, *nzbtm,  *nx2p1, *nx2p2, *ny2p1, *ny2p2);
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, rcy2_vel");
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL rcy2_vel", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(rcy2D_meta); 
#endif
}


// --- Vel_interpl_3vbtm
void interpl_3vbtm_vel1(int* ny1p2, int* ny2p2, int* nz1p1, int* nyvx, 
		int* nxbm1, int* nxbtm, int* nzbtm, int* nxtop, int* nztop,  int* neighb2, float* rcx2)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int i=0;
	dim3 threadsPerBlock(32,32);
	dim3 blocks(((int)(*nyvx + 1)/32)+1 , ((int)(*nxbm1/32)+1));
	//    cudaFuncSetCacheConfig(vel1_interpl_3vbtm, cudaFuncCachePreferL1);

	globalWorkSize[0] = blocks.x*threadsPerBlock.x;
	globalWorkSize[1] = blocks.y*threadsPerBlock.y;
	globalWorkSize[2] = blocks.z*threadsPerBlock.z;
	localWorkSize[0] = threadsPerBlock.x;
	localWorkSize[1] = threadsPerBlock.y;
	localWorkSize[2] = threadsPerBlock.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel1_interpl_3vbtm, argIdx++, sizeof(int), ny1p2);
	errNum |= clSetKernelArg(_cl_kernel_vel1_interpl_3vbtm, argIdx++, sizeof(int), ny2p2);
	errNum |= clSetKernelArg(_cl_kernel_vel1_interpl_3vbtm, argIdx++, sizeof(int), nz1p1);
	errNum |= clSetKernelArg(_cl_kernel_vel1_interpl_3vbtm, argIdx++, sizeof(int), nyvx);
	errNum |= clSetKernelArg(_cl_kernel_vel1_interpl_3vbtm, argIdx++, sizeof(int), nxbm1);
	errNum |= clSetKernelArg(_cl_kernel_vel1_interpl_3vbtm, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel1_interpl_3vbtm, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel1_interpl_3vbtm, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel1_interpl_3vbtm, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_vel1_interpl_3vbtm, argIdx++, sizeof(int), neighb2);
	errNum |= clSetKernelArg(_cl_kernel_vel1_interpl_3vbtm, argIdx++, sizeof(cl_mem), &chxD);
	errNum |= clSetKernelArg(_cl_kernel_vel1_interpl_3vbtm, argIdx++, sizeof(cl_mem), &v1xD);
	errNum |= clSetKernelArg(_cl_kernel_vel1_interpl_3vbtm, argIdx++, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_vel1_interpl_3vbtm, argIdx++, sizeof(cl_mem), &rcx2D);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel1_interpl_3vbtm arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel1_interpl_3vbtm, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel1_interpl_3vbtm for execution!\n");
	}
//	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel1_interpl_3vbtm");

	//vel1_interpl_3vbtm <<<blocks,threadsPerBlock>>> (*ny1p2, *ny2p2, *nz1p1, *nyvx,
	//                               *nxbm1, *nxbtm, *nzbtm, *nxtop, *nztop, *neighb2, 
	//                              chxD, v1xD, v2xD, rcx2D);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL vel1_interpl_3vbtm", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(v1xD_meta); 
#endif
}

void interpl_3vbtm_vel3(int* ny1p2, int* nz1p1, int* nyvx1, int* nxbm1, 
		int* nxbtm, int* nybtm, int* nzbtm, int* nxtop, int* nytop, int* nztop)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int i=0;
	dim3 threadsPerBlock(32,32);
	dim3 blocks(((int)(*nxtop)/32)+1 , ((int)(*nyvx1/32)+1));
	//    cudaFuncSetCacheConfig(vel3_interpl_3vbtm, cudaFuncCachePreferL1);

	globalWorkSize[0] = blocks.x*threadsPerBlock.x;
	globalWorkSize[1] = blocks.y*threadsPerBlock.y;
	globalWorkSize[2] = blocks.z*threadsPerBlock.z;
	localWorkSize[0] = threadsPerBlock.x;
	localWorkSize[1] = threadsPerBlock.y;
	localWorkSize[2] = threadsPerBlock.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel3_interpl_3vbtm, argIdx++, sizeof(int), ny1p2);
	errNum |= clSetKernelArg(_cl_kernel_vel3_interpl_3vbtm, argIdx++, sizeof(int), nz1p1);
	errNum |= clSetKernelArg(_cl_kernel_vel3_interpl_3vbtm, argIdx++, sizeof(int), nyvx1);
	errNum |= clSetKernelArg(_cl_kernel_vel3_interpl_3vbtm, argIdx++, sizeof(int), nxbm1);
	errNum |= clSetKernelArg(_cl_kernel_vel3_interpl_3vbtm, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel3_interpl_3vbtm, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel3_interpl_3vbtm, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel3_interpl_3vbtm, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_vel3_interpl_3vbtm, argIdx++, sizeof(cl_mem), &ciyD);
	errNum |= clSetKernelArg(_cl_kernel_vel3_interpl_3vbtm, argIdx++, sizeof(cl_mem), &v1xD);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel3_interpl_3vbtm arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel3_interpl_3vbtm, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel3_interpl_3vbtm for execution!\n");
	}
//	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel3_interpl_3vbtm");

	//   vel3_interpl_3vbtm <<<blocks,threadsPerBlock>>> ( *ny1p2, *nz1p1, *nyvx1, 
	//              *nxbm1, *nxbtm, *nzbtm, *nxtop, *nztop, 
	//            ciyD, v1xD);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL vel3_interpl_3vbtm", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(v1xD_meta); 
#endif
}

void interpl_3vbtm_vel4(int* nx1p2, int* ny2p2, int* nz1p1, int* nxvy, int* nybm1, 
		int* nxbtm, int* nybtm, int* nzbtm, int* nxtop, int* nytop, int* nztop)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int i=0;
	dim3 threadsPerBlock(32,32);
	dim3 blocks(((int)(*nxvy+1)/32)+1 , ((int)(*nybm1/32)+1));
	//    cudaFuncSetCacheConfig(vel4_interpl_3vbtm, cudaFuncCachePreferL1);

	globalWorkSize[0] = blocks.x*threadsPerBlock.x;
	globalWorkSize[1] = blocks.y*threadsPerBlock.y;
	globalWorkSize[2] = blocks.z*threadsPerBlock.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel4_interpl_3vbtm, argIdx++, sizeof(int), nx1p2);
	errNum |= clSetKernelArg(_cl_kernel_vel4_interpl_3vbtm, argIdx++, sizeof(int), ny2p2);
	errNum |= clSetKernelArg(_cl_kernel_vel4_interpl_3vbtm, argIdx++, sizeof(int), nz1p1);
	errNum |= clSetKernelArg(_cl_kernel_vel4_interpl_3vbtm, argIdx++, sizeof(int), nxvy);
	errNum |= clSetKernelArg(_cl_kernel_vel4_interpl_3vbtm, argIdx++, sizeof(int), nybm1);
	errNum |= clSetKernelArg(_cl_kernel_vel4_interpl_3vbtm, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel4_interpl_3vbtm, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel4_interpl_3vbtm, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel4_interpl_3vbtm, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel4_interpl_3vbtm, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel4_interpl_3vbtm, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_vel4_interpl_3vbtm, argIdx++, sizeof(cl_mem), &chyD);
	errNum |= clSetKernelArg(_cl_kernel_vel4_interpl_3vbtm, argIdx++, sizeof(cl_mem), &v1yD);
	errNum |= clSetKernelArg(_cl_kernel_vel4_interpl_3vbtm, argIdx++, sizeof(cl_mem), &v2yD);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel4_interpl_3vbtm arguments!\n");
	}
	localWorkSize[0] = threadsPerBlock.x;
	localWorkSize[1] = threadsPerBlock.y;
	localWorkSize[2] = threadsPerBlock.z;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel4_interpl_3vbtm, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel4_interpl_3vbtm for execution!\n");
	}
//	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel4_interpl_3vbtm");

	//vel4_interpl_3vbtm <<<blocks,threadsPerBlock>>> ( *nx1p2, *ny2p2, *nz1p1, *nxvy, 
	//                *nybm1, *nxbtm, *nybtm, *nzbtm, *nxtop, *nytop, *nztop, 
	//              chyD, v1yD, v2yD);

	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL vel4_interpl_3vbtm", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(v1yD_meta); 
#endif
}

void interpl_3vbtm_vel5(int* nx1p2, int* nx2p2, int* nz1p1, int* nxvy, int* nybm1, 
		int* nxbtm, int* nybtm, int* nzbtm, int* nxtop, int* nytop, int* nztop)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int i=0;
	dim3 threadsPerBlock(512);
	dim3 blocks(((int)(*nxvy+1)/512)+1);
	//cudaFuncSetCacheConfig(vel5_interpl_3vbtm, cudaFuncCachePreferL1);

	globalWorkSize[0] = blocks.x*threadsPerBlock.x;
	globalWorkSize[1] = blocks.y*threadsPerBlock.y;
	globalWorkSize[2] = blocks.z*threadsPerBlock.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel5_interpl_3vbtm, argIdx++, sizeof(int), nx1p2);
	errNum |= clSetKernelArg(_cl_kernel_vel5_interpl_3vbtm, argIdx++, sizeof(int), nx2p2);
	errNum |= clSetKernelArg(_cl_kernel_vel5_interpl_3vbtm, argIdx++, sizeof(int), nz1p1);
	errNum |= clSetKernelArg(_cl_kernel_vel5_interpl_3vbtm, argIdx++, sizeof(int), nxvy);
	errNum |= clSetKernelArg(_cl_kernel_vel5_interpl_3vbtm, argIdx++, sizeof(int), nybm1);
	errNum |= clSetKernelArg(_cl_kernel_vel5_interpl_3vbtm, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel5_interpl_3vbtm, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel5_interpl_3vbtm, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel5_interpl_3vbtm, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel5_interpl_3vbtm, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel5_interpl_3vbtm, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_vel5_interpl_3vbtm, argIdx++, sizeof(cl_mem), &chyD);
	errNum |= clSetKernelArg(_cl_kernel_vel5_interpl_3vbtm, argIdx++, sizeof(cl_mem), &v1yD);
	errNum |= clSetKernelArg(_cl_kernel_vel5_interpl_3vbtm, argIdx++, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_vel5_interpl_3vbtm, argIdx++, sizeof(cl_mem), &rcy2D);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel5_interpl_3vbtm arguments!\n");
	}
	localWorkSize[0] = threadsPerBlock.x;
	localWorkSize[1] = threadsPerBlock.y;
	localWorkSize[2] = threadsPerBlock.z;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel5_interpl_3vbtm, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel5_interpl_3vbtm for execution!\n");
	}
//	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel5_interpl_3vbtm");

	//vel5_interpl_3vbtm <<<blocks,threadsPerBlock>>> ( *nx1p2, *nx2p2, *nz1p1, *nxvy, 
	//              *nybm1, *nxbtm, *nybtm, *nzbtm, *nxtop, *nytop, *nztop, 
	//            chyD, v1yD, v2yD, rcy2D);

	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL vel5_interpl_3vbtm", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(v1yD_meta); 
#endif
}

void interpl_3vbtm_vel6(int* nx1p2,  int* nz1p1, int* nxvy1, int* nybm1, 
		int* nxbtm, int* nybtm, int* nzbtm, int* nxtop, int* nytop, int* nztop)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int i=0;
	dim3 threadsPerBlock(32,32);
	dim3 blocks(((int)(*nytop)/32)+1,((int)((*nxvy1)/32)+1));
	//cudaFuncSetCacheConfig(vel6_interpl_3vbtm, cudaFuncCachePreferL1);

	globalWorkSize[0] = blocks.x*threadsPerBlock.x;
	globalWorkSize[1] = blocks.y*threadsPerBlock.y;
	globalWorkSize[2] = blocks.z*threadsPerBlock.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel6_interpl_3vbtm, argIdx++, sizeof(int), nx1p2);
	errNum |= clSetKernelArg(_cl_kernel_vel6_interpl_3vbtm, argIdx++, sizeof(int), nz1p1);
	errNum |= clSetKernelArg(_cl_kernel_vel6_interpl_3vbtm, argIdx++, sizeof(int), nxvy1);
	errNum |= clSetKernelArg(_cl_kernel_vel6_interpl_3vbtm, argIdx++, sizeof(int), nybm1);
	errNum |= clSetKernelArg(_cl_kernel_vel6_interpl_3vbtm, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel6_interpl_3vbtm, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel6_interpl_3vbtm, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel6_interpl_3vbtm, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel6_interpl_3vbtm, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel6_interpl_3vbtm, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_vel6_interpl_3vbtm, argIdx++, sizeof(cl_mem), &cixD);
	errNum |= clSetKernelArg(_cl_kernel_vel6_interpl_3vbtm, argIdx++, sizeof(cl_mem), &v1yD);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel6_interpl_3vbtm arguments!\n");
	}
	localWorkSize[0] = threadsPerBlock.x;
	localWorkSize[1] = threadsPerBlock.y;
	localWorkSize[2] = threadsPerBlock.z;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel6_interpl_3vbtm, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel6_interpl_3vbtm for execution!\n");
	}
//	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel6_interpl_3vbtm");

	//vel6_interpl_3vbtm <<<blocks,threadsPerBlock>>>( *nx1p2, *nz1p1, *nxvy1, 
	//              *nybm1, *nxbtm, *nybtm, *nzbtm, *nxtop, *nytop,*nztop, 
	//            cixD, v1yD);

	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL vel6_interpl_3vbtm", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(v1yD_meta); 
#endif
}

void interpl_3vbtm_vel7 (int* nxbtm, int* nybtm, int* nzbtm, int* nxtop, int* nytop, int* nztop, float* sdx1)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int i=0;
	dim3 threadsPerBlock(512);
	dim3 blocks(((int)(*nybtm)/512)+1);
	//cudaFuncSetCacheConfig(vel7_interpl_3vbtm, cudaFuncCachePreferL1);

	globalWorkSize[0] = blocks.x*threadsPerBlock.x;
	globalWorkSize[1] = blocks.y*threadsPerBlock.y;
	globalWorkSize[2] = blocks.z*threadsPerBlock.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel7_interpl_3vbtm, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel7_interpl_3vbtm, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel7_interpl_3vbtm, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel7_interpl_3vbtm, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel7_interpl_3vbtm, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel7_interpl_3vbtm, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_vel7_interpl_3vbtm, argIdx++, sizeof(cl_mem), &ciyD);
	errNum |= clSetKernelArg(_cl_kernel_vel7_interpl_3vbtm, argIdx++, sizeof(cl_mem), &sdx1D);
	errNum |= clSetKernelArg(_cl_kernel_vel7_interpl_3vbtm, argIdx++, sizeof(cl_mem), &rcx1D);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel7_interpl_3vbtm arguments!\n");
	}
	localWorkSize[0] = threadsPerBlock.x;
	localWorkSize[1] = threadsPerBlock.y;
	localWorkSize[2] = threadsPerBlock.z;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel7_interpl_3vbtm, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel7_interpl_3vbtm for execution!\n");
	}
//	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel7_interpl_3vbtm");

	//vel7_interpl_3vbtm <<<blocks,threadsPerBlock>>>( *nxbtm, *nybtm, *nzbtm, *nxtop, *nytop, *nztop, 
	//          ciyD, sdx1D, rcx1D);

	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL vel7_interpl_3vbtm", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(sdx1D_meta); 
#endif
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(sdx1, sdx1D, sizeof(float) * (*nytop+6), cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost3, sdx2D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY vel7_interpl_3vbtm", cpy_time );
#endif    

}
void interpl_3vbtm_vel8 (int* nxbtm, int* nybtm, int* nzbtm, int* nxtop, int* nytop, int* nztop, float* sdx2)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int i=0;
	dim3 threadsPerBlock(512);
	dim3 blocks(((int)(*nybtm)/512)+1);
	//cudaFuncSetCacheConfig(vel8_interpl_3vbtm, cudaFuncCachePreferL1);

	globalWorkSize[0] = blocks.x*threadsPerBlock.x;
	globalWorkSize[1] = blocks.y*threadsPerBlock.y;
	globalWorkSize[2] = blocks.z*threadsPerBlock.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel8_interpl_3vbtm, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel8_interpl_3vbtm, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel8_interpl_3vbtm, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel8_interpl_3vbtm, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel8_interpl_3vbtm, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel8_interpl_3vbtm, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_vel8_interpl_3vbtm, argIdx++, sizeof(cl_mem), &ciyD);
	errNum |= clSetKernelArg(_cl_kernel_vel8_interpl_3vbtm, argIdx++, sizeof(cl_mem), &sdx2D);
	errNum |= clSetKernelArg(_cl_kernel_vel8_interpl_3vbtm, argIdx++, sizeof(cl_mem), &rcx2D);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel8_interpl_3vbtm arguments!\n");
	}
	localWorkSize[0] = threadsPerBlock.x;
	localWorkSize[1] = threadsPerBlock.y;
	localWorkSize[2] = threadsPerBlock.z;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel8_interpl_3vbtm, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel8_interpl_3vbtm for execution!\n");
	}
//	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel8_interpl_3vbtm");

	//vel8_interpl_3vbtm <<<blocks,threadsPerBlock>>> (*nxbtm, *nybtm, *nzbtm, 
	//                                          *nxtop, *nytop, *nztop, 
	//                                        ciyD, sdx2D, rcx2D);

	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL vel8_interpl_3vbtm", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(sdx2D_meta); 
#endif
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(sdx2, sdx2D, sizeof(float) * (*nytop+6), cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost2, sdx2D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY vel8_interpl_3vbtm", cpy_time );
#endif    

}

void interpl_3vbtm_vel9(int* nx1p2, int* ny2p1, int* nz1p1, int* nxvy, int* nybm1, 
		int* nxbtm, int* nybtm, int* nzbtm, int* nxtop, int* nytop, int* nztop, int* neighb4)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int i=0;
	dim3 threadsPerBlock(32,32);
	dim3 blocks(((int)(*nxvy+1)/32)+1,((int)((*nybtm)/32)+1));
	//cudaFuncSetCacheConfig(vel9_interpl_3vbtm, cudaFuncCachePreferL1);

	globalWorkSize[0] = blocks.x*threadsPerBlock.x;
	globalWorkSize[1] = blocks.y*threadsPerBlock.y;
	globalWorkSize[2] = blocks.z*threadsPerBlock.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel9_interpl_3vbtm, argIdx++, sizeof(int), nz1p1);
	errNum |= clSetKernelArg(_cl_kernel_vel9_interpl_3vbtm, argIdx++, sizeof(int), nx1p2);
	errNum |= clSetKernelArg(_cl_kernel_vel9_interpl_3vbtm, argIdx++, sizeof(int), ny2p1);
	errNum |= clSetKernelArg(_cl_kernel_vel9_interpl_3vbtm, argIdx++, sizeof(int), nxvy);
	errNum |= clSetKernelArg(_cl_kernel_vel9_interpl_3vbtm, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel9_interpl_3vbtm, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel9_interpl_3vbtm, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel9_interpl_3vbtm, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel9_interpl_3vbtm, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel9_interpl_3vbtm, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_vel9_interpl_3vbtm, argIdx++, sizeof(int), neighb4);
	errNum |= clSetKernelArg(_cl_kernel_vel9_interpl_3vbtm, argIdx++, sizeof(cl_mem), &ciyD);
	errNum |= clSetKernelArg(_cl_kernel_vel9_interpl_3vbtm, argIdx++, sizeof(cl_mem), &rcy1D);
	errNum |= clSetKernelArg(_cl_kernel_vel9_interpl_3vbtm, argIdx++, sizeof(cl_mem), &rcy2D);
	errNum |= clSetKernelArg(_cl_kernel_vel9_interpl_3vbtm, argIdx++, sizeof(cl_mem), &v1zD);
	errNum |= clSetKernelArg(_cl_kernel_vel9_interpl_3vbtm, argIdx++, sizeof(cl_mem), &v2zD);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel9_interpl_3vbtm arguments!\n");
	}
	localWorkSize[0] = threadsPerBlock.x;
	localWorkSize[1] = threadsPerBlock.y;
	localWorkSize[2] = threadsPerBlock.z;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel9_interpl_3vbtm, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel9_interpl_3vbtm for execution!\n");
	}
//	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel9_interpl_3vbtm");

	//vel9_interpl_3vbtm <<<blocks,threadsPerBlock>>> ( *nz1p1, *nx1p2, *ny2p1, *nxvy, 
	//          *nxbtm, *nybtm, *nzbtm, *nxtop, *nytop, *nztop, 
	//        *neighb4, ciyD, rcy1D, rcy2D, v1zD, v2zD);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL vel9_interpl_3vbtm", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(v1zD_meta); 
#endif
}

void interpl_3vbtm_vel11(int* nx1p2, int* nx2p1, int* ny1p1, int* nz1p1, int* nxvy1, 
		int* nxbtm, int* nybtm, int* nzbtm, int* nxtop, int* nytop, int* nztop)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int i=0;
	dim3 threadsPerBlock(32,32);
	dim3 blocks(((int)(*ny1p1+1)/32)+1,((int)((*nxvy1)/32)+1));
	//    cudaFuncSetCacheConfig(vel11_interpl_3vbtm, cudaFuncCachePreferL1);

	globalWorkSize[0] = blocks.x*threadsPerBlock.x;
	globalWorkSize[1] = blocks.y*threadsPerBlock.y;
	globalWorkSize[2] = blocks.z*threadsPerBlock.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel11_interpl_3vbtm, argIdx++, sizeof(int), nx1p2);
	errNum |= clSetKernelArg(_cl_kernel_vel11_interpl_3vbtm, argIdx++, sizeof(int), nx2p1);
	errNum |= clSetKernelArg(_cl_kernel_vel11_interpl_3vbtm, argIdx++, sizeof(int), ny1p1);
	errNum |= clSetKernelArg(_cl_kernel_vel11_interpl_3vbtm, argIdx++, sizeof(int), nz1p1);
	errNum |= clSetKernelArg(_cl_kernel_vel11_interpl_3vbtm, argIdx++, sizeof(int), nxvy1);
	errNum |= clSetKernelArg(_cl_kernel_vel11_interpl_3vbtm, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel11_interpl_3vbtm, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel11_interpl_3vbtm, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel11_interpl_3vbtm, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel11_interpl_3vbtm, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel11_interpl_3vbtm, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_vel11_interpl_3vbtm, argIdx++, sizeof(cl_mem), &cixD);
	errNum |= clSetKernelArg(_cl_kernel_vel11_interpl_3vbtm, argIdx++, sizeof(cl_mem), &sdx1D);
	errNum |= clSetKernelArg(_cl_kernel_vel11_interpl_3vbtm, argIdx++, sizeof(cl_mem), &sdx2D);
	errNum |= clSetKernelArg(_cl_kernel_vel11_interpl_3vbtm, argIdx++, sizeof(cl_mem), &v1zD);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel11_interpl_3vbtm arguments!\n");
	}
	localWorkSize[0] = threadsPerBlock.x;
	localWorkSize[1] = threadsPerBlock.y;
	localWorkSize[2] = threadsPerBlock.z;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel11_interpl_3vbtm, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel11_interpl_3vbtm for execution!\n");
	}
//	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel11_interpl_3vbtm");

	//  vel11_interpl_3vbtm <<<blocks,threadsPerBlock>>>  (*nx1p2, *nx2p1, *ny1p1, *nz1p1, *nxvy1,
	//	*nxbtm, *nybtm, *nzbtm, *nxtop, *nytop, *nztop, 
	//          cixD, sdx1D, sdx2D, v1zD);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL vel11_interpl_3vbtm", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(v1zD_meta); 
#endif
}

void interpl_3vbtm_vel13(int* nxbtm, int* nybtm, int* nzbtm, int* nxtop, int* nytop, int* nztop)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int i=0;
	dim3 threadsPerBlock(32,32);
	dim3 blocks(((int)(*nybtm)/32)+1,((int)((*nxbtm)/32)+1));
	//cudaFuncSetCacheConfig(vel13_interpl_3vbtm, cudaFuncCachePreferL1);

	globalWorkSize[0] = blocks.x*threadsPerBlock.x;
	globalWorkSize[1] = blocks.y*threadsPerBlock.y;
	globalWorkSize[2] = blocks.z*threadsPerBlock.z;
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel13_interpl_3vbtm, argIdx++, sizeof(cl_mem), &v1xD);
	errNum |= clSetKernelArg(_cl_kernel_vel13_interpl_3vbtm, argIdx++, sizeof(cl_mem), &v2xD);
	errNum |= clSetKernelArg(_cl_kernel_vel13_interpl_3vbtm, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel13_interpl_3vbtm, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel13_interpl_3vbtm, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel13_interpl_3vbtm, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel13_interpl_3vbtm, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel13_interpl_3vbtm, argIdx++, sizeof(int), nztop);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel13_interpl_3vbtm arguments!\n");
	}
	localWorkSize[0] = threadsPerBlock.x;
	localWorkSize[1] = threadsPerBlock.y;
	localWorkSize[2] = threadsPerBlock.z;
	record_time(&tstart);
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel13_interpl_3vbtm, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel13_interpl_3vbtm for execution!\n");
	}
//	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel13_interpl_3vbtm");

	// vel13_interpl_3vbtm <<<blocks,threadsPerBlock>>> (v1xD, v2xD,
	//				    *nxbtm, *nybtm, *nzbtm,
	//                                  *nxtop, *nytop, *nztop);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL vel13_interpl_3vbtm", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(v2xD_meta); 
#endif
}

void interpl_3vbtm_vel14(int* nxbtm, int* nybtm, int* nzbtm, int* nxtop, int* nytop, int* nztop)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int i=0;
	dim3 threadsPerBlock(32,32);
	dim3 blocks(((int)(*nybtm)/32)+1,((int)((*nxbtm)/32)+1));
	//cudaFuncSetCacheConfig(vel14_interpl_3vbtm, cudaFuncCachePreferL1);

	globalWorkSize[0] = blocks.x*threadsPerBlock.x;
	globalWorkSize[1] = blocks.y*threadsPerBlock.y;
	globalWorkSize[2] = blocks.z*threadsPerBlock.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel14_interpl_3vbtm, argIdx++, sizeof(cl_mem), &v1yD);
	errNum |= clSetKernelArg(_cl_kernel_vel14_interpl_3vbtm, argIdx++, sizeof(cl_mem), &v2yD);
	errNum |= clSetKernelArg(_cl_kernel_vel14_interpl_3vbtm, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel14_interpl_3vbtm, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel14_interpl_3vbtm, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel14_interpl_3vbtm, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel14_interpl_3vbtm, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel14_interpl_3vbtm, argIdx++, sizeof(int), nztop);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel14_interpl_3vbtm arguments!\n");
	}
	localWorkSize[0] = threadsPerBlock.x;
	localWorkSize[1] = threadsPerBlock.y;
	localWorkSize[2] = threadsPerBlock.z;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel14_interpl_3vbtm, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel14_interpl_3vbtm for execution!\n");
	}
//	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel14_interpl_3vbtm");

	//vel14_interpl_3vbtm <<<blocks,threadsPerBlock>>> (v1yD, v2yD,
	//			    *nxbtm, *nybtm, *nzbtm,
	//                              *nxtop, *nytop, *nztop);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL vel14_interpl_3vbtm", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(v2yD_meta); 
#endif
}

void interpl_3vbtm_vel15(int* nxbtm, int* nybtm, int* nzbtm, int* nxtop, int* nytop, int* nztop)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int i=0;
	dim3 threadsPerBlock(32,32);
	dim3 blocks(((int)(*nybtm)/32)+1,((int)((*nxbtm)/32)+1));
	//    cudaFuncSetCacheConfig(vel15_interpl_3vbtm, cudaFuncCachePreferL1);

	globalWorkSize[0] = blocks.x*threadsPerBlock.x;
	globalWorkSize[1] = blocks.y*threadsPerBlock.y;
	globalWorkSize[2] = blocks.z*threadsPerBlock.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel15_interpl_3vbtm, argIdx++, sizeof(cl_mem), &v1zD);
	errNum |= clSetKernelArg(_cl_kernel_vel15_interpl_3vbtm, argIdx++, sizeof(cl_mem), &v2zD);
	errNum |= clSetKernelArg(_cl_kernel_vel15_interpl_3vbtm, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel15_interpl_3vbtm, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel15_interpl_3vbtm, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel15_interpl_3vbtm, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel15_interpl_3vbtm, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel15_interpl_3vbtm, argIdx++, sizeof(int), nztop);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel15_interpl_3vbtm arguments!\n");
	}
	localWorkSize[0] = threadsPerBlock.x;
	localWorkSize[1] = threadsPerBlock.y;
	localWorkSize[2] = threadsPerBlock.z;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel15_interpl_3vbtm, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel15_interpl_3vbtm for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel15_interpl_3vbtm");

	//  vel15_interpl_3vbtm <<<blocks,threadsPerBlock>>> (v1zD, v2zD,
	//			    *nxbtm, *nybtm, *nzbtm,
	//                              *nxtop, *nytop, *nztop);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL vel15_interpl_3vbtm", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(v2zD_meta); 
#endif
}

/*
   void vxy_image_layer_vel1(float* v1x, float* v1y, float* v1z,
   float* v2x, float* v2y, float* v2z,
   int neighb1, int neighb2, int neighb3, int neighb4,
   int* nxtm1, int* nytm1, int* nxtop, int* nytop, int* nztop,
   int* nxbtm, int* nybtm ,int* nzbtm)
   {

   vel_vxy_image_layer1 <<<1,1>>>(v1xD, v1yD, v1zD, nd1_velD, dxi1D, dyi1D, dzh1D,
 *nxtm1, *nytm1, *nxtop, *nytop, *nztop, 
 neighb1, neighb2, neighb3, neighb4);
 cl_int errNum;
 errNum = cudaThreadSynchronize();
 CHECK_ERROR(errNum, "vxy_image_layer kernel error");
#if LOGGING_ENABLED == 1    
logger.logGPUArrInfo(v1xD_meta); 
logger.logGPUArrInfo(v1yD_meta); 
#endif
#if USE_MPIX == 0 && VALIDATE_MODE == 0
//for inner_I
errNum = cudaMemcpy(v1x, v1xD,  sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), cudaMemcpyDeviceToHost);
CHECK_ERROR(errNum, "outputDataCopyDeviceToHost1, v1x");
errNum = cudaMemcpy(v1y, v1yD,  sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), cudaMemcpyDeviceToHost);
CHECK_ERROR(errNum, "outputDataCopyDeviceToHost1, v1y");
errNum = cudaMemcpy(v1z, v1zD,  sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), cudaMemcpyDeviceToHost);
CHECK_ERROR(errNum, "outputDataCopyDeviceToHost1, v1z");

//for inner_II
errNum = cudaMemcpy(v2x, v2xD,  sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3), cudaMemcpyDeviceToHost);
CHECK_ERROR(errNum, "outputDataCopyDeviceToHost1, v2x");
errNum = cudaMemcpy(v2y, v2yD,  sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3), cudaMemcpyDeviceToHost);
CHECK_ERROR(errNum, "outputDataCopyDeviceToHost1, v2y");
errNum = cudaMemcpy(v2z, v2zD,  sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3), cudaMemcpyDeviceToHost);
CHECK_ERROR(errNum, "outputDataCopyDeviceToHost1, vzz");
#endif

}
*/
void vxy_image_layer_vel1(int* nd1_vel, int i, float dzdx, int nxbtm, int nybtm, int nzbtm, 
		int nxtop, int nytop, int nztop)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	dim3 threadsPerBlock(1024);
	dim3 blocks(((int)(nd1_vel[5]-nd1_vel[0]+1)/1024)+1);
	//cudaFuncSetCacheConfig(vel1_vxy_image_layer, cudaFuncCachePreferL1);

	globalWorkSize[0] = blocks.x*threadsPerBlock.x;
	globalWorkSize[1] = blocks.y*threadsPerBlock.y;
	globalWorkSize[2] = blocks.z*threadsPerBlock.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel1_vxy_image_layer, argIdx++, sizeof(cl_mem), &v1xD);
	errNum |= clSetKernelArg(_cl_kernel_vel1_vxy_image_layer, argIdx++, sizeof(cl_mem), &v1zD);
	errNum |= clSetKernelArg(_cl_kernel_vel1_vxy_image_layer, argIdx++, sizeof(cl_mem), &nd1_velD);
	errNum |= clSetKernelArg(_cl_kernel_vel1_vxy_image_layer, argIdx++, sizeof(cl_mem), &dxi1D);
	errNum |= clSetKernelArg(_cl_kernel_vel1_vxy_image_layer, argIdx++, sizeof(cl_mem), &dzh1D);
	errNum |= clSetKernelArg(_cl_kernel_vel1_vxy_image_layer, argIdx++, sizeof(int), &i);
	errNum |= clSetKernelArg(_cl_kernel_vel1_vxy_image_layer, argIdx++, sizeof(float), &dzdx);
	errNum |= clSetKernelArg(_cl_kernel_vel1_vxy_image_layer, argIdx++, sizeof(int), &nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel1_vxy_image_layer, argIdx++, sizeof(int), &nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel1_vxy_image_layer, argIdx++, sizeof(int), &nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel1_vxy_image_layer, argIdx++, sizeof(int), &nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel1_vxy_image_layer, argIdx++, sizeof(int), &nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel1_vxy_image_layer, argIdx++, sizeof(int), &nztop);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel1_vxy_image_layer arguments!\n");
	}
	localWorkSize[0] = threadsPerBlock.x;
	localWorkSize[1] = threadsPerBlock.y;
	localWorkSize[2] = threadsPerBlock.z;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel1_vxy_image_layer, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel1_vxy_image_layer for execution!\n");
	}
//	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel1_vxy_image_layer");

	//vel1_vxy_image_layer <<<blocks,threadsPerBlock>>> (v1xD, v1zD,
	//                              nd1_velD, dxi1D, dzh1D, 
	//                            i, dzdx,
	//			    nxbtm, nybtm, nzbtm,
	//                          nxtop, nytop, nztop);

	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL vel1_vxy_image_layer", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(v1xD_meta); 
#endif
}

void vxy_image_layer_vel2(int* nd1_vel, float* v1x, int* iix, float* dzdt, int* nxbtm, int* nybtm, int* nzbtm, int* nxtop, int* nytop, int* nztop)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int i=0;
	dim3 threadsPerBlock(32,32);
	dim3 blocks(((int)(nd1_vel[5]-nd1_vel[0]+1)/32)+1, 
			((int)(*iix-nd1_vel[6]+1)/32)+1);
	//cudaFuncSetCacheConfig(vel2_vxy_image_layer, cudaFuncCachePreferL1);

	globalWorkSize[0] = blocks.x*threadsPerBlock.x;
	globalWorkSize[1] = blocks.y*threadsPerBlock.y;
	globalWorkSize[2] = blocks.z*threadsPerBlock.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel2_vxy_image_layer, argIdx++, sizeof(cl_mem), &v1xD);
	errNum |= clSetKernelArg(_cl_kernel_vel2_vxy_image_layer, argIdx++, sizeof(cl_mem), &v1zD);
	errNum |= clSetKernelArg(_cl_kernel_vel2_vxy_image_layer, argIdx++, sizeof(cl_mem), &nd1_velD);
	errNum |= clSetKernelArg(_cl_kernel_vel2_vxy_image_layer, argIdx++, sizeof(cl_mem), &dxi1D);
	errNum |= clSetKernelArg(_cl_kernel_vel2_vxy_image_layer, argIdx++, sizeof(cl_mem), &dzh1D);
	errNum |= clSetKernelArg(_cl_kernel_vel2_vxy_image_layer, argIdx++, sizeof(int), iix);
	errNum |= clSetKernelArg(_cl_kernel_vel2_vxy_image_layer, argIdx++, sizeof(float), dzdt);
	errNum |= clSetKernelArg(_cl_kernel_vel2_vxy_image_layer, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel2_vxy_image_layer, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel2_vxy_image_layer, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel2_vxy_image_layer, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel2_vxy_image_layer, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel2_vxy_image_layer, argIdx++, sizeof(int), nztop);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel2_vxy_image_layer arguments!\n");
	}
	localWorkSize[0] = threadsPerBlock.x;
	localWorkSize[1] = threadsPerBlock.y;
	localWorkSize[2] = threadsPerBlock.z;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel2_vxy_image_layer, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel2_vxy_image_layer for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel2_vxy_image_layer");

	//    vel2_vxy_image_layer <<<blocks,threadsPerBlock>>> (v1xD, v1zD, nd1_velD,
	//				dxi1D, dzh1D,
	//				*iix, *dzdt,
	//				*nxbtm, *nybtm, *nzbtm, 
	//				*nxtop, *nytop, *nztop);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL vel2_vxy_image_layer", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(v1xD_meta); 
#endif
#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(v1x, v1xD,  sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "outputDataCopyDeviceToHost1, v1x");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY vel2_vxy_image_layer", cpy_time );
#endif
}

void vxy_image_layer_vel3(int* nd1_vel, int* j, float* dzdy, int* nxbtm, int* nybtm, int* nzbtm, int* nxtop, int* nytop, int* nztop)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int i=0;
	dim3 threadsPerBlock(1024);
	dim3 blocks(((int)(nd1_vel[11]-nd1_vel[6]+1)/1024)+1);
	//cudaFuncSetCacheConfig(vel3_vxy_image_layer, cudaFuncCachePreferL1);

	globalWorkSize[0] = blocks.x*threadsPerBlock.x;
	globalWorkSize[1] = blocks.y*threadsPerBlock.y;
	globalWorkSize[2] = blocks.z*threadsPerBlock.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel3_vxy_image_layer, argIdx++, sizeof(cl_mem), &v1yD);
	errNum |= clSetKernelArg(_cl_kernel_vel3_vxy_image_layer, argIdx++, sizeof(cl_mem), &v1zD);
	errNum |= clSetKernelArg(_cl_kernel_vel3_vxy_image_layer, argIdx++, sizeof(cl_mem), &nd1_velD);
	errNum |= clSetKernelArg(_cl_kernel_vel3_vxy_image_layer, argIdx++, sizeof(cl_mem), &dyi1D);
	errNum |= clSetKernelArg(_cl_kernel_vel3_vxy_image_layer, argIdx++, sizeof(cl_mem), &dzh1D);
	errNum |= clSetKernelArg(_cl_kernel_vel3_vxy_image_layer, argIdx++, sizeof(int), j);
	errNum |= clSetKernelArg(_cl_kernel_vel3_vxy_image_layer, argIdx++, sizeof(float), dzdy);
	errNum |= clSetKernelArg(_cl_kernel_vel3_vxy_image_layer, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel3_vxy_image_layer, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel3_vxy_image_layer, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel3_vxy_image_layer, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel3_vxy_image_layer, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel3_vxy_image_layer, argIdx++, sizeof(int), nztop);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel3_vxy_image_layer arguments!\n");
	}
	localWorkSize[0] = threadsPerBlock.x;
	localWorkSize[1] = threadsPerBlock.y;
	localWorkSize[2] = threadsPerBlock.z;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel3_vxy_image_layer, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel3_vxy_image_layer for execution!\n");
	}
//	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel3_vxy_image_layer");

	// vel3_vxy_image_layer <<<blocks,threadsPerBlock>>> (v1yD, v1zD,
	//                               nd1_velD, dyi1D, dzh1D, 
	//                             *j, *dzdy,
	//		    *nxbtm, *nybtm, *nzbtm,
	//                          *nxtop, *nytop, *nztop);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL vel3_vxy_image_layer", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(v1yD_meta); 
#endif
}

void vxy_image_layer_vel4(int* nd1_vel, float* v1y, int* jjy, float* dzdt, int* nxbtm, int* nybtm, int* nzbtm, int* nxtop, int* nytop, int* nztop)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int i=0;
	dim3 threadsPerBlock(32,32);
	dim3 blocks(((int)(*jjy - nd1_vel[0]+1)/32)+1, 
			((int)(nd1_vel[11]-nd1_vel[6]+1)/32)+1);
	//    cudaFuncSetCacheConfig(vel4_vxy_image_layer, cudaFuncCachePreferL1);

	globalWorkSize[0] = blocks.x*threadsPerBlock.x;
	globalWorkSize[1] = blocks.y*threadsPerBlock.y;
	globalWorkSize[2] = blocks.z*threadsPerBlock.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel4_vxy_image_layer, argIdx++, sizeof(cl_mem), &v1yD);
	errNum |= clSetKernelArg(_cl_kernel_vel4_vxy_image_layer, argIdx++, sizeof(cl_mem), &v1zD);
	errNum |= clSetKernelArg(_cl_kernel_vel4_vxy_image_layer, argIdx++, sizeof(cl_mem), &nd1_velD);
	errNum |= clSetKernelArg(_cl_kernel_vel4_vxy_image_layer, argIdx++, sizeof(cl_mem), &dyi1D);
	errNum |= clSetKernelArg(_cl_kernel_vel4_vxy_image_layer, argIdx++, sizeof(cl_mem), &dzh1D);
	errNum |= clSetKernelArg(_cl_kernel_vel4_vxy_image_layer, argIdx++, sizeof(int), jjy);
	errNum |= clSetKernelArg(_cl_kernel_vel4_vxy_image_layer, argIdx++, sizeof(float), dzdt);
	errNum |= clSetKernelArg(_cl_kernel_vel4_vxy_image_layer, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel4_vxy_image_layer, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel4_vxy_image_layer, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel4_vxy_image_layer, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel4_vxy_image_layer, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel4_vxy_image_layer, argIdx++, sizeof(int), nztop);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel4_vxy_image_layer arguments!\n");
	}
	localWorkSize[0] = threadsPerBlock.x;
	localWorkSize[1] = threadsPerBlock.y;
	localWorkSize[2] = threadsPerBlock.z;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel4_vxy_image_layer, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel4_vxy_image_layer for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel4_vxy_image_layer");

	//   vel4_vxy_image_layer <<<blocks,threadsPerBlock>>> (v1yD, v1zD, nd1_velD,
	//				dyi1D, dzh1D,
	//				*jjy, *dzdt,
	//				*nxbtm, *nybtm, *nzbtm, 
	//				*nxtop, *nytop, *nztop);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL vel4_vxy_image_layer", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(v1yD_meta); 
#endif
#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(v1y, v1yD,  sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "outputDataCopyDeviceToHost1, v1y");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY vel4_vxy_image_layer", cpy_time );
#endif
}



/*
//vxy_image_layer
void vxy_image_layer_vel1(float* v1x, float* v1y, float* v1z,
float* v2x, float* v2y, float* v2z,
int neighb1, int neighb2, int neighb3, int neighb4,
int* nxtm1, int* nytm1, int* nxtop, int* nytop, int* nztop,
int* nxbtm, int* nybtm ,int* nzbtm)
{
errNum = CL_SUCCESS;
int argIdx = 0;
errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer1, argIdx++, sizeof(cl_mem), &v1xD);
errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer1, argIdx++, sizeof(cl_mem), &v1yD);
errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer1, argIdx++, sizeof(cl_mem), &v1zD);
errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer1, argIdx++, sizeof(cl_mem), &nd1_velD);
errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer1, argIdx++, sizeof(cl_mem), &dxi1D);
errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer1, argIdx++, sizeof(cl_mem), &dyi1D);
errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer1, argIdx++, sizeof(cl_mem), &dzh1D);
errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer1, argIdx++, sizeof(int), nxtm1);
errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer1, argIdx++, sizeof(int), nytm1);
errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer1, argIdx++, sizeof(int), nxtop);
errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer1, argIdx++, sizeof(int), nytop);
errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer1, argIdx++, sizeof(int), nztop);
errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer1, argIdx++, sizeof(int), neighb1);
errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer1, argIdx++, sizeof(int), neighb2);
errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer1, argIdx++, sizeof(int), neighb3);
errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer1, argIdx++, sizeof(int), neighb4);

if(errNum != CL_SUCCESS)
{
fprintf(stderr, "Error: setting kernel _cl_kernel_vel_vxy_image_layer1 arguments!\n");
}
int which_qid = MARSHAL_CMDQ;
errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_vxy_image_layer1, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
if(errNum != CL_SUCCESS)
{
fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_vxy_image_layer1 for execution!\n");
}
errNum = clWaitForEvents(1, &_cl_events[which_qid]);
CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel_vxy_image_layer1");

vel_vxy_image_layer1 <<<1,1>>>(v1xD, v1yD, v1zD, nd1_velD, dxi1D, dyi1D, dzh1D,
 *nxtm1, *nytm1, *nxtop, *nytop, *nztop, 
 neighb1, neighb2, neighb3, neighb4);
 cl_int errNum;
 errNum = cudaThreadSynchronize();
 CHECK_ERROR(errNum, "vxy_image_layer kernel error");
#if LOGGING_ENABLED == 1    
logger.logGPUArrInfo(v1xD_meta); 
logger.logGPUArrInfo(v1yD_meta); 
#endif
#if USE_MPIX == 0 && VALIDATE_MODE == 0
//for inner_I
errNum = cudaMemcpy(v1x, v1xD,  sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), cudaMemcpyDeviceToHost);
CHECK_ERROR(errNum, "outputDataCopyDeviceToHost1, v1x");
errNum = cudaMemcpy(v1y, v1yD,  sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), cudaMemcpyDeviceToHost);
CHECK_ERROR(errNum, "outputDataCopyDeviceToHost1, v1y");
errNum = cudaMemcpy(v1z, v1zD,  sizeof(float) * (*nztop + 2) * (*nxtop + 3) * (*nytop + 3), cudaMemcpyDeviceToHost);
CHECK_ERROR(errNum, "outputDataCopyDeviceToHost1, v1z");

//for inner_II
errNum = cudaMemcpy(v2x, v2xD,  sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3), cudaMemcpyDeviceToHost);
CHECK_ERROR(errNum, "outputDataCopyDeviceToHost1, v2x");
errNum = cudaMemcpy(v2y, v2yD,  sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3), cudaMemcpyDeviceToHost);
CHECK_ERROR(errNum, "outputDataCopyDeviceToHost1, v2y");
errNum = cudaMemcpy(v2z, v2zD,  sizeof(float) * (*nzbtm + 1) * (*nxbtm + 3) * (*nybtm + 3), cudaMemcpyDeviceToHost);
CHECK_ERROR(errNum, "outputDataCopyDeviceToHost1, vzz");
#endif

}
*/
void vxy_image_layer_sdx_vel(float* sdx1, float* sdx2, int* nxtop, int* nytop, int* nztop ) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	dim3 threadsPerBlock(MaxThreadsPerBlock);
	dim3 numblocks((*nytop/MaxThreadsPerBlock)+1);

	globalWorkSize[0] = numblocks.x*threadsPerBlock.x;
	globalWorkSize[1] = numblocks.y*threadsPerBlock.y;
	globalWorkSize[2] = numblocks.z*threadsPerBlock.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_sdx, argIdx++, sizeof(cl_mem), &sdx1D);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_sdx, argIdx++, sizeof(cl_mem), &sdx2D);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_sdx, argIdx++, sizeof(cl_mem), &v1xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_sdx, argIdx++, sizeof(cl_mem), &v1yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_sdx, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_sdx, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_sdx, argIdx++, sizeof(int), nztop);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_vxy_image_layer_sdx arguments!\n");
	}
	localWorkSize[0] = threadsPerBlock.x;
	localWorkSize[1] = threadsPerBlock.y;
	localWorkSize[2] = threadsPerBlock.z;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_vxy_image_layer_sdx, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_vxy_image_layer_sdx for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel_vxy_image_layer_sdx");

	//vel_vxy_image_layer_sdx <<<numblocks,threadsPerBlock>>>(sdx1D, sdx2D, v1xD, v1yD, *nxtop,
	//  *nytop, *nztop); 
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL vel_vxy_image_layer_sdx", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(sdx1D_meta); 
	logger.logGPUArrInfo(sdx2D_meta); 
#endif
#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(sdx1, sdx1D, sizeof(float) * (*nytop+6), cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sdx1D");
	errNum = cudaMemcpy(sdx2, sdx2D, sizeof(float) * (*nytop+6), cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost4, sdx2D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY vel_vxy_image_layer_sdx", cpy_time );
#endif    
}

void vxy_image_layer_sdy_vel(float* sdy1, float* sdy2, int* nxtop, int* nytop,int* nztop ) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	dim3 threadsPerBlock(MaxThreadsPerBlock);
	dim3 numblocks((*nxtop/MaxThreadsPerBlock)+1);

	globalWorkSize[0] = numblocks.x*threadsPerBlock.x;
	globalWorkSize[1] = numblocks.y*threadsPerBlock.y;
	globalWorkSize[2] = numblocks.z*threadsPerBlock.z;
	record_time(&tstart);
	cl_int errNum;
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_sdy, argIdx++, sizeof(cl_mem), &sdy1D);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_sdy, argIdx++, sizeof(cl_mem), &sdy2D);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_sdy, argIdx++, sizeof(cl_mem), &v1xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_sdy, argIdx++, sizeof(cl_mem), &v1yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_sdy, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_sdy, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_sdy, argIdx++, sizeof(int), nztop);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_vxy_image_layer_sdy arguments!\n");
	}
	localWorkSize[0] = threadsPerBlock.x;
	localWorkSize[1] = threadsPerBlock.y;
	localWorkSize[2] = threadsPerBlock.z;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_vxy_image_layer_sdy, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_vxy_image_layer_sdy for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel_vxy_image_layer_sdy");

	//vel_vxy_image_layer_sdy  <<<numblocks,threadsPerBlock>>>(sdy1D, sdy2D, v1xD, v1yD, *nxtop,
	//  *nytop, *nztop); 
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL vel_vxy_image_layer_sdy", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(sdy1D_meta); 
	logger.logGPUArrInfo(sdy2D_meta); 
#endif
#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(sdy1, sdy1D, sizeof(float) * (*nxtop+6), cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sdx1D");
	errNum = cudaMemcpy(sdy2, sdy2D, sizeof(float) * (*nxtop+6), cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost5, sdx2D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY vel_vxy_image_layer_sdy", cpy_time );
#endif    
}

void vxy_image_layer_rcx_vel(float* rcx1, float* rcx2, int* nxtop, int* nytop, int* nztop,  int* nx1p1) {

	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(rcx1D, rcx1, sizeof(float) * (*nytop+6), cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyhostToDevice, rcx1D");
	errNum = cudaMemcpy( rcx2D, rcx2, sizeof(float) * (*nytop+6), cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, rcx2D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY vel_vxy_image_layer_rcx", cpy_time );
#endif   
	dim3 threadsPerBlock(MaxThreadsPerBlock);
	dim3 numblocks((*nytop/MaxThreadsPerBlock)+1);

	globalWorkSize[0] = numblocks.x*threadsPerBlock.x;
	globalWorkSize[1] = numblocks.y*threadsPerBlock.y;
	globalWorkSize[2] = numblocks.z*threadsPerBlock.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_rcx, argIdx++, sizeof(cl_mem), &rcx1D);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_rcx, argIdx++, sizeof(cl_mem), &rcx2D);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_rcx, argIdx++, sizeof(cl_mem), &v1xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_rcx, argIdx++, sizeof(cl_mem), &v1yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_rcx, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_rcx, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_rcx, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_rcx, argIdx++, sizeof(int), nx1p1);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_vxy_image_layer_rcx arguments!\n");
	}
	localWorkSize[0] = threadsPerBlock.x;
	localWorkSize[1] = threadsPerBlock.y;
	localWorkSize[2] = threadsPerBlock.z;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_vxy_image_layer_rcx, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_vxy_image_layer_rcx for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel_vxy_image_layer_rcx");

	//vel_vxy_image_layer_rcx <<<numblocks,threadsPerBlock>>>(rcx1D, rcx2D, v1xD, v1yD, *nxtop,
	//*nytop, *nztop, *nx1p1); 
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL vel_vxy_image_layer_rcx", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(v1xD_meta); 
	logger.logGPUArrInfo(v1yD_meta); 
#endif    
}

void vxy_image_layer_rcx2_vel(float* rcx1, float* rcx2, int* nxtop, int* nytop, int* nztop, int* ny1p1) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(rcx1D, rcx1, sizeof(float) * (*nytop+6), cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, rcx1D");
	errNum = cudaMemcpy(rcx2D, rcx2, sizeof(float) * (*nytop+6), cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, rcx2D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY vel_vxy_image_layer_rcx2", cpy_time );
#endif  

	dim3 threadsPerBlock(MaxThreadsPerBlock);
	dim3 numblocks((*nxtop/MaxThreadsPerBlock)+1);
	//printf("in vxy_image_layer : nxtop=%d, nytop=%d\n", nxtop, nytop);

	globalWorkSize[0] = numblocks.x*threadsPerBlock.x;
	globalWorkSize[1] = numblocks.y*threadsPerBlock.y;
	globalWorkSize[2] = numblocks.z*threadsPerBlock.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_rcy, argIdx++, sizeof(cl_mem), &rcx1D);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_rcy, argIdx++, sizeof(cl_mem), &rcx2D);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_rcy, argIdx++, sizeof(cl_mem), &v1xD);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_rcy, argIdx++, sizeof(cl_mem), &v1yD);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_rcy, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_rcy, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_rcy, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_vel_vxy_image_layer_rcy, argIdx++, sizeof(int), ny1p1);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_vxy_image_layer_rcy arguments!\n");
	}
	localWorkSize[0] = threadsPerBlock.x;
	localWorkSize[1] = threadsPerBlock.y;
	localWorkSize[2] = threadsPerBlock.z;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_vxy_image_layer_rcy, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_vxy_image_layer_rcy for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel_vxy_image_layer_rcy");

	//vel_vxy_image_layer_rcy  <<<numblocks,threadsPerBlock>>>(rcx1D, rcx2D, v1xD, v1yD, *nxtop, *nytop, *nztop, *ny1p1); 
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL vel_vxy_image_layer_rcx2", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(v1xD_meta); 
	logger.logGPUArrInfo(v1yD_meta); 
#endif    
}

void add_dcs_vel(float* sutmArr, int nfadd, int ixsX, int ixsY, int ixsZ, 
		int fampX, int fampY, int ruptmX, int risetX, int sparam2X,  int nzrg11, int nzrg12, int nzrg13, int nzrg14, 
		int nxtop, int nytop, int nztop, int nxbtm, int nybtm, int nzbtm)
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int which_qid = MARSHAL_CMDQ;
	// TODO: change the below to OpenCL
	if(sutmArrD) clReleaseMemObject(sutmArrD);
	sutmArrD = NULL;
	//cudaFree(sutmArrD);
	sutmArrD = clCreateBuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(float) * (nfadd), NULL, NULL);
	CHECK_NULL_ERROR(sutmArrD, "sutmArrD");
	//errNum = cudaMalloc((void**)&sutmArrD, sizeof(float) * (nfadd));
	errNum = clEnqueueWriteBuffer(_cl_commandQueues[which_qid], sutmArrD, CL_TRUE, 0, sizeof(float) * (nfadd), sutmArr, 0, NULL, NULL);
//	errNum = cudaMemcpy(sutmArrD, sutmArr, sizeof(float) * (nfadd), cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sutmArrD");

	clRetainMemObject(sutmArrD);
	dim3 threadsPerBlock(MaxThreadsPerBlock);
	dim3 numblocks((nfadd/MaxThreadsPerBlock)+1);
	globalWorkSize[0] = numblocks.x*threadsPerBlock.x;
	globalWorkSize[1] = numblocks.y*threadsPerBlock.y;
	globalWorkSize[2] = numblocks.z*threadsPerBlock.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(cl_mem), &t1xxD);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(cl_mem), &t1xyD);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(cl_mem), &t1xzD);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(cl_mem), &t1yyD);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(cl_mem), &t1yzD);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(cl_mem), &t1zzD);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(cl_mem), &t2xxD);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(cl_mem), &t2yyD);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(cl_mem), &t2xyD);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(cl_mem), &t2xzD);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(cl_mem), &t2yzD);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(cl_mem), &t2zzD);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(int), &nfadd);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(cl_mem), &index_xyz_sourceD);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(int), &ixsX);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(int), &ixsY);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(int), &ixsZ);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(cl_mem), &fampD);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(int), &fampX);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(int), &fampY);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(cl_mem), &ruptmD);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(int), &ruptmX);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(cl_mem), &risetD);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(int), &risetX);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(cl_mem), &sparam2D);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(int), &sparam2X);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(cl_mem), &sutmArrD);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(int), &nzrg11);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(int), &nzrg12);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(int), &nzrg13);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(int), &nzrg14);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(int), &nxtop);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(int), &nytop);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(int), &nztop);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(int), &nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(int), &nybtm);
	errNum |= clSetKernelArg(_cl_kernel_vel_add_dcs, argIdx++, sizeof(int), &nzbtm);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_vel_add_dcs arguments!\n");
	}
	localWorkSize[0] = threadsPerBlock.x;
	localWorkSize[1] = threadsPerBlock.y;
	localWorkSize[2] = threadsPerBlock.z;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_vel_add_dcs, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_vel_add_dcs for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_vel_add_dcs");

	//vel_add_dcs <<<numblocks, threadsPerBlock>>>( t1xxD, t1xyD, t1xzD, t1yyD, t1yzD, t1zzD, 
	//          t2xxD, t2yyD, t2xyD, t2xzD, t2yzD, t2zzD, 
	//        nfadd, index_xyz_sourceD,  ixsX, ixsY, ixsZ, 
	//      fampD, fampX, fampY, 
	//    ruptmD, ruptmX, risetD, risetX, sparam2D, sparam2X, sutmArrD,
	//  nzrg11, nzrg12, nzrg13, nzrg14, 
	//       nxtop, nytop, nztop,
	//     nxbtm, nybtm , nzbtm);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL add_dcs_vel", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(t1xxD_meta); 
	logger.logGPUArrInfo(t1yyD_meta); 
	logger.logGPUArrInfo(t1zzD_meta); 
	logger.logGPUArrInfo(t2xxD_meta); 
	logger.logGPUArrInfo(t2yyD_meta); 
	logger.logGPUArrInfo(t2zzD_meta); 
	logger.logGPUArrInfo(t1xzD_meta); 
	logger.logGPUArrInfo(t2xzD_meta); 
	logger.logGPUArrInfo(t1yzD_meta); 
	logger.logGPUArrInfo(t2yzD_meta); 
#endif
}

// STRESS  Computation:
void sdx41_stress (float** sdx41, int* nxtop, int* nytop, int* nztop) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nztop)/blockSizeX + 1;
	int gridY = (*nytop)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx41, argIdx++, sizeof(cl_mem), &sdx41D);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx41, argIdx++, sizeof(cl_mem), &t1xxD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx41, argIdx++, sizeof(cl_mem), &t1xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx41, argIdx++, sizeof(cl_mem), &t1xzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx41, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx41, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx41, argIdx++, sizeof(int), nxtop);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_sdx41 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_stress_sdx41, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_sdx41 for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_stress_sdx41");

	//stress_sdx41 <<< dimGrid, dimBlock >>> (sdx41D, t1xxD, t1xyD, t1xzD, *nytop, *nztop, *nxtop);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL sdx41_stress", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(sdx41D_meta);
#endif    
#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(*sdx41, sdx41D, sizeof(float) * (*nztop)* (*nytop)* 4, cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sdx41D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY sdx41_stress", cpy_time );
#endif    
}

void sdx42_stress (float** sdx42, int* nxbtm, int* nybtm, int* nzbtm) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nzbtm)/blockSizeX + 1;
	int gridY = (*nybtm)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx42, argIdx++, sizeof(cl_mem), &sdx42D);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx42, argIdx++, sizeof(cl_mem), &t2xxD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx42, argIdx++, sizeof(cl_mem), &t2xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx42, argIdx++, sizeof(cl_mem), &t2xzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx42, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx42, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx42, argIdx++, sizeof(int), nxbtm);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_sdx42 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_stress_sdx42, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_sdx42 for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_stress_sdx42");

	//    stress_sdx42 <<< dimGrid, dimBlock >>> (sdx42D, t2xxD, t2xyD, t2xzD, *nybtm, *nzbtm, *nxbtm);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL sdx42_stress", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(sdx42D_meta); 
#endif
#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(*sdx42, sdx42D, sizeof(float) * (*nzbtm)* (*nybtm)* 4, cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sdx42D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY sdx42_stress", cpy_time );
#endif    
}
void sdx51_stress (float** sdx51, int* nxtop, int* nytop, int* nztop, int* nxtm1) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nztop)/blockSizeX + 1;
	int gridY = (*nytop)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];

	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx51, argIdx++, sizeof(cl_mem), &sdx51D);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx51, argIdx++, sizeof(cl_mem), &t1xxD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx51, argIdx++, sizeof(cl_mem), &t1xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx51, argIdx++, sizeof(cl_mem), &t1xzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx51, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx51, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx51, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx51, argIdx++, sizeof(int), nxtm1);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_sdx51 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_stress_sdx51, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_sdx51 for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_stress_sdx51");

	//stress_sdx51 <<< dimGrid, dimBlock >>> (sdx51D, t1xxD, t1xyD, t1xzD, *nytop, *nztop, *nxtop, *nxtm1);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL sdx51_stress", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(sdx51D_meta); 
#endif
#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(*sdx51, sdx51D, sizeof(float) * (*nztop)* (*nytop)* 5, cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sdx51D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY sdx51_stress", cpy_time );
#endif    
}

void sdx52_stress (float** sdx52, int* nxbtm, int* nybtm, int* nzbtm, int* nxbm1) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nzbtm)/blockSizeX + 1;
	int gridY = (*nybtm)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx52, argIdx++, sizeof(cl_mem), &sdx52D);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx52, argIdx++, sizeof(cl_mem), &t2xxD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx52, argIdx++, sizeof(cl_mem), &t2xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx52, argIdx++, sizeof(cl_mem), &t2xzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx52, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx52, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx52, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdx52, argIdx++, sizeof(int), nxbm1);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_sdx52 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_stress_sdx52, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_sdx52 for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_stress_sdx52");

	//stress_sdx52 <<< dimGrid, dimBlock >>> (sdx52D, t2xxD, t2xyD, t2xzD, *nybtm, *nzbtm, *nxbtm, *nxbm1);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL sdx52_stress", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(sdx52D_meta); 
#endif
#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(*sdx52, sdx52D, sizeof(float) * (*nzbtm)* (*nybtm)* 5, cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sdx52D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY sdx52_stress", cpy_time );
#endif    
}
void sdy41_stress (float** sdy41, int* nxtop, int* nytop, int* nztop) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nztop)/blockSizeX + 1;
	int gridY = (*nxtop)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];

	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy41, argIdx++, sizeof(cl_mem), &sdy41D);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy41, argIdx++, sizeof(cl_mem), &t1yyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy41, argIdx++, sizeof(cl_mem), &t1xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy41, argIdx++, sizeof(cl_mem), &t1yzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy41, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy41, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy41, argIdx++, sizeof(int), nxtop);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_sdy41 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_stress_sdy41, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_sdy41 for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_stress_sdy41");

	//stress_sdy41 <<< dimGrid, dimBlock >>> (sdy41D, t1yyD, t1xyD, t1yzD, *nytop, *nztop, *nxtop);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL sdy41_stress", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(sdy41D_meta); 
#endif
#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(*sdy41, sdy41D, sizeof(float) * (*nztop)* (*nxtop)* 4, cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sdy41D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY sdy41_stress", cpy_time );
#endif    
}

void sdy42_stress (float** sdy42, int* nxbtm, int* nybtm, int* nzbtm) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nzbtm)/blockSizeX + 1;
	int gridY = (*nxbtm)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];

	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy42, argIdx++, sizeof(cl_mem), &sdy42D);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy42, argIdx++, sizeof(cl_mem), &t2yyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy42, argIdx++, sizeof(cl_mem), &t2xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy42, argIdx++, sizeof(cl_mem), &t2yzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy42, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy42, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy42, argIdx++, sizeof(int), nxbtm);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_sdy42 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_stress_sdy42, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_sdy42 for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_stress_sdy42");

	//stress_sdy42 <<< dimGrid, dimBlock >>> (sdy42D, t2yyD, t2xyD, t2yzD, *nybtm, *nzbtm, *nxbtm);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL sdy42_stress", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(sdy42D_meta); 
#endif
#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(*sdy42, sdy42D, sizeof(float) * (*nzbtm)* (*nxbtm)* 4, cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sdx42D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY sdy42_stress", cpy_time );
#endif    
}
void sdy51_stress (float** sdy51, int* nxtop, int* nytop, int* nztop, int* nytm1) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nztop)/blockSizeX + 1;
	int gridY = (*nxtop)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy51, argIdx++, sizeof(cl_mem), &sdy51D);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy51, argIdx++, sizeof(cl_mem), &t1yyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy51, argIdx++, sizeof(cl_mem), &t1xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy51, argIdx++, sizeof(cl_mem), &t1yzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy51, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy51, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy51, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy51, argIdx++, sizeof(int), nytm1);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_sdy51 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_stress_sdy51, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_sdy51 for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_stress_sdy51");

	//stress_sdy51 <<< dimGrid, dimBlock >>> (sdy51D, t1yyD, t1xyD, t1yzD, *nytop, *nztop, *nxtop, *nytm1);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL sdy51_stress", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(sdy51D_meta); 
#endif
#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(*sdy51, sdy51D, sizeof(float) * (*nztop)* (*nxtop)* 5, cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sdy51D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY sdy51_stress", cpy_time );
#endif    
}

void sdy52_stress (float** sdy52, int* nxbtm, int* nybtm, int* nzbtm, int* nybm1) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nzbtm)/blockSizeX + 1;
	int gridY = (*nxbtm)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy52, argIdx++, sizeof(cl_mem), &sdy52D);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy52, argIdx++, sizeof(cl_mem), &t2yyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy52, argIdx++, sizeof(cl_mem), &t2xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy52, argIdx++, sizeof(cl_mem), &t2yzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy52, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy52, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy52, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_sdy52, argIdx++, sizeof(int), nybm1);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_sdy52 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_stress_sdy52, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_sdy52 for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_stress_sdy52");

	//stress_sdy52 <<< dimGrid, dimBlock >>> (sdy52D, t2yyD, t2xyD, t2yzD, *nybtm, *nzbtm, *nxbtm, *nybm1);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL sdy52_stress", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(sdy52D_meta); 
#endif
#if 0//USE_MPIX == 0 && VALIDATE_MODE == 0
	record_time(&tstart);
	errNum = cudaMemcpy(*sdy52, sdy52D, sizeof(float) * (*nzbtm)* (*nxbtm)* 5, cudaMemcpyDeviceToHost);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, sdy52D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY sdy52_stress", cpy_time );
#endif    
}
//rc
void rcx41_stress (float** rcx41, int* nxtop, int* nytop, int* nztop, int* nx1p1, int* nx1p2) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(rcx41D, *rcx41, sizeof(float) * (*nztop)* (*nytop)* 4, cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, rcx41D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY rcx41_stress", cpy_time );
#endif
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nztop)/blockSizeX + 1;
	int gridY = (*nytop)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx41, argIdx++, sizeof(cl_mem), &rcx41D);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx41, argIdx++, sizeof(cl_mem), &t1xxD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx41, argIdx++, sizeof(cl_mem), &t1xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx41, argIdx++, sizeof(cl_mem), &t1xzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx41, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx41, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx41, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx41, argIdx++, sizeof(int), nx1p1);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx41, argIdx++, sizeof(int), nx1p2);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_rcx41 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_stress_rcx41, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_rcx41 for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_stress_rcx41");

	//stress_rcx41 <<< dimGrid, dimBlock >>> (rcx41D, t1xxD, t1xyD, t1xzD, *nytop, *nztop, *nxtop, *nx1p1, *nx1p2);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL rcx41_stress", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(rcx41D_meta); 
#endif
}

void rcx42_stress (float** rcx42, int* nxbtm, int* nybtm, int* nzbtm, int* nx2p1, int* nx2p2) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(rcx42D, *rcx42, sizeof(float) * (*nzbtm)* (*nybtm)* 4, cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, rcx42D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY rcx42_stress", cpy_time );
#endif
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nzbtm)/blockSizeX + 1;
	int gridY = (*nybtm)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};

	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx42, argIdx++, sizeof(cl_mem), &rcx42D);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx42, argIdx++, sizeof(cl_mem), &t2xxD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx42, argIdx++, sizeof(cl_mem), &t2xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx42, argIdx++, sizeof(cl_mem), &t2xzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx42, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx42, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx42, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx42, argIdx++, sizeof(int), nx2p1);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx42, argIdx++, sizeof(int), nx2p2);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_rcx42 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_stress_rcx42, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_rcx42 for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_stress_rcx42");

	// stress_rcx42 <<< dimGrid, dimBlock >>> (rcx42D, t2xxD, t2xyD, t2xzD, *nybtm, *nzbtm, *nxbtm, *nx2p1, *nx2p2);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL rcx42_stress", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(rcx42D_meta); 
#endif
}
void rcx51_stress (float** rcx51, int* nxtop, int* nytop, int* nztop) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(rcx51D, *rcx51, sizeof(float) * (*nztop)* (*nytop)* 5, cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, rcx51D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY rcx51_stress", cpy_time );
#endif
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nztop)/blockSizeX + 1;
	int gridY = (*nytop)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];

	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx51, argIdx++, sizeof(cl_mem), &rcx51D);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx51, argIdx++, sizeof(cl_mem), &t1xxD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx51, argIdx++, sizeof(cl_mem), &t1xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx51, argIdx++, sizeof(cl_mem), &t1xzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx51, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx51, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx51, argIdx++, sizeof(int), nxtop);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_rcx51 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_stress_rcx51, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_rcx51 for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_stress_rcx51");

	//stress_rcx51 <<< dimGrid, dimBlock >>> (rcx51D, t1xxD, t1xyD, t1xzD, *nytop, *nztop, *nxtop);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL rcx51_stress", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(rcx51D_meta); 
#endif
}

void rcx52_stress (float** rcx52, int* nxbtm, int* nybtm, int* nzbtm) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(rcx52D, *rcx52, sizeof(float) * (*nzbtm)* (*nybtm)* 5, cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyHostToDevice, rcx52D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY rcx52_stress", cpy_time );
#endif
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nzbtm)/blockSizeX + 1;
	int gridY = (*nybtm)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];

	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx52, argIdx++, sizeof(cl_mem), &rcx52D);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx52, argIdx++, sizeof(cl_mem), &t2xxD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx52, argIdx++, sizeof(cl_mem), &t2xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx52, argIdx++, sizeof(cl_mem), &t2xzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx52, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx52, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcx52, argIdx++, sizeof(int), nxbtm);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_rcx52 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_stress_rcx52, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_rcx52 for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_stress_rcx52");

	//stress_rcx52 <<< dimGrid, dimBlock >>> (rcx52D, t2xxD, t2xyD, t2xzD, *nybtm, *nzbtm, *nxbtm);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL rcx52_stress", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(rcx52D_meta); 
#endif
}

void rcy41_stress (float** rcy41, int* nxtop, int* nytop, int* nztop, int* ny1p1, int* ny1p2) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(rcy41D, *rcy41, sizeof(float) * (*nztop)* (*nxtop)* 4, cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, rcy41D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY rcy41_stress", cpy_time );
#endif
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nztop)/blockSizeX + 1;
	int gridY = (*nxtop)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];

	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy41, argIdx++, sizeof(cl_mem), &rcy41D);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy41, argIdx++, sizeof(cl_mem), &t1yyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy41, argIdx++, sizeof(cl_mem), &t1xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy41, argIdx++, sizeof(cl_mem), &t1yzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy41, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy41, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy41, argIdx++, sizeof(int), nxtop);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy41, argIdx++, sizeof(int), ny1p1);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy41, argIdx++, sizeof(int), ny1p2);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_rcy41 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_stress_rcy41, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_rcy41 for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_stress_rcy41");

	//    stress_rcy41 <<< dimGrid, dimBlock >>> (rcy41D, t1yyD, t1xyD, t1yzD, *nytop, *nztop, *nxtop, *ny1p1, *ny1p2);

	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL rcy41_stress", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(rcy41D_meta); 
#endif
}

void rcy42_stress (float** rcy42, int* nxbtm, int* nybtm, int* nzbtm, int* ny2p1, int* ny2p2) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(rcy42D, *rcy42, sizeof(float) * (*nzbtm)* (*nxbtm)* 4, cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, rcx42D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY rcy42_stress", cpy_time );
#endif
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nzbtm)/blockSizeX + 1;
	int gridY = (*nxbtm)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];

	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy42, argIdx++, sizeof(cl_mem), &rcy42D);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy42, argIdx++, sizeof(cl_mem), &t2yyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy42, argIdx++, sizeof(cl_mem), &t2xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy42, argIdx++, sizeof(cl_mem), &t2yzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy42, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy42, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy42, argIdx++, sizeof(int), nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy42, argIdx++, sizeof(int), ny2p1);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy42, argIdx++, sizeof(int), ny2p2);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_rcy42 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_stress_rcy42, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_rcy42 for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_stress_rcy42");

	//stress_rcy42 <<< dimGrid, dimBlock >>> (rcy42D, t2yyD, t2xyD, t2yzD, *nybtm, *nzbtm, *nxbtm, *ny2p1, *ny2p2);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL rcy42_stress", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(rcy41D_meta); 
#endif
}
void rcy51_stress (float** rcy51, int* nxtop, int* nytop, int* nztop) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(rcy51D, *rcy51, sizeof(float) * (*nztop)* (*nxtop)* 5, cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, rcy51D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY rcy51_stress", cpy_time );
#endif
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nztop)/blockSizeX + 1;
	int gridY = (*nxtop)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];

	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy51, argIdx++, sizeof(cl_mem), &rcy51D);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy51, argIdx++, sizeof(cl_mem), &t1yyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy51, argIdx++, sizeof(cl_mem), &t1xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy51, argIdx++, sizeof(cl_mem), &t1yzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy51, argIdx++, sizeof(int), nytop);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy51, argIdx++, sizeof(int), nztop);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy51, argIdx++, sizeof(int), nxtop);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_rcy51 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_stress_rcy51, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_rcy51 for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_stress_rcy51");

	//stress_rcy51 <<< dimGrid, dimBlock >>> (rcy51D, t1yyD, t1xyD, t1yzD, *nytop, *nztop, *nxtop);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL rcy51_stress", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(rcy41D_meta); 
#endif
}

void rcy52_stress (float** rcy52, int* nxbtm, int* nybtm, int* nzbtm) {
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;    
#if 0//USE_MPIX == 0
	record_time(&tstart);
	errNum = cudaMemcpy(rcy52D, *rcy52, sizeof(float) * (*nzbtm)* (*nxbtm)* 5, cudaMemcpyHostToDevice);
	CHECK_ERROR(errNum, "InputDataCopyDeviceToHost, rcy52D");
	record_time(&tend);
	cpy_time = tend-tstart;
	logger_log_timing("CPY rcx52_stress", cpy_time );
#endif
	int blockSizeX = NTHREADS;        
	int blockSizeY = NTHREADS;        
	size_t dimBlock[3] = {blockSizeX, blockSizeY, 1};
	int gridX = (*nzbtm)/blockSizeX + 1;
	int gridY = (*nxbtm)/blockSizeY + 1;
	size_t dimGrid[3] = {gridX, gridY, 1};
	localWorkSize[0] = dimBlock[0];
	localWorkSize[1] = dimBlock[1];
	localWorkSize[2] = dimBlock[2];
	globalWorkSize[0] = dimGrid[0]*localWorkSize[0];
	globalWorkSize[1] = dimGrid[1]*localWorkSize[1];
	globalWorkSize[2] = dimGrid[2]*localWorkSize[2];

	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy52, argIdx++, sizeof(cl_mem), &rcy52D);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy52, argIdx++, sizeof(cl_mem), &t2yyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy52, argIdx++, sizeof(cl_mem), &t2xyD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy52, argIdx++, sizeof(cl_mem), &t2yzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy52, argIdx++, sizeof(int), nybtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy52, argIdx++, sizeof(int), nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_rcy52, argIdx++, sizeof(int), nxbtm);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_rcy52 arguments!\n");
	}
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_stress_rcy52, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_rcy52 for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, foo_kernel");

	//stress_rcy52 <<< dimGrid, dimBlock >>> (rcy52D, t2yyD, t2xyD, t2yzD, *nybtm, *nzbtm, *nxbtm);
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL rcx52_stress", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(rcx52D_meta); 
#endif    
}
void interp_stress (int neighb1, int neighb2, int neighb3, int neighb4, 
		int nxbm1, int nybm1, int nxbtm , int nybtm , int nzbtm, int nxtop, int nytop,  int nztop, int nz1p1) 
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_stress_interp_stress, argIdx++, sizeof(cl_mem), &t1xzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp_stress, argIdx++, sizeof(cl_mem), &t1yzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp_stress, argIdx++, sizeof(cl_mem), &t1zzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp_stress, argIdx++, sizeof(cl_mem), &t2xzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp_stress, argIdx++, sizeof(cl_mem), &t2yzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp_stress, argIdx++, sizeof(cl_mem), &t2zzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp_stress, argIdx++, sizeof(int), &neighb1);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp_stress, argIdx++, sizeof(int), &neighb2);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp_stress, argIdx++, sizeof(int), &neighb3);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp_stress, argIdx++, sizeof(int), &neighb4);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp_stress, argIdx++, sizeof(int), &nxbm1);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp_stress, argIdx++, sizeof(int), &nybm1);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp_stress, argIdx++, sizeof(int), &nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp_stress, argIdx++, sizeof(int), &nybtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp_stress, argIdx++, sizeof(int), &nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp_stress, argIdx++, sizeof(int), &nxtop);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp_stress, argIdx++, sizeof(int), &nytop);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp_stress, argIdx++, sizeof(int), &nztop);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp_stress, argIdx++, sizeof(int), &nz1p1);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_interp_stress arguments!\n");
	}
	localWorkSize[0] = 1;
	localWorkSize[1] = 1; 
	localWorkSize[2] = 1; 
	globalWorkSize[0] = 1;
	globalWorkSize[1] = 1;
	globalWorkSize[2] = 1;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_stress_interp_stress, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_interp_stress for execution!\n");
	}
	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_stress_interp_stress");

	//stress_interp_stress <<<1,1>>> (t1xzD, t1yzD, t1zzD, t2xzD, t2yzD, t2zzD,
	//                                 neighb1, neighb2, neighb3, neighb4, 
	//                               nxbm1, nybm1,
	//                             nxbtm, nybtm, nzbtm,
	//                           nxtop, nytop, nztop, nz1p1 );     
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL interp_stress", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(t2xzD_meta); 
	logger.logGPUArrInfo(t2yzD_meta); 
	logger.logGPUArrInfo(t2zzD_meta); 
#endif    
}

void interp_stress1 ( int ntx1, int nz1p1, int nxbtm , int nybtm , int nzbtm, 
		int nxtop, int nytop,  int nztop) 
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	dim3 threads(32,32);
	dim3 blocks ( ((int)nybtm/32)+1, ((int)ntx1/32)+1);
	globalWorkSize[0] = blocks.x*threads.x;
	globalWorkSize[1] = blocks.y*threads.y;
	globalWorkSize[2] = blocks.z*threads.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_stress_interp1, argIdx++, sizeof(int), &ntx1);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp1, argIdx++, sizeof(int), &nz1p1);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp1, argIdx++, sizeof(int), &nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp1, argIdx++, sizeof(int), &nybtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp1, argIdx++, sizeof(int), &nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp1, argIdx++, sizeof(int), &nxtop);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp1, argIdx++, sizeof(int), &nytop);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp1, argIdx++, sizeof(int), &nztop);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp1, argIdx++, sizeof(cl_mem), &t1xzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp1, argIdx++, sizeof(cl_mem), &t2xzD);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_interp1 arguments!\n");
	}
	localWorkSize[0] = threads.x;
	localWorkSize[1] = threads.y;
	localWorkSize[2] = threads.z;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_stress_interp1, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_interp1 for execution!\n");
	}
//	errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_stress_interp1");

	//    stress_interp1 <<< blocks, threads>>>(ntx1, nz1p1, 
	//                              nxbtm, nybtm, nzbtm,
	//                            nxtop, nytop, nztop,
	//                          t1xzD, t2xzD); 
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL interp1_stress", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(t2xzD_meta); 
#endif    
}

void interp_stress2 ( int nty1, int nz1p1, int nxbtm , int nybtm , int nzbtm, 
		int nxtop, int nytop,  int nztop) 
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	dim3 threads(32,32);
	dim3 blocks ( ((int)nty1/32)+1, ((int)nxbtm/32)+1);
	globalWorkSize[0] = blocks.x*threads.x;
	globalWorkSize[1] = blocks.y*threads.y;
	globalWorkSize[2] = blocks.z*threads.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_stress_interp2, argIdx++, sizeof(int), &nty1);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp2, argIdx++, sizeof(int), &nz1p1);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp2, argIdx++, sizeof(int), &nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp2, argIdx++, sizeof(int), &nybtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp2, argIdx++, sizeof(int), &nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp2, argIdx++, sizeof(int), &nxtop);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp2, argIdx++, sizeof(int), &nytop);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp2, argIdx++, sizeof(int), &nztop);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp2, argIdx++, sizeof(cl_mem), &t1yzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp2, argIdx++, sizeof(cl_mem), &t2yzD);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_interp2 arguments!\n");
	}
	localWorkSize[0] = threads.x;
	localWorkSize[1] = threads.y;
	localWorkSize[2] = threads.z;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_stress_interp2, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_interp2 for execution!\n");
	}
	//errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_stress_interp2");

	//stress_interp2 <<< blocks, threads>>>(nty1, nz1p1, 
	//                              nxbtm, nybtm, nzbtm,
	//                            nxtop, nytop, nztop,
	//                          t1yzD, t2yzD); 
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL stress_interp2", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(t2yzD_meta); 
#endif    
}

void interp_stress3 ( int nxbtm , int nybtm , int nzbtm, 
		int nxtop, int nytop,  int nztop) 
{
	double tstart, tend;
	double cpy_time=0, kernel_time=0;

	cl_int errNum;
	dim3 threads(32,32);
	dim3 blocks ( ((int)nybtm/32)+1, ((int)nxbtm/32)+1);
	globalWorkSize[0] = blocks.x*threads.x;
	globalWorkSize[1] = blocks.y*threads.y;
	globalWorkSize[2] = blocks.z*threads.z;
	record_time(&tstart);
	errNum = CL_SUCCESS;
	int argIdx = 0;
	errNum |= clSetKernelArg(_cl_kernel_stress_interp3, argIdx++, sizeof(int), &nxbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp3, argIdx++, sizeof(int), &nybtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp3, argIdx++, sizeof(int), &nzbtm);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp3, argIdx++, sizeof(int), &nxtop);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp3, argIdx++, sizeof(int), &nytop);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp3, argIdx++, sizeof(int), &nztop);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp3, argIdx++, sizeof(cl_mem), &t1zzD);
	errNum |= clSetKernelArg(_cl_kernel_stress_interp3, argIdx++, sizeof(cl_mem), &t2zzD);

	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: setting kernel _cl_kernel_stress_interp3 arguments!\n");
	}
	localWorkSize[0] = threads.x;
	localWorkSize[1] = threads.y;
	localWorkSize[2] = threads.z;
	int which_qid = MARSHAL_CMDQ;
	errNum = clEnqueueNDRangeKernel(_cl_commandQueues[which_qid], _cl_kernel_stress_interp3, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, &_cl_events[which_qid]);
	if(errNum != CL_SUCCESS)
	{
		fprintf(stderr, "Error: queuing kernel _cl_kernel_stress_interp3 for execution!\n");
	}
	//errNum = clWaitForEvents(1, &_cl_events[which_qid]);
	CHECK_ERROR(errNum, "Kernel Launch, _cl_kernel_stress_interp3");

	//stress_interp3 <<< blocks, threads>>>(nxbtm, nybtm, nzbtm,
	//                              nxtop, nytop, nztop,
	//                            t1zzD, t2zzD); 
	record_time(&tend);
	kernel_time = tend-tstart;
	logger_log_timing("KERNEL stress_interp3", kernel_time );

#if LOGGING_ENABLED == 1    
	logger.logGPUArrInfo(t2zzD_meta); 
#endif    
}



#ifdef __cplusplus
}
#endif
