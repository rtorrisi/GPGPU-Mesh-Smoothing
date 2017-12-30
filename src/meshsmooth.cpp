#define OCL_PLATFORM 0
#define OCL_DEVICE 0

#define TEST "res/test.obj"
#define ANGEL "res/angel.obj"
#define CUBE "res/cube_example.obj"
#define NOISECUBE "res/cube_noise.obj"

#define IN_MESH CUBE
#define OUT_MESH "res/out.obj"

#include "ocl_boiler.h"
#include <iostream>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/gtx/string_cast.hpp>
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>

#define OCL_FILENAME "src/meshsmooth.ocl"

size_t preferred_wg_smooth;

void orderedUniqueInsert(std::vector< unsigned int >*vertexAdjacents, unsigned int vertexID) {
	std::vector< unsigned int >::iterator it;
	for(it = vertexAdjacents->begin(); it != vertexAdjacents->end() && *it < vertexID; it++) {}
	if(it == vertexAdjacents->end() || *it != vertexID){
		vertexAdjacents->insert(it, vertexID);
	}
}

void readOBJFile(std::string path,
	unsigned int &nels, float* &vertex4_array, unsigned int &nadjs, unsigned int* &adjs_array,
	unsigned int &minAdjsCount, unsigned int &maxAdjsCount
	){
		
	printf("===== LOAD & INIT DATA =====\n");
	printf(" > Loading %s...\n", IN_MESH);
	
	std::vector< glm::vec3 > obj_vertex_vector;
	std::vector< unsigned int > obj_facesVertexIndex_vector;
	
	FILE * file = fopen(path.c_str(), "r");
	if( file == NULL ){
		printf("Impossible to open the file !\n");
		return;
	}
	while( 1 ){ // parsing
		char lineHeader[128];
		// read the first word of the line
		int res = fscanf(file, "%s", lineHeader);
		if (res == EOF) break; // EOF = End Of File. Quit the loop.
		// else : parse lineHeader
		if ( strcmp( lineHeader, "v" ) == 0 ){
			glm::vec3 vertex;
			fscanf(file, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z );
			obj_vertex_vector.push_back(vertex);
		}
		else if ( strcmp( lineHeader, "f" ) == 0 ){
			unsigned int faceVertexIndex[3], faceUvIndex[3], faceNormalIndex[3];
			
			char line[512];
			fgets( line, 512, file);
			
			int	match = sscanf(line, "%d/%d/%d %d/%d/%d %d/%d/%d\n", &faceVertexIndex[0], &faceUvIndex[0], &faceNormalIndex[0], &faceVertexIndex[1], &faceUvIndex[1], &faceNormalIndex[1], &faceVertexIndex[2], &faceUvIndex[2], &faceNormalIndex[2]);
			if(match < 9)
				match = sscanf(line,"%d//%d %d//%d %d//%d\n", &faceVertexIndex[0], &faceNormalIndex[0], &faceVertexIndex[1], &faceNormalIndex[1], &faceVertexIndex[2], &faceNormalIndex[2]);
			if(match < 6)
				match = sscanf(line,"%d %d %d\n", &faceVertexIndex[0], &faceVertexIndex[1], &faceVertexIndex[2]);
			if(match < 3){
				printf("File can't be read by parser. Try exporting with other options\n");
				return;
			}
			obj_facesVertexIndex_vector.push_back(faceVertexIndex[0]);
			obj_facesVertexIndex_vector.push_back(faceVertexIndex[1]);
			obj_facesVertexIndex_vector.push_back(faceVertexIndex[2]);
		}
	}
	
	printf(" > %s loaded!\n", IN_MESH);
	printf(" > Initializing data...\n");
	
	// Init number of vertex
	nels = obj_vertex_vector.size();
	
	// Discover adjacents vertex for each vertex
	std::vector< unsigned int >* obj_adjacents_arrayVector = new std::vector< unsigned int >[nels];

	for(int i=0; i<obj_facesVertexIndex_vector.size(); i+=3){
		unsigned int vertexID1 = obj_facesVertexIndex_vector[i] - 1;
		unsigned int vertexID2 = obj_facesVertexIndex_vector[i+1] - 1;
		unsigned int vertexID3 = obj_facesVertexIndex_vector[i+2] - 1;

		std::vector< unsigned int >* adjacent1 = &obj_adjacents_arrayVector[vertexID1];
		std::vector< unsigned int >* adjacent2 = &obj_adjacents_arrayVector[vertexID2];
		std::vector< unsigned int >* adjacent3 = &obj_adjacents_arrayVector[vertexID3];
		
		#if 1
		orderedUniqueInsert(adjacent1, vertexID2);
		orderedUniqueInsert(adjacent1, vertexID3);
		orderedUniqueInsert(adjacent2, vertexID1);
		orderedUniqueInsert(adjacent2, vertexID3);
		orderedUniqueInsert(adjacent3, vertexID1);
		orderedUniqueInsert(adjacent3, vertexID2);
		#else
		if (std::find(adjacent1->begin(), adjacent1->end(), vertexID2) == adjacent1->end()) adjacent1->push_back(vertexID2);
		if (std::find(adjacent1->begin(), adjacent1->end(), vertexID3) == adjacent1->end()) adjacent1->push_back(vertexID3);
		if (std::find(adjacent2->begin(), adjacent2->end(), vertexID1) == adjacent2->end()) adjacent2->push_back(vertexID1);
		if (std::find(adjacent2->begin(), adjacent2->end(), vertexID3) == adjacent2->end()) adjacent2->push_back(vertexID3);
		if (std::find(adjacent3->begin(), adjacent3->end(), vertexID1) == adjacent3->end()) adjacent3->push_back(vertexID1);
		if (std::find(adjacent3->begin(), adjacent3->end(), vertexID2) == adjacent3->end()) adjacent3->push_back(vertexID2);
		#endif
	}
	
	/* print adjacents per vertex
	for( int i=0; i<nels; i++ ) for(unsigned int index : obj_adjacents_arrayVector[i]) printf("vertex %d => adj %d\n", i, index);
	*/
	
	// Init vertex4_array
	vertex4_array = new float[4*nels];
	
	unsigned int currentAdjStartIndex = 0;
	minAdjsCount = obj_adjacents_arrayVector[0].size();
	maxAdjsCount = 0;
	
	for(int i=0; i<nels; i++) {
		glm::vec3 *vertex = &obj_vertex_vector[i];
		vertex4_array[4*i] = vertex->x;
		vertex4_array[4*i+1] = vertex->y;
		vertex4_array[4*i+2] = vertex->z;

		unsigned int currentAdjsCount = obj_adjacents_arrayVector[i].size();
		unsigned int* adjIndexPtr = (unsigned int*)(&vertex4_array[4*i+3]);
		*adjIndexPtr = ((unsigned int)currentAdjStartIndex)<<6;
		*adjIndexPtr += (currentAdjsCount<<26)>>26;

		//printf("indexOfAdjs -> %d\nnumOfAdjs -> %d\n", (*adjIndexPtr)>>8, ((*adjIndexPtr)<<24)>>24);

		// Min & max adjacent number 
		if(currentAdjsCount > maxAdjsCount) maxAdjsCount = currentAdjsCount;
		else if(currentAdjsCount < minAdjsCount) minAdjsCount = currentAdjsCount;
		
		currentAdjStartIndex += currentAdjsCount;
	}
	nadjs = currentAdjStartIndex;
	
	// Now, currentAdjStartIndex is the total adjacents number.
	adjs_array = new unsigned int[currentAdjStartIndex];
	unsigned int adjIndex = 0;
	for(int i=0; i<nels; i++) {
		for( unsigned int vertexIndex : obj_adjacents_arrayVector[i])
			adjs_array[adjIndex++] = vertexIndex;
	}
	
	printf(" > Data initialized!\n");
	printf("============================\n");
}

