#
# © 2013.  Virginia Polytechnic Institute & State University
# 
# This GPU-accelerated code is based on the MPI code supplied by Pengcheng Liu of USBR.
#
MPI_DIR    = ${MPICH_DIR}
CUDA_HOME  = ${CUDA_INSTALL_PATH}
#SOCL_INC = -I$(CUDA_INSTALL_PATH)/include -I$(SOCL_ROOT)/socl/src -I$(HOME)/opt/include/starpu/1.2 -I$(HOME)/src/starpu-1.2.0rc1/build/include 
SOCL_INC =  -I$(SOCL_ROOT)/socl/src -I$(HOME)/opt/include/starpu/1.2 -I$(CUDA_INSTALL_PATH)/include -I$(HOME)/src/starpu-1.2.0rc1/build/include
OPENCL_HOME  = ${SNUCLROOT}
SNUCL_INC = -I${SNUCLROOT}/inc
CL_INC = ${SOCL_INC}

PAPI_HOME = /state/partition1/apps/papi
LINKF      = disfd_comm.o disfd_sub.o disfd_subc.o timer.o disfd_sub_opencl.o #papi_interface.o #disfd_subcu.o
MY_INCLUDES=-I$(HOME)/opt/include -I/usr/include -I$(MPI_DIR)/include -I. ${CL_INC} -I$(CUDA_HOME)/include 
MPIF90     = mpif90 -ffree-line-length-none -ffixed-line-length-none -O3 -g $(MY_INCLUDES)
MPICC      = mpicxx -O3 -g $(MY_INCLUDES)
FF90       = gfortran -ffree-line-length-none -ffixed-line-length-none -O3 -g $(MY_INCLUDES)
#FF90       = pgf90  -O3
CC         = g++ -O3 -g $(MY_INCLUDES)
#CC         = pgcc -O3
GCC        = g++ -O3 -g $(MY_INCLUDES)

all: disfd vm4dfd_v3 read_dfd

papi_interface.o: papi_interface.c
	$(CC) -I$(PAPI_HOME)/include -c $< -o $@

timer.o: timer.c
	$(CC) -c $< -o $@

#disfd_subcu.o: disfd_sub.cu
#	nvcc -gencode arch=compute_35,code=sm_35 -c $< -o $@

disfd_subc.o: disfd_sub.c
	$(CC) -I$(MPI_DIR)/include/ ${CL_INC} -c $< -o $@

disfd: disfd.f90 ${LINKF}
	$(MPIF90) -cpp -lOpenCL -L$(CUDA_HOME)/lib64 -lcudart -L$(MPI_DIR)/lib -lmpichf90 -L$(PAPI_HOME)/lib -lpapi -o $@ $@.f90 ${LINKF} 

disfd_sub.o: disfd_sub.f90 disfd_comm.o 
	$(MPIF90) -cpp -c disfd_sub.f90

disfd_comm.o: disfd_comm.f90
	$(MPIF90) -cpp -c $< -o $@

vm4dfd_v3: vm4dfd_v3.f90
	$(FF90) -o $@ $< 

read_dfd: read_dfd.f90 xapiir.o sacio.o
	$(FF90) -o $@ $@.f90 xapiir.o sacio.o

xapiir.o: xapiir.f90
	$(FF90) -c $< -o $@
	
sacio.o: sacio.c
	$(CC) -c $< -o $@

disfd_sub_opencl.o: disfd_sub_opencl.c switches.h
	$(GCC) -o $@ -c $< 
#	$(GCC) -o $@ -c $< -I$(CUDA_HOME)/include
	
clean:
	rm -f *.mod *.o \
		disfd vm4dfd_v3 read_dfd
