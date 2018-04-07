#include "Smoothing.h"
#include "OBJ.h"
#include "Timer.h"
#include "OpenCLEnvironment.h"
#include "ocl_boiler.h"
#include <iostream>
#include <algorithm>

#define OCL_PLATFORM 1
#define OCL_DEVICE 0

#define WRESTLERS "res/wrestlers.obj"
#define SUZANNE "res/suzanne.obj"
#define EGYPT "res/egypt.obj"
#define DRAGON "res/dragon.obj"
#define HUMAN "res/human.obj"
#define TEST "res/test.obj"
#define CUBE "res/cube_example.obj"
#define NOISECUBE "res/cube_noise.obj"

#define IN_MESH WRESTLERS
#define OUT_MESH "res/out.obj"

bool Smoothing::orderedUniqueInsert(std::vector< uint >*vertexAdjacents, const uint vertexID) {
	std::vector< uint >::iterator it;
	for (it = vertexAdjacents->begin(); it != vertexAdjacents->end() && *it < vertexID; it++) {}
	if (it == vertexAdjacents->end() || *it != vertexID) {
		vertexAdjacents->insert(it, vertexID);
		return true;
	}
	return false;
}

void Smoothing::parseBitFlags(const unsigned char flagsOpt) {
	coalescence = (flagsOpt & CommandOptions::Options::OptCoalescence);
	localMemory = (flagsOpt & CommandOptions::Options::OptLocalMemory);
	// coalescence access requires sortVertex = true
	sortVertex = (flagsOpt & CommandOptions::Options::OptSortVertex) || coalescence;
	sortAdjs = (flagsOpt & CommandOptions::Options::OptSortAdjs);
}

Smoothing::Smoothing(const OpenCLEnvironment* OCLenv, OBJ* obj, const unsigned char flagsOpt, const uint localWorkSize) {
	this->OCLenv = OCLenv;
	this->localWorkSize = localWorkSize;
	this->obj = new OBJ(*obj);
	obj_adjacents_arrayVector = nullptr;
	vertex4_array = nullptr;
	adjs_array = nullptr;
	adjCounter_array = nullptr;
	parseBitFlags(flagsOpt);
	init();
}

Smoothing::~Smoothing() {
	if (obj != nullptr) { delete obj; }
	if (vertex4_array != nullptr) { delete[]vertex4_array; }
	if (adjs_array != nullptr) { delete[]adjs_array; }
	if (obj_adjacents_arrayVector != nullptr) { delete[]obj_adjacents_arrayVector; }
	if (adjCounter_array != nullptr) { delete[]adjCounter_array; }
}

uint Smoothing::discoverAdjacents(const bool orderedInsert)
{
	unsigned int adjsCount = 0;
	// Discover adjacents vertex for each vertex
	obj_adjacents_arrayVector = new std::vector< uint >[nels];

	INIT_TIMER;
	START_TIMER;
	for (int i = 0; i<obj->facesVertexIndex_vector.size(); i += 3) {
		uint vertexID1 = obj->facesVertexIndex_vector[i] - 1;
		uint vertexID2 = obj->facesVertexIndex_vector[i + 1] - 1;
		uint vertexID3 = obj->facesVertexIndex_vector[i + 2] - 1;


		std::vector< uint >* adjacent1 = &obj_adjacents_arrayVector[vertexID1];
		std::vector< uint >* adjacent2 = &obj_adjacents_arrayVector[vertexID2];
		std::vector< uint >* adjacent3 = &obj_adjacents_arrayVector[vertexID3];

		if (orderedInsert && !sortVertex) {
			if (orderedUniqueInsert(adjacent1, vertexID2)) adjsCount++;
			if (orderedUniqueInsert(adjacent1, vertexID3)) adjsCount++;
			if (orderedUniqueInsert(adjacent2, vertexID1)) adjsCount++;
			if (orderedUniqueInsert(adjacent2, vertexID3)) adjsCount++;
			if (orderedUniqueInsert(adjacent3, vertexID1)) adjsCount++;
			if (orderedUniqueInsert(adjacent3, vertexID2)) adjsCount++;
		}
		else {
			if (std::find(adjacent1->begin(), adjacent1->end(), vertexID2) == adjacent1->end()) { adjacent1->push_back(vertexID2); adjsCount++; }
			if (std::find(adjacent1->begin(), adjacent1->end(), vertexID3) == adjacent1->end()) { adjacent1->push_back(vertexID3); adjsCount++; }
			if (std::find(adjacent2->begin(), adjacent2->end(), vertexID1) == adjacent2->end()) { adjacent2->push_back(vertexID1); adjsCount++; }
			if (std::find(adjacent2->begin(), adjacent2->end(), vertexID3) == adjacent2->end()) { adjacent2->push_back(vertexID3); adjsCount++; }
			if (std::find(adjacent3->begin(), adjacent3->end(), vertexID1) == adjacent3->end()) { adjacent3->push_back(vertexID1); adjsCount++; }
			if (std::find(adjacent3->begin(), adjacent3->end(), vertexID2) == adjacent3->end()) { adjacent3->push_back(vertexID2); adjsCount++; }
		}
	}
	PRINT_ELAPSED_TIME("discoverAdjacents", ELAPSED_TIME);
	return adjsCount;
}