void writeOBJFile(std::string path, std::string out_path, float *res){
	printf("========= SAVE OBJ =========\n");
	printf(" > Saving result to %s...\n", OUT_MESH);
	char line[512];

	FILE * in_file = fopen(path.c_str(), "r");
	FILE * out_file = fopen(out_path.c_str(), "w");
	
	if(in_file == NULL){
		printf("Impossible to open the file !\n");
		return;
	}
	if(res == NULL) {
		printf("Result input error !\n");
		return;
	}
	
	int i=0;
	while (fgets(line, sizeof line, in_file) != NULL) {
		if ( line[0] == 'v' && line[1] == ' ') {
			fprintf(out_file, "v %f %f %f\n", res[4*i], res[4*i+1], res[4*i+2]);
			i++;
		}
		else fprintf(out_file, "%s", line);
	}
	printf(" > Result saved to %s!\n", OUT_MESH);
	printf("============================\n");
}

cl_event smooth(cl_command_queue queue, cl_kernel smooth_k, cl_mem cl_vertex4Array, cl_mem cl_adjsArray, cl_mem cl_result, cl_uint nels, cl_float factor){
	size_t gws[] = { round_mul_up(nels, preferred_wg_smooth) };
	cl_event smooth_evt;
	cl_int err;

	//printf("smooth gws: %d | %zu => %zu\n", nels, preferred_wg_smooth, gws[0]);

	// Setting arguments
	err = clSetKernelArg(smooth_k, 0, sizeof(cl_vertex4Array), &cl_vertex4Array);
	ocl_check(err, "set smooth arg 0");
	err = clSetKernelArg(smooth_k, 1, sizeof(cl_adjsArray), &cl_adjsArray);
	ocl_check(err, "set smooth arg 1");
	err = clSetKernelArg(smooth_k, 2, sizeof(cl_result), &cl_result);
	ocl_check(err, "set smooth arg 2");
	err = clSetKernelArg(smooth_k, 3, sizeof(nels), &nels);
	ocl_check(err, "set smooth arg 3");
	err = clSetKernelArg(smooth_k, 4, sizeof(factor), &factor);
	ocl_check(err, "set smooth arg 4");

	err = clEnqueueNDRangeKernel(queue, smooth_k,
		1, NULL, gws, NULL, /* griglia di lancio */
		0, NULL, /* waiting list */
		&smooth_evt);
	ocl_check(err, "enqueue kernel smooth");
	return smooth_evt;
}

