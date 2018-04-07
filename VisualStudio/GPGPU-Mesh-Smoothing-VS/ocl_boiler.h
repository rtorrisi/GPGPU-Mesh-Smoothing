#pragma once

/* A collection of functions wrapping the most common boilerplate
of OpenCL program. You can now reduce the boilerplate to:
*/

/* Include the headers defining the OpenCL host API */
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <CL/cl.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define BUFSIZE 4096

/* Check an OpenCL error status, printing a message and exiting
* in case of failure
*/
void ocl_check(cl_int err, const char *msg, ...);

// Return the ID of the platform specified in the OCL_PLATFORM
// environment variable (or the first one if none specified)
cl_platform_id select_platform(cl_uint platformIndex);

// Return the ID of the device (of the given platform p) specified in the
// OCL_DEVICE environment variable (or the first one if none specified)
cl_device_id select_device(cl_platform_id p, cl_uint deviceIndex);

// Create a one-device context
cl_context create_context(cl_platform_id p, cl_device_id d);

// Create a command queue for the given device in the given context
cl_command_queue create_queue(cl_context ctx, cl_device_id d);

// Compile the device part of the program, stored in the external
// file `fname`, for device `dev` in context `ctx`
cl_program create_program(const char * const fname, cl_context ctx, cl_device_id dev);

// Runtime of an event, in nanoseconds. Note that if NS is the
// runtimen of an event in nanoseconds and NB is the number of byte
// read and written during the event, NB/NS is the effective bandwidth
// expressed in GB/s
cl_ulong runtime_ns(cl_event evt);

// Runtime of an event, in milliseconds
double runtime_ms(cl_event evt);

/* round gws to the next multiple of lws */
size_t round_mul_up(size_t gws, size_t lws);