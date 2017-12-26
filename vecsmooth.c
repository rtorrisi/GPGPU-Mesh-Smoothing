#define OCL_PLATFORM 1
#define OCL_DEVICE 0
#define OCL_FILENAME "vecsmooth412.ocl"

#include "ocl_boiler.h"

int error(const char *msg) {
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

int lws_cli;
int vec_kernel;

size_t preferred_wg_init;
size_t preferred_wg_smooth;

cl_event init(cl_command_queue queue, cl_kernel init_kernel, cl_mem d_v1, cl_int nels) {
	size_t lws[] = { lws_cli };
	/* se il local work size Ã¨ stato specificato, arrotondiamo il
	 * global work size al multiplo successivo del lws, altrimenti,
	 * lo arrotondiamo al multiplo successivo della base preferita
	 * dalla piattaforma */
	size_t gws[] = {
		round_mul_up(nels/vec_kernel,
			lws_cli ? lws[0] : preferred_wg_init)
	};
	cl_event init_evt;
	cl_int err;

	printf("init gws: %d | %d | %d => %d\n", nels, lws_cli, preferred_wg_init, gws[0]);

	/* setting arguments */
	err = clSetKernelArg(init_kernel, 0, sizeof(d_v1), &d_v1);
	ocl_check(err, "set init arg 0");
	
	err = clSetKernelArg(init_kernel, 1, sizeof(nels), &nels);
	ocl_check(err, "set init arg 1");

	
	err = clEnqueueNDRangeKernel(queue, init_kernel,
		1, NULL, gws, (lws_cli ? lws : NULL), /* griglia di lancio */
		0, NULL, /* waiting list */
		&init_evt);
	ocl_check(err, "enqueue kernel init");

	return init_evt;
}

cl_event smooth(cl_command_queue queue, cl_kernel smooth_kernel, cl_mem d_vsmooth, cl_mem d_v1, cl_int nels, cl_event init_evt) {
	size_t lws[] = { lws_cli };
	/* il numero di workitem è pari o al numero di elementi,
	 * o al numero di quartine */
	cl_int work_items = vec_kernel == 4 ? nels/4 : nels;
	/* se il local work size è stato specificato, arrotondiamo il
	 * global work size al multiplo successivo del lws, altrimenti,
	 * lo arrotondiamo al multiplo successivo della base preferita
	 * dalla piattaforma */
	size_t gws[] = {
		round_mul_up(work_items,
			lws_cli ? lws[0] : preferred_wg_smooth)
	};
	cl_event smooth_evt;
	cl_int err;

	printf("smooth gws: %d | %d | %d => %d\n",
		nels, lws_cli, preferred_wg_smooth, gws[0]);

	/* setting arguments */
	
	err = clSetKernelArg(smooth_kernel, 0, sizeof(d_vsmooth), &d_vsmooth);
	ocl_check(err, "set smooth arg 0");
	
	err = clSetKernelArg(smooth_kernel, 1, sizeof(d_v1), &d_v1);
	ocl_check(err, "set smooth arg 1");
	
	err = clSetKernelArg(smooth_kernel, 2, sizeof(nels), &work_items);
	ocl_check(err, "set smooth arg 2");

	
	cl_event wait_list[] = { init_evt };
	err = clEnqueueNDRangeKernel(queue, smooth_kernel,
		1, NULL, gws, (lws_cli ? lws : NULL), /* griglia di lancio */
		1, wait_list, /* waiting list */
		&smooth_evt);
	ocl_check(err, "enqueue kernel smooth");

	return smooth_evt;
}

int main(int argc, char *argv[]) {
	
	if (argc < 2) error("sintassi: vecsmooth numels [lws]");
	
	int nels = atoi(argv[1]); /* numero di elementi */
	if (nels <= 0) error("il numero di elementi deve essere positivo");
	const size_t memsize = sizeof(int)*nels;

	if (argc >= 3) lws_cli = atoi(argv[2]); /* local work size */
	if (lws_cli < 0) error("il local work size deve essere non negativo");

	cl_platform_id platform_ID = select_platform();
	cl_device_id device_ID = select_device(platform_ID);
	cl_context context = create_context(platform_ID, device_ID);
	cl_command_queue queue = create_queue(context, device_ID);
	cl_program program = create_program(OCL_FILENAME, context, device_ID);
	cl_int err;
	
	/* Extract kernels */
	cl_kernel init_kernel = clCreateKernel(program, "init", &err);
	ocl_check(err, "create kernel init");
	
	cl_kernel smooth_kernel = clCreateKernel(program, "smooth", &err);
	ocl_check(err, "create kernel smooth");

	err = clGetKernelWorkGroupInfo(init_kernel, d,
		CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,
		sizeof(preferred_wg_init), &preferred_wg_init, NULL);
		
	err = clGetKernelWorkGroupInfo(smooth_kernel, d,
		CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,
		sizeof(preferred_wg_smooth), &preferred_wg_smooth, NULL);

	cl_mem d_v1 = clCreateBuffer(ctx, CL_MEM_READ_WRITE,
		memsize, NULL, &err);
	ocl_check(err, "create buffer v1");
	
	cl_mem d_vsmooth = clCreateBuffer(ctx, CL_MEM_WRITE_ONLY,
		memsize, NULL, &err);
	ocl_check(err, "create buffer vsmooth");

	cl_event init_evt = init(queue, init_kernel, d_v1, nels);
	cl_event smooth_evt = smooth(queue, smooth_kernel, d_vsmooth, d_v1, nels, init_evt);

	int *vsmooth = malloc(memsize);
	if (!vsmooth) error("alloc vsmooth");

	cl_event copy_evt;
	err = clEnqueueReadBuffer(queue, d_vsmooth, CL_TRUE,
		0, memsize, vsmooth,
		1, &smooth_evt, &copy_evt);
	ocl_check(err, "read buffer vsmooth");

	
	printf("init time:\t%gms\t%gGB/s\n", runtime_ms(init_evt), (1.0*memsize)/runtime_ns(init_evt));
	printf("smooth time:\t%gms\t%gGB/s\n", runtime_ms(smooth_evt), (1.0*memsize)/runtime_ns(smooth_evt));
	printf("copy time:\t%gms\t%gGB/s\n", runtime_ms(copy_evt), (1.0*memsize)/runtime_ns(copy_evt));

	return 0;
}
