/*****************************************************************************/
/*                                                                           */
/* Copyright (c) 2011-2013 Seoul National University.                        */
/* All rights reserved.                                                      */
/*                                                                           */
/* Redistribution and use in source and binary forms, with or without        */
/* modification, are permitted provided that the following conditions        */
/* are met:                                                                  */
/*   1. Redistributions of source code must retain the above copyright       */
/*      notice, this list of conditions and the following disclaimer.        */
/*   2. Redistributions in binary form must reproduce the above copyright    */
/*      notice, this list of conditions and the following disclaimer in the  */
/*      documentation and/or other materials provided with the distribution. */
/*   3. Neither the name of Seoul National University nor the names of its   */
/*      contributors may be used to endorse or promote products derived      */
/*      from this software without specific prior written permission.        */
/*                                                                           */
/* THIS SOFTWARE IS PROVIDED BY SEOUL NATIONAL UNIVERSITY "AS IS" AND ANY    */
/* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED */
/* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE    */
/* DISCLAIMED. IN NO EVENT SHALL SEOUL NATIONAL UNIVERSITY BE LIABLE FOR ANY */
/* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL        */
/* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS   */
/* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)     */
/* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,       */
/* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN  */
/* ANY WAY OUT OF THE USE OF THIS  SOFTWARE, EVEN IF ADVISED OF THE          */
/* POSSIBILITY OF SUCH DAMAGE.                                               */
/*                                                                           */
/* Contact information:                                                      */
/*   Center for Manycore Programming                                         */
/*   Department of Computer Science and Engineering                          */
/*   Seoul National University, Seoul 151-744, Korea                         */
/*   http://aces.snu.ac.kr                                                   */
/*                                                                           */
/* Contributors:                                                             */
/*   Jungwon Kim, Sangmin Seo, Gangwon Jo, Jun Lee, Jeongho Nah,             */
/*   Jungho Park, Junghyun Kim, and Jaejin Lee                               */
/*                                                                           */
/*****************************************************************************/

#include "CLContext.h"
#include <algorithm>
#include <cstring>
#include <vector>
#include <pthread.h>
#include <CL/cl.h>
#include "Callbacks.h"
#include "CLDevice.h"
#include "CLDispatch.h"
#include "CLMem.h"
#include "CLObject.h"
#include "CLSampler.h"
#include "CLPlatform.h"
#include "Structs.h"
#include "Utils.h"

using namespace std;

CLContext::CLContext(const std::vector<CLDevice*>& devices,
                     size_t num_properties,
                     const cl_context_properties* properties,
					 const std::vector<hwloc_obj_t> &hosts,
					 std::vector<perf_vector> &d2d_distances,
  					 std::vector<perf_vector> &d2h_distances,
  					 perf_vector &d_compute_perfs,
  					 perf_vector &d_mem_perfs,
  					 perf_vector &d_lmem_perfs,
					 vector<size_t> &filter_indices
					 )
    : devices_(devices) {//, hosts_(hosts) {
  for (vector<CLDevice*>::iterator it = devices_.begin();
       it != devices_.end();
       ++it) {
    (*it)->Retain();
  }

  num_properties_ = num_properties;
  if (num_properties > 0) {
    properties_ = (cl_context_properties*)malloc(
        sizeof(cl_context_properties) * num_properties);
    memcpy(properties_, properties,
           sizeof(cl_context_properties) * num_properties);
  } else {
    properties_ = NULL;
  }
  callback_ = NULL;
  hosts_.clear();
  hosts_ = hosts;
  if(d_compute_perfs.size() > 0)
  	InitDeviceMetrics(d2d_distances, d2h_distances, d_compute_perfs, 
  								d_mem_perfs, d_lmem_perfs, filter_indices);
  pthread_mutex_init(&mutex_mems_, NULL);
  pthread_mutex_init(&mutex_samplers_, NULL);

  InitImageInfo();
  SNUCL_INFO("Num properties: %d\n", num_properties_);
}