void Smoothing::calcMinMaxAdjCount() {
	maxAdjsCount = minAdjsCount = (uint)obj_adjacents_arrayVector[0].size();

	for (uint i = 0; i<nels; i++) {
		uint currentAdjsCount = (uint)obj_adjacents_arrayVector[i].size();
		if (currentAdjsCount > maxAdjsCount) maxAdjsCount = currentAdjsCount;
		else if (currentAdjsCount < minAdjsCount) minAdjsCount = currentAdjsCount;
	}
}

Smoothing::vertex_struct** Smoothing::orderVertexByAdjCount(const bool sortAdjs) {

	// vertex_arrayStruct creation
	vertex_struct** vertex_arrayStruct = new vertex_struct*[nels];

	for (uint i = 0; i<nels; i++) {
		vertex_struct* v = new vertex_struct();

		v->currentIndex = i;
		v->obj_vertex_vector_Index = i;

		//number of vertices adjacents to vertex i
		v->adjsCount = (uint)obj_adjacents_arrayVector[i].size();
		vertex_arrayStruct[i] = v;
	}

	for (uint i = 0; i<nels; i++)
		for (uint index : obj_adjacents_arrayVector[i])
			vertex_arrayStruct[i]->adjs.push_back(vertex_arrayStruct[index]);

	// faces_arrayStruct creation
	uint facesCount = obj->getFacesCount();
	face_struct* faces_arrayStruct = new face_struct[facesCount];

	for (uint i = 0; i<facesCount; i++) {
		faces_arrayStruct[i].faceVertices_struct_array[0] = vertex_arrayStruct[obj->facesVertexIndex_vector[i * 3] - 1];
		faces_arrayStruct[i].faceVertices_struct_array[1] = vertex_arrayStruct[obj->facesVertexIndex_vector[i * 3 + 1] - 1];
		faces_arrayStruct[i].faceVertices_struct_array[2] = vertex_arrayStruct[obj->facesVertexIndex_vector[i * 3 + 2] - 1];
	}

	//COUNTING SORT
	countingSize = maxAdjsCount - minAdjsCount + 1;
	uint* counting_array = new uint[countingSize]{ 0 };


	for (uint i = 0; i<nels; i++) ++counting_array[obj_adjacents_arrayVector[i].size() - minAdjsCount];

	//counting sort sums
	for (uint i = 1; i<countingSize; i++) counting_array[i] += counting_array[i - 1];

	vertex_struct** orderedVertex_arrayStruct = new vertex_struct*[nels];

	//insert ordered vertex
	for (uint i = 0; i<nels; i++) {
		vertex_struct * currVertex = vertex_arrayStruct[i];
		uint currentAdjsCount = currVertex->adjsCount;

		//calc new orderedIndex from counting_array
		uint orderedIndex = --counting_array[currentAdjsCount - minAdjsCount];

		//set to vertex in struct the new orderedIndex
		currVertex->currentIndex = nels - 1 - orderedIndex;

		orderedVertex_arrayStruct[nels - 1 - orderedIndex] = currVertex;
	}

	// Update vertex index [1-based] of faces in obj
	for (uint i = 0; i<obj->getFacesCount(); i++) {
		obj->facesVertexIndex_vector[i * 3] = faces_arrayStruct[i].faceVertices_struct_array[0]->currentIndex + 1;
		obj->facesVertexIndex_vector[i * 3 + 1] = faces_arrayStruct[i].faceVertices_struct_array[1]->currentIndex + 1;
		obj->facesVertexIndex_vector[i * 3 + 2] = faces_arrayStruct[i].faceVertices_struct_array[2]->currentIndex + 1;
	}

	adjCounter_array = new uint[maxAdjsCount];

	for (uint i = 0; i<maxAdjsCount; i++)
		adjCounter_array[i] = 0;
	for (uint i = 0; i<maxAdjsCount; i++) {
		for (uint j = 0; j<nels; j++) {
			vertex_struct * currVertex = vertex_arrayStruct[j];
			if (currVertex->adjsCount >= i + 1) adjCounter_array[i]++;
		}
	}

	if (sortAdjs) {
		for (uint i = 0; i<nels; i++) {
			vertex_struct * currVertex = orderedVertex_arrayStruct[i];
			std::vector<vertex_struct*> * v = &(currVertex->adjs);

			std::sort(v->begin(), v->end(), vertex_struct_cmp);
		}
	}

	delete[]vertex_arrayStruct;
	delete[]faces_arrayStruct;
	return orderedVertex_arrayStruct;
}