int main(int argc, char *argv[]) {
	
	unsigned int iterations = 1;
	float lambda = 0.5f;
	float mi = 0.5f;
	
	if(argc >= 2) {
		iterations = atoi(argv[1]);
	} if(argc == 4) {
		lambda = atof(argv[2]);
		mi = atof(argv[3]);
	}
	
	unsigned int nels, nadjs, minAdjsCount, maxAdjsCount;
	float meanAdjsCount;
	float* vertex4_array, *result_vertex4Array;
	unsigned int* adjs_array;
	size_t memsize, ajdsmemsize;
	
	cl_int err;
	printf("\n======= BOILER INFO ========\n");
	cl_platform_id platformID = select_platform();
	cl_device_id deviceID = select_device(platformID);
	cl_context context = create_context(platformID, deviceID);
	cl_command_queue queue = create_queue(context, deviceID);
	cl_program program = create_program(OCL_FILENAME, context, deviceID);
	printf("============================\n");
		
	readOBJFile(IN_MESH, nels, vertex4_array, nadjs, adjs_array, minAdjsCount, maxAdjsCount);
	meanAdjsCount = nadjs/(float)nels;
	memsize = 4*nels*sizeof(float);
	ajdsmemsize = nadjs*sizeof(unsigned int);
	
	printf("====== SMOOTHING INFO ======\n");
	std::cout << " # Iterations: " << iterations << std::endl;
	std::cout << " Lambda factor: " << lambda << std::endl;
	std::cout << " Mi factor: " << mi << std::endl;
	printf("============================\n");
	
	printf("========= OBJ INFO =========\n");
	std::cout << " Input .obj path: " << IN_MESH << std::endl;
	std::cout << " Output .obj path: " << OUT_MESH << std::endl;
	std::cout << " # Vertex: " << nels << std::endl;
	std::cout << " # obj_adjacents_arrayVector: " << nadjs << std::endl;
	std::cout << " # Min vertex adjs: " << minAdjsCount << std::endl;
	std::cout << " # Max vertex adjs: " << maxAdjsCount << std::endl;
	std::cout << " # Mean vertex adjs: " << meanAdjsCount << std::endl;
	printf("============================\n");
		
	// Create Buffers
	cl_mem cl_vertex4Array = clCreateBuffer(context, CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR, memsize, vertex4_array, &err);
	cl_mem cl_adjsArray = clCreateBuffer(context, CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR, ajdsmemsize, adjs_array, &err);
	cl_mem cl_result = clCreateBuffer(context, CL_MEM_READ_WRITE, memsize, NULL, &err); // ANGEL

	// Extract kernels
	cl_kernel smooth_k = clCreateKernel(program, "smooth", &err);
	ocl_check(err, "create kernel smooth");

	// Set preferred_wg size from device info
	err = clGetKernelWorkGroupInfo(smooth_k, deviceID, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(preferred_wg_smooth), &preferred_wg_smooth, NULL);
	
	
	printf("====== KERNEL LAUNCH =======\n");
	cl_event smooth_evt, smooth_evt2;
	printf("start / ");
	for(int iter=0; iter<iterations; iter++) {
		smooth_evt = smooth(queue, smooth_k, cl_vertex4Array, cl_adjsArray, cl_result, nels, lambda);
		err = clWaitForEvents(1, &smooth_evt);
		ocl_check(err, "clWaitForEvents");
		
		smooth_evt2 = smooth(queue, smooth_k, cl_result, cl_adjsArray, cl_vertex4Array, nels, mi);
		err = clWaitForEvents(1, &smooth_evt2);
		ocl_check(err, "clWaitForEvents");
	}
	printf("stop\n");
	
	// Copy result
	result_vertex4Array = new float[4*nels];
	if (!result_vertex4Array) printf("error res\n");

	cl_event copy_evt;
	err = clEnqueueReadBuffer(queue, cl_vertex4Array, CL_TRUE,
		0, memsize, result_vertex4Array,
		1, &smooth_evt2, &copy_evt);
	ocl_check(err, "read buffer vertex4_array");
	
	err = clWaitForEvents(1, &copy_evt);
	ocl_check(err, "clWaitForEvents");
	
	printf("smooth time:\t%gms\t%gGB/s\n", runtime_ms(smooth_evt),
		(2.0*memsize + meanAdjsCount*memsize + meanAdjsCount*nels*sizeof(int))/runtime_ns(smooth_evt));
	printf("copy time:\t%gms\t%gGB/s\n", runtime_ms(copy_evt),
		(2.0*memsize)/runtime_ns(copy_evt));
		
	printf("============================\n");
		
	writeOBJFile(IN_MESH, OUT_MESH, result_vertex4Array);
	return 0;
}