CLContext::~CLContext() {
  unsigned int device_id = 0;
  unsigned int host_id = 0;
  for(host_id = 0; host_id < hosts_.size(); host_id++)
  {
  	devices_hosts_distances_[host_id].clear();
  }
  devices_hosts_distances_.clear();
  hosts_.clear();
  for(device_id = 0; device_id < devices_.size(); device_id++)
  {
  	devices_devices_distances_[device_id].clear();
  }
  devices_devices_distances_.clear();
  devices_compute_perf_.clear();
  devices_memory_perf_.clear();
  devices_lmemory_perf_.clear();

  for (vector<CLDevice*>::iterator it = devices_.begin();
       it != devices_.end();
       ++it) {
    (*it)->Release();
  }
  if (properties_ != NULL)
    free(properties_);
  if (callback_ != NULL)
    delete callback_;

  pthread_mutex_destroy(&mutex_mems_);
  pthread_mutex_destroy(&mutex_samplers_);
}

void CLContext::InitDeviceMetrics(const std::vector<perf_vector> &d2d_distances,
  						 const std::vector<perf_vector> &d2h_distances,
  						 const perf_vector &d_compute_perfs,
  						 const perf_vector &d_mem_perfs,
  						 const perf_vector &d_lmem_perfs,
						 const std::vector<size_t> &filter_indices
						 )
{
	// Extract performance numbers of 'selected or filtered devices'
	// to the context-specific performance vectors
	unsigned int device_id = 0;
	unsigned int host_id = 0;
	size_t devices_size = filter_indices.size();
	size_t hosts_size = hosts_.size();
	SNUCL_INFO("Number of hosts: %u\n", hosts_size);
	SNUCL_INFO("Number of devices: %u\n", devices_size);
	devices_hosts_distances_.resize(hosts_size);
  	for(host_id = 0; host_id < hosts_size; host_id++)
	{
		devices_hosts_distances_[host_id].resize(devices_size);
	}

	devices_devices_distances_.resize(devices_size);
	for(device_id = 0; device_id < devices_size; device_id++)
	{
		devices_devices_distances_[device_id].resize(devices_size);
	}

	devices_compute_perf_.resize(devices_size);
	devices_memory_perf_.resize(devices_size);
	devices_lmemory_perf_.resize(devices_size);

  	for(host_id = 0; host_id < hosts_size; host_id++)
	{
		for(int next_device_id = 0; next_device_id < devices_size; next_device_id++)
		{
			unsigned int global_next_device_id = filter_indices[next_device_id];
			devices_hosts_distances_[host_id][next_device_id].first = d2h_distances[host_id][global_next_device_id];
			devices_hosts_distances_[host_id][next_device_id].second = next_device_id; 
			SNUCL_INFO("[%d][%d] = [%d][%d] (sizes: %d, %d)\n", next_device_id, host_id, global_next_device_id, host_id, devices_hosts_distances_[host_id].size(), d2h_distances[host_id].size());
		}
	}

	for(device_id = 0; device_id < devices_size; device_id++)
	{
		unsigned int global_device_id = filter_indices[device_id];
		devices_memory_perf_[device_id].first = d_mem_perfs[global_device_id];
		devices_memory_perf_[device_id].second = device_id;

		devices_lmemory_perf_[device_id].first = d_lmem_perfs[global_device_id];
		devices_lmemory_perf_[device_id].second = device_id;

		devices_compute_perf_[device_id].first = d_compute_perfs[global_device_id];
		devices_compute_perf_[device_id].second = device_id;

		SNUCL_INFO("[%d][%d] = [%d][%d]\n", device_id, device_id, global_device_id, global_device_id);
		devices_devices_distances_[device_id][device_id].first = d2d_distances[global_device_id][global_device_id];
		devices_devices_distances_[device_id][device_id].second = device_id;

		for(int next_device_id = device_id+1; next_device_id < devices_size; next_device_id++)
		{
			unsigned int global_next_device_id = filter_indices[next_device_id];
			devices_devices_distances_[device_id][next_device_id].first = d2d_distances[global_device_id][global_next_device_id];
			devices_devices_distances_[device_id][next_device_id].second = next_device_id;

			SNUCL_INFO("[%d][%d] = [%d][%d] (sizes: %d, %d)\n", next_device_id, device_id, global_next_device_id, global_device_id, devices_devices_distances_[next_device_id].size(), d2d_distances[global_next_device_id].size());
			devices_devices_distances_[next_device_id][device_id].first = d2d_distances[global_next_device_id][global_device_id];
			devices_devices_distances_[next_device_id][device_id].second = device_id;
		}
	}

	// Test the sort functionality
	print_perf_vector(devices_compute_perf_, "Compute");
	print_perf_vector(devices_memory_perf_, "Memory");
	print_perf_vector(devices_lmemory_perf_, "Local Memory");
	// Populate the sorted/ordered vectors
	std::sort(devices_compute_perf_.begin(), devices_compute_perf_.end(), sort_pred());
	std::sort(devices_memory_perf_.begin(), devices_memory_perf_.end(), sort_pred());
	std::sort(devices_lmemory_perf_.begin(), devices_lmemory_perf_.end(), sort_pred());
  	for(host_id = 0; host_id < hosts_size; host_id++)
	{
		print_perf_vector(devices_hosts_distances_[host_id], "D2H Vector[i]");
		std::sort(devices_hosts_distances_[host_id].begin(), devices_hosts_distances_[host_id].end(), sort_pred());
		print_perf_vector(devices_hosts_distances_[host_id], "D2H Vector[i]");
	}
	for(device_id = 0; device_id < devices_size; device_id++)
	{
		std::sort(devices_devices_distances_[device_id].begin(), devices_devices_distances_[device_id].end(), sort_pred());
	}
	// Test the sort functionality
	print_perf_vector(devices_compute_perf_, "Compute");
	print_perf_vector(devices_memory_perf_, "Memory");
	print_perf_vector(devices_lmemory_perf_, "Local Memory");
}