void Smoothing::fillVertex4Array(float* vertex4_array) {
	INIT_TIMER;
	START_TIMER;
	uint currentAdjStartIndex = 0;

	for (uint i = 0; i<nels; i++) {
		glm::vec3 vertex = obj->vertex_vector[i];
		vertex4_array[4 * i] = vertex.x;
		vertex4_array[4 * i + 1] = vertex.y;
		vertex4_array[4 * i + 2] = vertex.z;

		uint currentAdjsCount = (uint)obj_adjacents_arrayVector[i].size();
		uint* adjIndexPtr = (uint*)(&vertex4_array[4 * i + 3]);
		*adjIndexPtr = ((uint)currentAdjStartIndex) << 6;
		*adjIndexPtr += (currentAdjsCount << 26) >> 26;

		currentAdjStartIndex += currentAdjsCount;
	}
	PRINT_ELAPSED_TIME("fillVertex4Array", ELAPSED_TIME);
}

void Smoothing::fillOrderedVertex4Array(float* vertex4_array, vertex_struct** orderedVertex_arrayStruct, const bool coalescence) {
	INIT_TIMER;
	START_TIMER;
	uint currentAdjStartIndex = 0;

	for (uint i = 0; i<nels; i++) {
		vertex_struct * currVertex = orderedVertex_arrayStruct[i];

		glm::vec3 vertex = obj->vertex_vector[currVertex->obj_vertex_vector_Index];
		vertex4_array[4 * i] = vertex.x;
		vertex4_array[4 * i + 1] = vertex.y;
		vertex4_array[4 * i + 2] = vertex.z;

		uint currentAdjsCount = currVertex->adjsCount;
		uint* adjIndexPtr = (uint*)(&vertex4_array[4 * i + 3]);

		if (coalescence) *adjIndexPtr = currentAdjsCount;
		else {
			*adjIndexPtr = ((uint)currentAdjStartIndex) << 6;
			*adjIndexPtr += (currentAdjsCount << 26) >> 26;
		}

		currentAdjStartIndex += currentAdjsCount;
	}
	PRINT_ELAPSED_TIME("fillOrderedVertex4Array", ELAPSED_TIME);
}

void Smoothing::fillVertexAdjsArray() {
	INIT_TIMER;
	START_TIMER;
	uint adjsIndex = 0;
	for (uint i = 0; i<nels; i++)
		for (uint currentAdjIndex : obj_adjacents_arrayVector[i])
			adjs_array[adjsIndex++] = currentAdjIndex;
	PRINT_ELAPSED_TIME("fillVertexAdjsArray", ELAPSED_TIME);
}

void Smoothing::fillOrderedVertexAdjsArray(vertex_struct** orderedVertex_arrayStruct, const bool coalescence) {
	INIT_TIMER;
	START_TIMER;
	uint adjsIndex = 0;

	if (coalescence) {
		for (uint i = 0; i<maxAdjsCount; i++)
			for (uint j = 0; j<nels; j++) {
				vertex_struct * currVertex = orderedVertex_arrayStruct[j];
				if (currVertex->adjsCount >= i + 1)
					adjs_array[adjsIndex++] = currVertex->adjs[i]->currentIndex;
			}
	}
	else { //TO-DO non coalescence access (change kernel adjs access)
		for (uint i = 0; i<nels; i++) {
			vertex_struct * currVertex = orderedVertex_arrayStruct[i];
			for (vertex_struct* adj : currVertex->adjs)
				adjs_array[adjsIndex++] = adj->currentIndex;
		}
	}

	PRINT_ELAPSED_TIME("fillOrderedVertexAdjsArray", ELAPSED_TIME);
}

