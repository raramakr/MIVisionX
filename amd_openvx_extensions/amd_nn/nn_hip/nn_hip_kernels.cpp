/*
Copyright (c) 2015 - 2020 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "../../../amd_openvx/openvx/hipvx/hip_common_funcs.h"
#include "nn_hip_host_decls.h"
#include "hip/hip_fp16.h"

// ----------------------------------------------------------------------------
// Neural Network kernels for hip backend
// ----------------------------------------------------------------------------

template <typename T>
__global__ void __attribute__((visibility("default")))
Hip_Gather_layer(uchar* in, uint in_offset, uint4 in_stride, uchar* ind, uint ind_offset, uint4 ind_stride,
 uchar* out, uint out_offset, uint4 out_stride, uint axis) {

   uint x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
   uint y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;
   uint c = hipBlockDim_z * hipBlockIdx_z + hipThreadIdx_z;

   int indices = *(int*)&ind[ind_offset + y * ind_stride.x];
   T value;
   uint offset;
   if (axis == 0) {
       value = *(T*)&in[in_offset + x * in_stride.x + indices * in_stride.y + c * in_stride.z];
       offset = out_offset + x * out_stride.x + y * out_stride.y + c * out_stride.z;
   } else if (axis == 1) {
       value = *(T*)&in[in_offset + indices * in_stride.x + c * in_stride.y];
       offset = out_offset + y * out_stride.x + c * out_stride.y;
   } else if (axis == 2) {
       value = *(T*)&in[in_offset + c * in_stride.x];
       offset = out_offset + c * out_stride.x;
   }
   out += offset;
   *(T *)&out[0] = value;
}

int HipExec_Gather_layer(hipStream_t stream, dim3 globalThreads, dim3 localThreads, vx_enum type, uchar* in,
    uint in_offset, uint4 in_stride, uchar* ind, uint ind_offset, uint4 ind_stride, uchar* out, uint out_offset,
    uint4 out_stride, uint axis) {

    if (type == VX_TYPE_FLOAT32) {
        hipLaunchKernelGGL(Hip_Gather_layer<float>, dim3(ceil((float)globalThreads.x/localThreads.x),
            ceil((float)globalThreads.y/localThreads.y), ceil((float)globalThreads.z/localThreads.z)),
            dim3(localThreads.x, localThreads.y, localThreads.z), 0, stream, in, in_offset, in_stride,
            ind, ind_offset, ind_stride, out, out_offset, out_stride, axis);
    } else {
        hipLaunchKernelGGL(Hip_Gather_layer<__half>, dim3(ceil((float)globalThreads.x/localThreads.x),
            ceil((float)globalThreads.y/localThreads.y), ceil((float)globalThreads.z/localThreads.z)),
            dim3(localThreads.x, localThreads.y, localThreads.z), 0, stream, in, in_offset, in_stride,
            ind, ind_offset, ind_stride, out, out_offset, out_stride, axis);
    }

    return VX_SUCCESS;
}

template <typename T>
__global__ void __attribute__((visibility("default")))
Hip_Tile_layer(uchar* in, uint in_offset, uint4 in_stride, uint4 in_dims, uchar* rep, uint rep_offset,
    uint4 rep_stride, uchar* out, uint out_offset, uint4 out_stride) {

   uint x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
   uint y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;
   uint z = hipBlockDim_z * hipBlockIdx_z + hipThreadIdx_z;

   uint nx = x % in_dims.x;
   uint ny = y % in_dims.y;
   uint nz = z % in_dims.z;

   T value = *(T*)&in[in_offset + nx * in_stride.x + ny * in_stride.y + nz * in_stride.z];
   uint offset = out_offset + x * out_stride.x + y * out_stride.y + z * out_stride.z;
   out += offset;
   *(T *)&out[0] = value;
}

int HipExec_Tile_layer(hipStream_t stream, dim3 globalThreads, dim3 localThreads, vx_enum type, uchar* in,
    uint in_offset, uint4 in_stride, uint4 in_dims, uchar* rep, uint rep_offset, uint4 rep_stride, uchar* out,
    uint out_offset, uint4 out_stride) {

    if (type == VX_TYPE_FLOAT32) {
        hipLaunchKernelGGL(Hip_Tile_layer<float>, dim3(ceil((float)globalThreads.x/localThreads.x),
            ceil((float)globalThreads.y/localThreads.y), ceil((float)globalThreads.z/localThreads.z)),
            dim3(localThreads.x, localThreads.y, localThreads.z), 0, stream, in, in_offset, in_stride,
            in_dims, rep, rep_offset, rep_stride, out, out_offset, out_stride);
    } else {
        hipLaunchKernelGGL(Hip_Tile_layer<__half>, dim3(ceil((float)globalThreads.x/localThreads.x),
            ceil((float)globalThreads.y/localThreads.y), ceil((float)globalThreads.z/localThreads.z)),
            dim3(localThreads.x, localThreads.y, localThreads.z), 0, stream, in, in_offset, in_stride,
            in_dims, rep, rep_offset, rep_stride, out, out_offset, out_stride);
    }

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_Cast_layer_int32_float_v(uchar* in, uint in_offset, uint4 in_stride, uchar* out, uint out_offset, uint4 out_stride) {
   uint x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 4;
   uint y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;
   uint z = hipBlockDim_z * hipBlockIdx_z + hipThreadIdx_z;
   in += in_offset + z * in_stride.z + y * in_stride.y + x * in_stride.x;
   out += out_offset + z * out_stride.z + y * out_stride.y + x * out_stride.x;
   float4 f = *(float4 *)in;
   *(int4 *)&out[0] = make_int4(__float2int_rd(f.x), __float2int_rd(f.y), __float2int_rd(f.z), __float2int_rd(f.w));
}

__global__ void __attribute__((visibility("default")))
Hip_Cast_layer_int64_float_v(uchar* in, uint in_offset, uint4 in_stride, uchar* out, uint out_offset, uint4 out_stride) {
   uint x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 4;
   uint y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;
   uint z = hipBlockDim_z * hipBlockIdx_z + hipThreadIdx_z;
   in += in_offset + z * in_stride.z + y * in_stride.y + x * in_stride.x;
   out += out_offset + z * out_stride.z + y * out_stride.y + x * out_stride.x;
   float4 f = *(float4 *)in;
   *(longlong4 *)&out[0] = make_longlong4(__float2ll_rd(f.x), __float2ll_rd(f.y), __float2ll_rd(f.z), __float2ll_rd(f.w));
}

__global__ void __attribute__((visibility("default")))
Hip_Cast_layer_float_float_v(uchar* in, uint in_offset, uint4 in_stride, uchar* out, uint out_offset, uint4 out_stride) {
   uint x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 4;
   uint y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;
   uint z = hipBlockDim_z * hipBlockIdx_z + hipThreadIdx_z;
   in += in_offset + z * in_stride.z + y * in_stride.y + x * in_stride.x;
   out += out_offset + z * out_stride.z + y * out_stride.y + x * out_stride.x;
   *(float4 *)&out[0] = *(float4 *)in;
}

__global__ void __attribute__((visibility("default")))
Hip_Cast_layer_int64_int32_v(uchar* in, uint in_offset, uint4 in_stride, uchar* out, uint out_offset, uint4 out_stride) {
   uint x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 4;
   uint y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;
   uint z = hipBlockDim_z * hipBlockIdx_z + hipThreadIdx_z;
   in += in_offset + z * in_stride.z + y * in_stride.y + x * in_stride.x;
   out += out_offset + z * out_stride.z + y * out_stride.y + x * out_stride.x;
   int4 f = *(int4 *)in;
   *(longlong4 *)&out[0] = make_longlong4(f.x, f.y, f.z, f.w);
}

__global__ void __attribute__((visibility("default")))
Hip_Cast_layer_int32_int64_v(uchar* in, uint in_offset, uint4 in_stride, uchar* out, uint out_offset, uint4 out_stride) {
   uint x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 4;
   uint y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;
   uint z = hipBlockDim_z * hipBlockIdx_z + hipThreadIdx_z;
   in += in_offset + z * in_stride.z + y * in_stride.y + x * in_stride.x;
   out += out_offset + z * out_stride.z + y * out_stride.y + x * out_stride.x;
   longlong4 f = *(longlong4 *)in;
   *(int4 *)&out[0] = make_int4(f.x, f.y, f.z, f.w);
}

__global__ void __attribute__((visibility("default")))
Hip_Cast_layer_int32_float(uchar* in, uint in_offset, uint4 in_stride, uchar* out, uint out_offset, uint4 out_stride) {
   uint x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
   uint y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;
   uint z = hipBlockDim_z * hipBlockIdx_z + hipThreadIdx_z;
   in += in_offset + z * in_stride.z + y * in_stride.y + x * in_stride.x;
   out += out_offset + z * out_stride.z + y * out_stride.y + x * out_stride.x;
   *(int *)&out[0] = __float2int_rd(*(float *)in);
}

__global__ void __attribute__((visibility("default")))
Hip_Cast_layer_int64_float(uchar* in, uint in_offset, uint4 in_stride, uchar* out, uint out_offset, uint4 out_stride) {
   uint x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
   uint y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;
   uint z = hipBlockDim_z * hipBlockIdx_z + hipThreadIdx_z;
   in += in_offset + z * in_stride.z + y * in_stride.y + x * in_stride.x;
   out += out_offset + z * out_stride.z + y * out_stride.y + x * out_stride.x;
   *(long long int*)&out[0] = __float2ll_rd(*(float *)in);
}

__global__ void __attribute__((visibility("default")))
Hip_Cast_layer_float_float(uchar* in, uint in_offset, uint4 in_stride, uchar* out, uint out_offset, uint4 out_stride) {
   uint x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
   uint y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;
   uint z = hipBlockDim_z * hipBlockIdx_z + hipThreadIdx_z;
   in += in_offset + z * in_stride.z + y * in_stride.y + x * in_stride.x;
   out += out_offset + z * out_stride.z + y * out_stride.y + x * out_stride.x;
   *(float *)&out[0] = *(float *)in;
}

__global__ void __attribute__((visibility("default")))
Hip_Cast_layer_int64_int32(uchar* in, uint in_offset, uint4 in_stride, uchar* out, uint out_offset, uint4 out_stride) {
   uint x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
   uint y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;
   uint z = hipBlockDim_z * hipBlockIdx_z + hipThreadIdx_z;
   in += in_offset + z * in_stride.z + y * in_stride.y + x * in_stride.x;
   out += out_offset + z * out_stride.z + y * out_stride.y + x * out_stride.x;
   int f = *(int *)in;
   *(long long int*)&out[0] = (long long int)f;
}

__global__ void __attribute__((visibility("default")))
Hip_Cast_layer_int32_int64(uchar* in, uint in_offset, uint4 in_stride, uchar* out, uint out_offset, uint4 out_stride) {
   uint x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
   uint y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;
   uint z = hipBlockDim_z * hipBlockIdx_z + hipThreadIdx_z;
   in += in_offset + z * in_stride.z + y * in_stride.y + x * in_stride.x;
   out += out_offset + z * out_stride.z + y * out_stride.y + x * out_stride.x;
   long long int f = *(long long int*)in;
   *(int *)&out[0] = (int)f;
}

int HipExec_Cast_layer(hipStream_t stream, dim3 globalThreads, dim3 localThreads, vx_enum input_type, vx_enum output_type, uchar* in, uint in_offset,
    uint4 in_stride, uchar* out, uint out_offset, uint4 out_stride) {

    bool input_element_count_multiple_of_4 = ((globalThreads.x * globalThreads.y) & 3) ? false : true;
    dim3 gridDim = dim3(ceil((float)globalThreads.x/localThreads.x),
                        ceil((float)globalThreads.y/localThreads.y),
                        ceil((float)globalThreads.z/localThreads.z));
    dim3 blockDim = dim3(localThreads.x, localThreads.y, localThreads.z);
    if(input_element_count_multiple_of_4) {
        if(input_type == VX_TYPE_FLOAT32) {
            if(output_type == VX_TYPE_INT32) {
                hipLaunchKernelGGL(Hip_Cast_layer_int32_float_v, gridDim, blockDim, 0, stream, in, in_offset, in_stride, out, out_offset, out_stride);
            } else if(output_type == VX_TYPE_INT64) {
                hipLaunchKernelGGL(Hip_Cast_layer_int64_float_v, gridDim, blockDim, 0, stream, in, in_offset, in_stride, out, out_offset, out_stride);
            } else if(output_type == VX_TYPE_FLOAT32) {
                hipLaunchKernelGGL(Hip_Cast_layer_float_float_v, gridDim, blockDim, 0, stream, in, in_offset, in_stride, out, out_offset, out_stride);
            }
        } else if(input_type == VX_TYPE_INT32) {
            if(output_type == VX_TYPE_INT64) {
                hipLaunchKernelGGL(Hip_Cast_layer_int64_int32_v, gridDim, blockDim, 0, stream, in, in_offset, in_stride, out, out_offset, out_stride);
            }
        } else if(input_type == VX_TYPE_INT64) {
            if(output_type == VX_TYPE_INT32) {
                hipLaunchKernelGGL(Hip_Cast_layer_int32_int64_v, gridDim, blockDim, 0, stream, in, in_offset, in_stride, out, out_offset, out_stride);
            }
        }
    } else {
        if(input_type == VX_TYPE_FLOAT32) {
            if(output_type == VX_TYPE_INT32) {
                hipLaunchKernelGGL(Hip_Cast_layer_int32_float, gridDim, blockDim, 0, stream, in, in_offset, in_stride, out, out_offset, out_stride);
            } else if(output_type == VX_TYPE_INT64) {
                hipLaunchKernelGGL(Hip_Cast_layer_int64_float, gridDim, blockDim, 0, stream, in, in_offset, in_stride, out, out_offset, out_stride);
            } else if(output_type == VX_TYPE_FLOAT32) {
                hipLaunchKernelGGL(Hip_Cast_layer_float_float, gridDim, blockDim, 0, stream, in, in_offset, in_stride, out, out_offset, out_stride);
            }
        } else if(input_type == VX_TYPE_INT32) {
            if(output_type == VX_TYPE_INT64) {
                hipLaunchKernelGGL(Hip_Cast_layer_int64_int32, gridDim, blockDim, 0, stream, in, in_offset, in_stride, out, out_offset, out_stride);
            }
        } else if(input_type == VX_TYPE_INT64) {
            if(output_type == VX_TYPE_INT32) {
                hipLaunchKernelGGL(Hip_Cast_layer_int32_int64, gridDim, blockDim, 0, stream, in, in_offset, in_stride, out, out_offset, out_stride);
            }
        }
    }

    return VX_SUCCESS;
}