void CLContext::print_perf_vector(const perf_order_vector &vec, const char *vec_name)
{
	std::cout << " --------- " << vec_name << " ----------- " << std::endl;
	for(int i = 0; i < vec.size(); i++)
	{
		std::cout << "[" << vec[i].second << "]: " << vec[i].first << "\t";
	}
	std::cout << std::endl;
}

int CLContext::GetCurrentHostIDx() {
	//gCommandTimer.Start();
	unsigned int chosen_device_id = 0;
	unsigned int chosen_host_id = 0;
	const std::vector<CLDevice*> devices = this->devices();
	const std::vector<hwloc_obj_t> hosts = this->hosts();
	// Find current host cpuset index/hwloc_obj_t
	CLPlatform *platform = CLPlatform::GetPlatform();
	hwloc_topology_t topology = platform->HWLOCTopology();
	hwloc_cpuset_t cpuset;
	cpuset = hwloc_bitmap_alloc();
	hwloc_bitmap_zero(cpuset);
	hwloc_get_cpubind(topology, cpuset, HWLOC_CPUBIND_THREAD);
	hwloc_obj_t cpuset_obj = hwloc_get_next_obj_covering_cpuset_by_type(topology, cpuset, HWLOC_OBJ_NODE, NULL);
	assert(cpuset_obj != NULL);

	std::vector<CLContext::perf_order_vector> d2h_distances = this->d2h_distances();
	for(unsigned int idx = 0; idx < hosts.size(); idx++)
	{
		//SNUCL_INFO("Comparing cpuset obj: %p Hosts hwloc ptr[%d]: %p\n", cpuset_obj, idx, hosts[idx]);
		//SNUCL_INFO("Chosen ones...host ID: %u/%u and device ID: %u/%u\n", chosen_host_id, hosts.size(), chosen_device_id, devices.size());
		if(hosts[idx] == cpuset_obj)
		{
			// choose this to find distance between this cpuset and all devices
			chosen_host_id = idx;
			// Nearest device will be at d2h_distances[idx][0];
			chosen_device_id = d2h_distances[idx][0].second;
			//SNUCL_INFO("Chosen ones...host ID: %u/%u and device ID: %u/%u\n", chosen_host_id, hosts.size(), chosen_device_id, devices.size());
		}
	}

	hwloc_bitmap_free(cpuset);
    return chosen_host_id;
	//gCommandTimer.Stop();
}

#if 1
bool CLContext::isEpochRecorded(std::string epoch) {
	if(epochPerformances_.find(epoch) != epochPerformances_.end())
		return true;
	return false;
}

std::vector<double> CLContext::getEpochCosts(std::string epoch) {
	//if(isEpochRecorded(epoch))
		return epochPerformances_[epoch];
	//return std::vector<double>(0);
}