void Smoothing::init() {
	printf("\n===================== INIT DATA ====================\n");
	printf(" > Initializing data...\n");

	nels = nadjs = minAdjsCount = maxAdjsCount = 0;
	meanAdjsCount = 0.0f;

	if (obj == nullptr || !obj->hasValidData()) {
		printf(" Error: Smoothing(OBJ) -> obj null or invalid data");
		exit(-1);
	}

	nels = obj->getVerticesCount();
	nadjs = discoverAdjacents(sortAdjs);

	meanAdjsCount = nadjs / (float)nels;
	//-----------------------------------
	vertex4_array = new float[4 * nels];
	adjs_array = new uint[nadjs];
	//-----------------------------------
	memsize = 4 * nels * sizeof(float);
	ajdsmemsize = nadjs * sizeof(uint);

	calcMinMaxAdjCount();

	if (sortVertex) {
		vertex_struct** orderedVertex_arrayStruct = orderVertexByAdjCount(sortAdjs);
		fillOrderedVertex4Array(vertex4_array, orderedVertex_arrayStruct, coalescence);
		fillOrderedVertexAdjsArray(orderedVertex_arrayStruct, coalescence);

		for (uint i = 0; i<nels; i++) delete orderedVertex_arrayStruct[i];
		delete[]orderedVertex_arrayStruct;
	}
	else {
		fillVertex4Array(vertex4_array);
		fillVertexAdjsArray();
	}
}

