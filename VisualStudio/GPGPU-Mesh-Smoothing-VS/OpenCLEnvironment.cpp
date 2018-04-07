#include "OpenCLEnvironment.h"
#include "ocl_boiler.h"
#include <iostream>

OpenCLEnvironment::OpenCLEnvironment(const cl_uint platformIndex, const cl_uint deviceIndex, const char * programPath)
{
	printf("\n=================== OPENCL INFO ====================\n");
	platformID = select_platform(platformIndex);
	deviceID = select_device(platformID, deviceIndex);
	context = create_context(platformID, deviceID);
	queue = create_queue(context, deviceID);
	program = create_program(programPath, context, deviceID);
}