void CLContext::recordEpoch(std::string epoch, std::vector<double> &performances) {
	epochPerformances_[epoch].resize(performances.size());
	for(int i = 0; i < performances.size(); i++) {
		epochPerformances_[epoch][i] = performances[i];
	}
//	epochPerformances_[epoch] = performances;
}
#endif
cl_int CLContext::GetContextInfo(cl_context_info param_name,
                                 size_t param_value_size, void* param_value,
                                 size_t* param_value_size_ret) {
  switch (param_name) {
    GET_OBJECT_INFO_T(CL_CONTEXT_REFERENCE_COUNT, cl_uint, ref_cnt());
    GET_OBJECT_INFO_T(CL_CONTEXT_NUM_DEVICES, cl_uint, devices_.size());
    case CL_CONTEXT_DEVICES: {
      size_t size = sizeof(cl_device_id) * devices_.size();
      if (param_value) {
        if (param_value_size < size) return CL_INVALID_VALUE;
        for (size_t i = 0; i < devices_.size(); i++)
          ((cl_device_id*)param_value)[i] = devices_[i]->st_obj();
      }
      if (param_value_size_ret) *param_value_size_ret = size;
      break;
    }
    GET_OBJECT_INFO_A(CL_CONTEXT_PROPERTIES, cl_context_properties,
                      properties_, num_properties_);
    default: return CL_INVALID_VALUE;
  }
  return CL_SUCCESS;
}

cl_int CLContext::GetSupportedImageFormats(cl_mem_flags flags,
                                           cl_mem_object_type image_type,
                                           cl_uint num_entries,
                                           cl_image_format* image_formats,
                                           cl_uint* num_image_formats) {
  // Currently we ignore flags and image_type
  if (image_formats) {
    for (size_t i = 0; i < supported_image_formats_.size(); i++) {
      if (i < num_entries)
        image_formats[i] = supported_image_formats_[i];
    }
  }
  if (num_image_formats) *num_image_formats = supported_image_formats_.size();
  return CL_SUCCESS;
}

bool CLContext::IsValidDevice(CLDevice* device) {
  return (find(devices_.begin(), devices_.end(), device) != devices_.end());
}

bool CLContext::IsValidDevices(cl_uint num_devices,
                               const cl_device_id* device_list) {
  for (cl_uint i = 0; i < num_devices; i++) {
    if (device_list[i] == NULL || !IsValidDevice(device_list[i]->c_obj))
      return false;
  }
  return true;
}

bool CLContext::IsValidMem(cl_mem mem) {
  bool valid = false;
  pthread_mutex_lock(&mutex_mems_);
  for (vector<CLMem*>::iterator it = mems_.begin();
       it != mems_.end();
       ++it) {
    if ((*it)->st_obj() == mem) {
      valid = true;
      break;
    }
  }
  pthread_mutex_unlock(&mutex_mems_);
  return valid;
}

bool CLContext::IsValidSampler(cl_sampler sampler) {
  bool valid = false;
  pthread_mutex_lock(&mutex_samplers_);
  for (vector<CLSampler*>::iterator it = samplers_.begin();
       it != samplers_.end();
       ++it) {
    if ((*it)->st_obj() == sampler) {
      valid = true;
      break;
    }
  }
  pthread_mutex_unlock(&mutex_samplers_);
  return valid;
}

bool CLContext::IsSupportedImageFormat(cl_mem_flags flags,
                                       cl_mem_object_type image_type,
                                       const cl_image_format* image_format) {
  // Currently we ignore flags and image_type
  for (vector<cl_image_format>::iterator it = supported_image_formats_.begin();
       it != supported_image_formats_.end();
       ++it) {
    if ((*it).image_channel_order == image_format->image_channel_order &&
        (*it).image_channel_data_type == image_format->image_channel_data_type)
      return true;
  }
  return false;
}