void Smoothing::execute(uint iterations, float lambda, float mi, const bool writeOBJ) {

	printf("\n================== SMOOTHING INFO ==================\n");
	std::cout << " # Iterations: " << iterations << std::endl;
	std::cout << " Lambda factor: " << lambda << std::endl;
	std::cout << " Mi factor: " << mi << std::endl;
	printf("\n===================== OBJ INFO =====================\n");
	std::cout << " Input path: " << obj->getPathName() << std::endl;
	std::cout << " Output path: " << OUT_MESH << std::endl;
	std::cout << " # Vertex: " << nels << std::endl;
	std::cout << " # obj_adjacents_arrayVector: " << nadjs << std::endl;
	std::cout << " # Vertex adjs range: [" << minAdjsCount << ";" << maxAdjsCount << "]" << std::endl;
	std::cout << " # Mean vertex adjs: " << meanAdjsCount << std::endl;\
		printf("\n==================== KERNEL OPT ====================\n");
	std::cout << " SortVertex: " << (sortVertex ? "true" : "false") << std::endl;
	std::cout << " SortAdjs: " << (sortAdjs ? "true" : "false") << std::endl;
	std::cout << " Coalescence: " << (coalescence ? "true" : "false") << std::endl;
	std::cout << " LocalMemory: " << (localMemory ? "true" : "false") << std::endl;
	std::cout << " LocalWorkSize: " << localWorkSize << std::endl;

	cl_int err;
	// Create Buffers
	cl_mem cl_vertex4_array = clCreateBuffer(OCLenv->context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, memsize, vertex4_array, &err);
	cl_mem cl_adjs_array = clCreateBuffer(OCLenv->context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, ajdsmemsize, adjs_array, &err);
	cl_mem cl_result_vertex4_array = clCreateBuffer(OCLenv->context, CL_MEM_READ_WRITE, memsize, NULL, &err);
	cl_mem cl_adjsCounter = nullptr;
	if (coalescence) cl_adjsCounter = clCreateBuffer(OCLenv->context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, maxAdjsCount * sizeof(uint), adjCounter_array, &err);

	// Extract kernels
	cl_kernel smooth_k;
	if (coalescence && localMemory) smooth_k = clCreateKernel(OCLenv->program, "smooth_coalescence_lmem", &err);
	else if (coalescence) smooth_k = clCreateKernel(OCLenv->program, "smooth_coalescence", &err);
	else if (localMemory) smooth_k = clCreateKernel(OCLenv->program, "smooth_lmem", &err);
	else smooth_k = clCreateKernel(OCLenv->program, "smooth", &err);
	ocl_check(err, "create kernel smooth");

	// Set preferred_wg size from device info
	err = clGetKernelWorkGroupInfo(smooth_k, OCLenv->deviceID, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(preferred_wg_smooth), &preferred_wg_smooth, NULL);

	printf("\n================== KERNEL LAUNCH ===================\n");

	cl_event* smooth_evts = new cl_event[iterations * 2];
	if (coalescence && localMemory) {
		for (uint iter = 0; iter<iterations; iter++) {
			smooth_evts[iter * 2] = smooth_coalescence_lmem(OCLenv->queue, smooth_k, cl_vertex4_array, cl_adjs_array, cl_adjsCounter, cl_result_vertex4_array, nels, lambda, !!iter, smooth_evts + (iter * 2) - 1);
			smooth_evts[iter * 2 + 1] = smooth_coalescence_lmem(OCLenv->queue, smooth_k, cl_result_vertex4_array, cl_adjs_array, cl_adjsCounter, cl_vertex4_array, nels, mi, 1, smooth_evts + (iter * 2));
		}
	}
	else if (coalescence) {
		for (uint iter = 0; iter<iterations; iter++) {
			smooth_evts[iter * 2] = smooth_coalescence(OCLenv->queue, smooth_k, cl_vertex4_array, cl_adjs_array, cl_adjsCounter, cl_result_vertex4_array, nels, lambda, !!iter, smooth_evts + (iter * 2) - 1);
			smooth_evts[iter * 2 + 1] = smooth_coalescence(OCLenv->queue, smooth_k, cl_result_vertex4_array, cl_adjs_array, cl_adjsCounter, cl_vertex4_array, nels, mi, 1, smooth_evts + (iter * 2));
		}
	}
	else if (localMemory) {
		for (uint iter = 0; iter<iterations; iter++) {
			smooth_evts[iter * 2] = smooth_lmem(OCLenv->queue, smooth_k, cl_vertex4_array, cl_adjs_array, cl_result_vertex4_array, nels, lambda, !!iter, smooth_evts + (iter * 2) - 1);
			smooth_evts[iter * 2 + 1] = smooth_lmem(OCLenv->queue, smooth_k, cl_result_vertex4_array, cl_adjs_array, cl_vertex4_array, nels, mi, 1, smooth_evts + (iter * 2));
		}
	}
	else {
		for (uint iter = 0; iter<iterations; iter++) {
			smooth_evts[iter * 2] = smooth(OCLenv->queue, smooth_k, cl_vertex4_array, cl_adjs_array, cl_result_vertex4_array, nels, lambda, !!iter, smooth_evts + (iter * 2) - 1);
			smooth_evts[iter * 2 + 1] = smooth(OCLenv->queue, smooth_k, cl_result_vertex4_array, cl_adjs_array, cl_vertex4_array, nels, mi, 1, smooth_evts + (iter * 2));
		}
	}

	// Copy result
	cl_event copy_evt;
	float *result_vertex4_array = (float *)clEnqueueMapBuffer(
		OCLenv->queue, cl_vertex4_array,
		CL_TRUE, CL_MAP_READ,
		0, memsize,
		1, &smooth_evts[iterations * 2 - 1],
		&copy_evt, &err);
	ocl_check(err, "map buffer vertex4_array");

	for (uint i = 0; i<nels; i++) {
		obj->vertex_vector[i].x = result_vertex4_array[i * 4];
		obj->vertex_vector[i].y = result_vertex4_array[i * 4 + 1];
		obj->vertex_vector[i].z = result_vertex4_array[i * 4 + 2];
	}

	cl_event unmap_evt;
	clEnqueueUnmapMemObject(OCLenv->queue, cl_vertex4_array, result_vertex4_array,
		0, NULL,
		&unmap_evt);

	double totalRuntime_ms = 0.0;
	double meanRuntime_ms = 0.0;
	double meanRuntime_ns = 0.0;

	for (uint i = 0; i<iterations * 2; i++) totalRuntime_ms += runtime_ms(smooth_evts[i]);

	meanRuntime_ms = totalRuntime_ms / (iterations*2.0);
	meanRuntime_ns = meanRuntime_ms * 1.0e6;

	if (coalescence && localMemory)
		printf(" coal lmem smooth time:\t%gms\n", meanRuntime_ms);
	else if (coalescence) {
		printf(" coal smooth time:\t%gms\t%gGB/s\n", meanRuntime_ms,
			(2.0*memsize + meanAdjsCount * memsize + meanAdjsCount * sizeof(int)*nels + meanAdjsCount * sizeof(int)*nels) / meanRuntime_ns);
	}
	else if (localMemory)
		printf(" lmem smooth time:\t%gms\n", meanRuntime_ms);
	else {
		printf(" smooth time:\t%gms\t%gGB/s\n", meanRuntime_ms,
			(2.0*memsize + meanAdjsCount * 4 * sizeof(float)*nels + meanAdjsCount * sizeof(int)*nels) / meanRuntime_ns);
	}

	printf(" copy time:\t%gms\t%gGB/s\n", runtime_ms(copy_evt), (2.0*memsize) / runtime_ns(copy_evt));

	printf(" kernels total time \t%gms\n", totalRuntime_ms);
	printf(" ~ %g smooth pass(es)/sec\n", (iterations * 2) / (totalRuntime_ms / 1.0e3));

	if (writeOBJ) obj->write(OUT_MESH);

	err = clReleaseMemObject(cl_vertex4_array);
	ocl_check(err, "clReleaseMemObject cl_vertex4_array");
	err = clReleaseMemObject(cl_adjs_array);
	ocl_check(err, "clReleaseMemObject cl_adjs_array");
	err = clReleaseMemObject(cl_result_vertex4_array);
	ocl_check(err, "clReleaseMemObject cl_result_vertex4_array");
	if (coalescence)
	{
		err = clReleaseMemObject(cl_adjsCounter);
		ocl_check(err, "clReleaseMemObject cl_adjsCounter");
	}

}

