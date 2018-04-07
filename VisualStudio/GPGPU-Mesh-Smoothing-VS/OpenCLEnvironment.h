#pragma once

#include "CL/cl.h"

class OpenCLEnvironment {
public:

	cl_platform_id platformID;
	cl_device_id deviceID;
	cl_context context;
	cl_command_queue queue;
	cl_program program;

	OpenCLEnvironment(const cl_uint platformIndex, const cl_uint deviceIndex, const char* programPath);
};