bool CLContext::IsSupportedImageSize(const cl_image_desc* image_desc) {
  cl_mem_object_type type = image_desc->image_type;
  size_t width = image_desc->image_width;
  switch (type) {
    case CL_MEM_OBJECT_IMAGE2D:
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
    case CL_MEM_OBJECT_IMAGE1D:
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
      if (width > supported_image2d_max_width_) return false;
      break;
    case CL_MEM_OBJECT_IMAGE3D:
      if (width > supported_image3d_max_width_) return false;
      break;
    case CL_MEM_OBJECT_IMAGE1D_BUFFER:
      if (width > supported_image_max_buffer_size_) return false;
      break;
    default: break;
  }
  size_t height = image_desc->image_height;
  switch (type) {
    case CL_MEM_OBJECT_IMAGE2D:
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
      if (height > supported_image2d_max_height_) return false;
      break;
    case CL_MEM_OBJECT_IMAGE3D:
      if (height > supported_image3d_max_height_) return false;
      break;
    default: break;
  }
  size_t depth = image_desc->image_depth;
  if (type == CL_MEM_OBJECT_IMAGE3D) {
    if (depth > supported_image3d_max_depth_) return false;
  }
  size_t array_size = image_desc->image_array_size;
  if (type == CL_MEM_OBJECT_IMAGE1D_ARRAY ||
      type == CL_MEM_OBJECT_IMAGE2D_ARRAY) {
    if (array_size > supported_image_max_array_size_) return false;
  }
}

void CLContext::AddMem(CLMem* mem) {
  pthread_mutex_lock(&mutex_mems_);
  mems_.push_back(mem);
  pthread_mutex_unlock(&mutex_mems_);
}

void CLContext::RemoveMem(CLMem* mem) {
  pthread_mutex_lock(&mutex_mems_);
  vector<CLMem*>::iterator it = find(mems_.begin(), mems_.end(), mem);
  if (it != mems_.end())
    mems_.erase(it);
  pthread_mutex_unlock(&mutex_mems_);
}

void CLContext::AddSampler(CLSampler* sampler) {
  pthread_mutex_lock(&mutex_samplers_);
  samplers_.push_back(sampler);
  pthread_mutex_unlock(&mutex_samplers_);
}

void CLContext::RemoveSampler(CLSampler* sampler) {
  pthread_mutex_lock(&mutex_samplers_);
  vector<CLSampler*>::iterator it = find(samplers_.begin(), samplers_.end(),
                                         sampler);
  if (it != samplers_.end())
    samplers_.erase(it);
  pthread_mutex_unlock(&mutex_samplers_);
}

void CLContext::SetErrorNotificationCallback(
    ContextErrorNotificationCallback* callback) {
  if (callback_ != NULL)
    delete callback_;
  callback_ = callback;
}

void CLContext::NotifyError(const char* errinfo, const void* private_info,
                            size_t cb) {
  if (callback_ != NULL)
    callback_->run(errinfo, private_info, cb);
}

void CLContext::InitImageInfo() {
  image_supported_ = false;
  supported_image2d_max_width_ = -1;
  supported_image2d_max_height_ = -1;
  supported_image3d_max_width_ = -1;
  supported_image3d_max_height_ = -1;
  supported_image3d_max_depth_ = -1;
  supported_image_max_buffer_size_ = -1;
  supported_image_max_array_size_ = -1;
  for (vector<CLDevice*>::iterator it = devices_.begin();
       it != devices_.end();
       ++it) {
    CLDevice* device = *it;
    if (device->IsImageSupported())
      image_supported_ = true;
    device->JoinSupportedImageSize(supported_image2d_max_width_,
                                   supported_image2d_max_height_,
                                   supported_image3d_max_width_,
                                   supported_image3d_max_height_,
                                   supported_image3d_max_depth_,
                                   supported_image_max_buffer_size_,
                                   supported_image_max_array_size_);
  }
  /*
   * Currently SnuCL only supports the minimum set of image formats.
   * See Section 5.3.2.1 of the OpenCL 1.2 specification
   */
  const cl_channel_order orders[] = {CL_RGBA, CL_BGRA};
  const cl_channel_type data_types[] = {
      CL_UNORM_INT8, CL_UNORM_INT16, CL_SIGNED_INT8, CL_SIGNED_INT16,
      CL_SIGNED_INT32, CL_UNSIGNED_INT8, CL_UNSIGNED_INT16, CL_UNSIGNED_INT32,
      CL_HALF_FLOAT, CL_FLOAT};
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < 10; j++) {
      cl_image_format format = {orders[i], data_types[j]};
      supported_image_formats_.push_back(format);
    }
}