cl_event Smoothing::smooth(cl_command_queue queue, cl_kernel smooth_k, cl_mem cl_vertex4_array, cl_mem cl_adjs_array, cl_mem cl_result_vertex4_array, cl_uint nels, cl_float factor, cl_int waintingSize, cl_event* waitingList) {
	size_t lws[] = { localWorkSize };
	size_t gws[] = { round_mul_up(nels, localWorkSize) };
	cl_event smooth_evt;
	cl_int err;

	//printf("smooth gws: %d | %zu => %zu\n", nels, preferred_wg_smooth, gws[0]);

	// Setting arguments
	err = clSetKernelArg(smooth_k, 0, sizeof(cl_vertex4_array), &cl_vertex4_array);
	ocl_check(err, "set smooth arg 0");
	err = clSetKernelArg(smooth_k, 1, sizeof(cl_adjs_array), &cl_adjs_array);
	ocl_check(err, "set smooth arg 1");
	err = clSetKernelArg(smooth_k, 2, sizeof(cl_result_vertex4_array), &cl_result_vertex4_array);
	ocl_check(err, "set smooth arg 2");
	err = clSetKernelArg(smooth_k, 3, sizeof(nels), &nels);
	ocl_check(err, "set smooth arg 3");
	err = clSetKernelArg(smooth_k, 4, sizeof(factor), &factor);
	ocl_check(err, "set smooth arg 4");

	err = clEnqueueNDRangeKernel(queue, smooth_k,
		1, NULL, gws, lws, /* griglia di lancio */
		waintingSize, waintingSize == 0 ? NULL : waitingList, /* waiting list */
		&smooth_evt);
	ocl_check(err, "enqueue kernel smooth");
	return smooth_evt;
}

cl_event Smoothing::smooth_lmem(cl_command_queue queue, cl_kernel smooth_k, cl_mem cl_vertex4_array, cl_mem cl_adjs_array, cl_mem cl_result_vertex4_array, cl_uint nels, cl_float factor, cl_int waintingSize, cl_event* waitingList) {
	size_t lws[] = { localWorkSize };
	size_t gws[] = { round_mul_up(nels, localWorkSize) };
	cl_event smooth_evt;
	cl_int err;

	//printf("smooth gws: %d | %zu => %zu\n", nels, preferred_wg_smooth, gws[0]);

	// Setting arguments
	err = clSetKernelArg(smooth_k, 0, sizeof(cl_vertex4_array), &cl_vertex4_array);
	ocl_check(err, "set smooth arg 0");
	err = clSetKernelArg(smooth_k, 1, lws[0] * sizeof(cl_float4), NULL);
	ocl_check(err, "set smooth arg 1");
	err = clSetKernelArg(smooth_k, 2, sizeof(cl_adjs_array), &cl_adjs_array);
	ocl_check(err, "set smooth arg 2");
	err = clSetKernelArg(smooth_k, 3, sizeof(cl_result_vertex4_array), &cl_result_vertex4_array);
	ocl_check(err, "set smooth arg 3");
	err = clSetKernelArg(smooth_k, 4, sizeof(nels), &nels);
	ocl_check(err, "set smooth arg 4");
	err = clSetKernelArg(smooth_k, 5, sizeof(factor), &factor);
	ocl_check(err, "set smooth arg 5");

	err = clEnqueueNDRangeKernel(queue, smooth_k,
		1, NULL, gws, lws, /* griglia di lancio */
		waintingSize, waintingSize == 0 ? NULL : waitingList, /* waiting list */
		&smooth_evt);
	ocl_check(err, "enqueue kernel smooth");
	return smooth_evt;
}

cl_event Smoothing::smooth_coalescence(cl_command_queue queue, cl_kernel smooth_k, cl_mem cl_vertex4_array, cl_mem cl_adjs_array, cl_mem cl_adjsCounter, cl_mem cl_result_vertex4_array, cl_uint nels, cl_float factor, cl_int waintingSize, cl_event* waitingList) {
	size_t lws[] = { localWorkSize };
	size_t gws[] = { round_mul_up(nels, localWorkSize) };
	cl_event smooth_evt;
	cl_int err;

	//printf("smooth gws: %d | %zu => %zu\n", nels, preferred_wg_smooth, gws[0]);

	// Setting arguments
	err = clSetKernelArg(smooth_k, 0, sizeof(cl_vertex4_array), &cl_vertex4_array);
	ocl_check(err, "coal set smooth arg 0");
	err = clSetKernelArg(smooth_k, 1, sizeof(cl_adjs_array), &cl_adjs_array);
	ocl_check(err, "coal set smooth arg 1");
	err = clSetKernelArg(smooth_k, 2, sizeof(cl_adjsCounter), &cl_adjsCounter);
	ocl_check(err, "coal set smooth arg 2");
	err = clSetKernelArg(smooth_k, 3, sizeof(cl_result_vertex4_array), &cl_result_vertex4_array);
	ocl_check(err, "coal set smooth arg 3");
	err = clSetKernelArg(smooth_k, 4, sizeof(nels), &nels);
	ocl_check(err, "coal set smooth arg 4");
	err = clSetKernelArg(smooth_k, 5, sizeof(factor), &factor);
	ocl_check(err, "coal set smooth arg 5");

	err = clEnqueueNDRangeKernel(queue, smooth_k,
		1, NULL, gws, lws, /* griglia di lancio */
		waintingSize, waintingSize == 0 ? NULL : waitingList, /* waiting list */
		&smooth_evt);
	ocl_check(err, "enqueue kernel smooth");
	return smooth_evt;
}

cl_event Smoothing::smooth_coalescence_lmem(cl_command_queue queue, cl_kernel smooth_k, cl_mem cl_vertex4_array, cl_mem cl_adjs_array, cl_mem cl_adjsCounter, cl_mem cl_result_vertex4_array, cl_uint nels, cl_float factor, cl_int waintingSize, cl_event* waitingList) {
	size_t lws[] = { localWorkSize };
	size_t gws[] = { round_mul_up(nels, localWorkSize) };
	cl_event smooth_evt;
	cl_int err;

	//printf("smooth gws: %d | %zu => %zu\n", nels, preferred_wg_smooth, gws[0]);

	// Setting arguments
	err = clSetKernelArg(smooth_k, 0, sizeof(cl_vertex4_array), &cl_vertex4_array);
	ocl_check(err, "coal set smooth arg 0");
	err = clSetKernelArg(smooth_k, 1, sizeof(cl_adjs_array), &cl_adjs_array);
	ocl_check(err, "coal set smooth arg 1");
	err = clSetKernelArg(smooth_k, 2, sizeof(cl_adjsCounter), &cl_adjsCounter);
	ocl_check(err, "coal set smooth arg 2");
	err = clSetKernelArg(smooth_k, 3, maxAdjsCount * sizeof(cl_uint), NULL);
	ocl_check(err, "coal set smooth arg 3");
	err = clSetKernelArg(smooth_k, 4, sizeof(cl_uint), &maxAdjsCount);
	ocl_check(err, "coal set smooth arg 4");
	err = clSetKernelArg(smooth_k, 5, sizeof(cl_result_vertex4_array), &cl_result_vertex4_array);
	ocl_check(err, "coal set smooth arg 5");
	err = clSetKernelArg(smooth_k, 6, sizeof(nels), &nels);
	ocl_check(err, "coal set smooth arg 6");
	err = clSetKernelArg(smooth_k, 7, sizeof(factor), &factor);
	ocl_check(err, "coal set smooth arg 7");

	err = clEnqueueNDRangeKernel(queue, smooth_k,
		1, NULL, gws, lws, /* griglia di lancio */
		waintingSize, waintingSize == 0 ? NULL : waitingList, /* waiting list */
		&smooth_evt);
	ocl_check(err, "enqueue kernel smooth");
	return smooth_evt;
}

void CommandOptions::cmdOptionsHelp()
{
	printf(" \n Command parameters:\n");
	printf(" -p / -platf / -platform       < Es: -p 0 >\n");
	printf(" -d / -dev   / -device         < Es: -d 0 >\n");
	printf(" -m / -mesh  / -input          < Es: -m cube_example >\n");
	printf(" -i / -iter  / -iterations     < Es: -i 1 >\n");
	printf(" -f / -facts / -factors        < Es: -f 0.22 0.21 >\n");
	printf(" -w / -write / -b_write        < Es: -w 1 >\n");
	printf(" -o / -opt   / -options        < Es: -o sortVertex sortAdjs coalescence localMemory>\n");
	printf(" -g / -l     / -lws            < Es: -g 512 >\n");
	exit(0);
}

CommandOptions::CommandOptions(const int argc, const std::vector<std::string> argv)
{
	initCmdOptions();
	cmdOptionsParser(argc, argv);
}

void CommandOptions::initCmdOptions()
{
	platformID = OCL_PLATFORM;
	deviceID = OCL_DEVICE;
	input_mesh = IN_MESH;
	iterations = 1;
	lambda = 0.5f;
	mi = -0.5f;
	writeObj = 0;
	kernelOptions = OptNoOption;
	lws = 32;
}

void CommandOptions::cmdOptionsParser(const int argc, const std::vector<std::string> argv)
{
	if (argc == 1) {
		std::string str = argv[0];
		if (str == "-h" || str == "-help") cmdOptionsHelp();
		else { printf(" Missing -parameter specification. (use -help)\n"); exit(-1); }
	}

	for (int i = 0; i<argc; i += 2) {
		if (i == argc - 1) { printf(" Wrong -parameters. (use -help)\n"); exit(-1); }
		std::string param = argv[i];
		std::string value = argv[i + 1];

		if (param[0] != '-') { printf(" Missing -parameter specification. (use -help)\n"); exit(-1); }
		if (value[0] == '-') { printf(" Missing value after -parameter. (use -help)\n"); exit(-1); }

		if (param == "-m" || param == "-mesh") input_mesh = "res/" + value + ".obj";
		else if (param == "-d" || param == "-dev" || param == "-device") deviceID = stoi(value);
		else if (param == "-p" || param == "-plat" || param == "-platform") platformID = stoi(value);
		else if (param == "-i" || param == "-iter" || param == "-iterations") iterations = stoi(value);
		else if (param == "-f" || param == "-facts" || param == "-factors") {
			std::string factor1 = argv[i + 1];
			std::string factor2;
			lambda = stof(factor1);
			if (i + 2 < argc && (argv[i + 2][0] >= '0' && argv[i + 2][0] <= '9')) {
				factor2 = argv[i + 2];
				mi = -stof(factor2);
				i++;
			}
			else mi = -lambda;
		}
		else if (param == "-w" || param == "-write" || param == "-b_write") writeObj = !!stoi(value);
		else if (param == "-o" || param == "-opt" || param == "-options") {
			std::string currentOpt;
			while (i + 1 < argc && argv[i + 1][0] != '-') {
				currentOpt = argv[i + 1];

				if (currentOpt == "sortVertex" || currentOpt == "SortVertex")
					kernelOptions |= OptSortVertex;
				else if (currentOpt == "sortAdjs" || currentOpt == "SortAdjs")
					kernelOptions |= OptSortAdjs;
				else if (currentOpt == "coalescence" || currentOpt == "Coalescence" || currentOpt == "coal")
					kernelOptions |= OptCoalescence;
				else if (currentOpt == "localMemory" || currentOpt == "LocalMemory" || currentOpt == "lmem")
					kernelOptions |= OptLocalMemory;
				else { printf(" Wrong value after -options. (use -help)\n"); exit(-1); }
				i++;
			}
			i--;
		}
		else if (param == "-g" || param == "-l" || param == "-lws") lws = stoi(value);
		else { printf(" Wrong -parameter. (use -help)\n"); exit(-1); }
	}
}
