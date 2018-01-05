#define OCL_PLATFORM 0
#define OCL_DEVICE 0

#define WRESTLERS "res/wrestlers.obj"
#define SUZANNE "res/suzanne.obj"
#define EGYPT "res/egypt.obj"
#define DRAGON "res/dragon.obj"
#define HUMAN "res/human.obj"
#define TEST "res/test.obj"
#define CUBE "res/cube_example.obj"
#define NOISECUBE "res/cube_noise.obj"

#define IN_MESH CUBE
#define OUT_MESH "res/out.obj"

#define OCL_FILENAME "src/meshsmooth.ocl"

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

typedef unsigned int uint;
size_t preferred_wg_smooth;

class OBJ {
	private:
	
	bool validData;
	uint verticesCount;
	uint facesCount;
	
	init() {
		validData = false;
		verticesCount = facesCount = 0;
	}
	bool OBJException(std::string strerror) {
		printf("Error: %s!\n", strerror);
		validData = false;
		return false;
	}
	
	
	public:
	
	std::vector< glm::vec3 > vertex_vector;
	std::vector< uint > facesVertexIndex_vector;
	
	OBJ(std::string path){
		init();
		load(path);
	}
	void clear(){
		init();
		vertex_vector.clear();
		facesVertexIndex_vector.clear();
	}
	bool load(std::string path){
		
		clear();

		printf(" > Loading %s...\n", path.c_str());
		
		FILE * file = fopen(path.c_str(), "r");
		if( file == NULL ) OBJException("fopen() -> Impossible to open the file");
		while( 1 ){ // parsing
			char lineHeader[128];
			// read the first word of the line
			int res = fscanf(file, "%s", lineHeader);
			if (res == EOF) break; // EOF = End Of File. Quit the loop.
			// else : parse lineHeader
			if ( strcmp( lineHeader, "v" ) == 0 ){
				glm::vec3 vertex;
				fscanf(file, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z );
				vertex_vector.push_back(vertex);
				verticesCount++;
			}
			else if ( strcmp( lineHeader, "f" ) == 0 ){
				uint faceVertexIndex[3], faceUvIndex[3], faceNormalIndex[3];
				
				char line[512];
				fgets( line, 512, file);
				
				int	match = sscanf(line, "%d/%d/%d %d/%d/%d %d/%d/%d\n", &faceVertexIndex[0], &faceUvIndex[0], &faceNormalIndex[0], &faceVertexIndex[1], &faceUvIndex[1], &faceNormalIndex[1], &faceVertexIndex[2], &faceUvIndex[2], &faceNormalIndex[2]);
				if(match < 9) match = sscanf(line,"%d//%d %d//%d %d//%d\n", &faceVertexIndex[0], &faceNormalIndex[0], &faceVertexIndex[1], &faceNormalIndex[1], &faceVertexIndex[2], &faceNormalIndex[2]);
				if(match < 6) match = sscanf(line,"%d %d %d\n", &faceVertexIndex[0], &faceVertexIndex[1], &faceVertexIndex[2]);
				if(match < 3) OBJException("parser -> Try exporting with other options");
				facesVertexIndex_vector.push_back(faceVertexIndex[0]);
				facesVertexIndex_vector.push_back(faceVertexIndex[1]);
				facesVertexIndex_vector.push_back(faceVertexIndex[2]);
				facesCount++;
			}
		}
		printf(" > %s loaded!\n", path.c_str());
		validData = true;
		return true;
	}
	bool hasValidData() const { return validData; }
	uint getVerticesCount() const { return verticesCount; }
	uint getFacesCount() const { return facesCount; }
};

/*class Smoothing {
	private:
		uint nels, nadjs, minAdjsCount, maxAdjsCount;
		float meanAdjsCount;
		
	public:
	Smoothing(const OBJ &obj){
		if(!obj.hasValidData()) {
			printf("Smoothing(OBJ) -> obj has invalid data");
			return;
		}
	}
};*/

void orderedUniqueInsert(std::vector< uint >*vertexAdjacents, uint vertexID) {
	std::vector< uint >::iterator it;
	for(it = vertexAdjacents->begin(); it != vertexAdjacents->end() && *it < vertexID; it++) {}
	if(it == vertexAdjacents->end() || *it != vertexID){
		vertexAdjacents->insert(it, vertexID);
	}
}

void readOBJFile(std::string path, uint &nels, float* &vertex4_array, uint &nadjs, uint* &adjs_array, uint &minAdjsCount, uint &maxAdjsCount) {
		
	printf("===== LOAD & INIT DATA =====\n");

	OBJ obj(path);
	if(!obj.hasValidData()) exit(1);

	printf(" > Initializing data...\n");
	
	// Init number of vertex
	nels = obj.getVerticesCount();
	
	// Discover adjacents vertex for each vertex
	std::vector< uint >* obj_adjacents_arrayVector = new std::vector< uint >[nels];

	for(int i=0; i<obj.facesVertexIndex_vector.size(); i+=3){
		uint vertexID1 = obj.facesVertexIndex_vector[i] - 1;
		uint vertexID2 = obj.facesVertexIndex_vector[i+1] - 1;
		uint vertexID3 = obj.facesVertexIndex_vector[i+2] - 1;

		std::vector< uint >* adjacent1 = &obj_adjacents_arrayVector[vertexID1];
		std::vector< uint >* adjacent2 = &obj_adjacents_arrayVector[vertexID2];
		std::vector< uint >* adjacent3 = &obj_adjacents_arrayVector[vertexID3];
		
		#define INDEX_ORDERED 1
		#if INDEX_ORDERED
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
	
	minAdjsCount = obj_adjacents_arrayVector[0].size();
	maxAdjsCount = minAdjsCount;
	
	struct vertex_struct{
		uint currentIndex;
		uint obj_vertex_vector_Index;
		uint adjsCount;
		std::vector<vertex_struct*> adjs;
	};
	
	vertex_struct** vertex_arrayStruct = new vertex_struct*[nels];
	
	for(int i=0; i<nels; i++){
		vertex_struct* v = new vertex_struct();
		v->currentIndex = i;
		v->obj_vertex_vector_Index = i;
		
		//number of vertices adjacents to vertex i
		uint currentAdjsCount = obj_adjacents_arrayVector[i].size();
			if(currentAdjsCount > maxAdjsCount) maxAdjsCount = currentAdjsCount;
			else if(currentAdjsCount < minAdjsCount) minAdjsCount = currentAdjsCount;
		v->adjsCount = currentAdjsCount;
		
		vertex_arrayStruct[i] = v;
	}
	
	uint countingSize = maxAdjsCount-minAdjsCount+1;
	uint counting_array[countingSize] = {0};
	
	for(int i=0; i<nels; i++){
		//counting elements
		++counting_array[obj_adjacents_arrayVector[i].size() - minAdjsCount];
		
		for(uint index : obj_adjacents_arrayVector[i])
			vertex_arrayStruct[i]->adjs.push_back(vertex_arrayStruct[index]);
	}
	
	//counting sort sums
	for(int i=1; i<countingSize; i++) counting_array[i] += counting_array[i-1];
	
	vertex4_array = new float[4*nels];
	uint currentAdjStartIndex = 0;
	
	
	vertex_struct** orderedVertex_arrayStruct = new vertex_struct*[nels];
	//insert ordered vertex
	for(int i=nels-1; i>=0; i--){ //(int i=0; i<nels; i++) not stable algorithm
		vertex_struct * currVertex = vertex_arrayStruct[i];
		uint currentAdjsCount = currVertex->adjsCount;
		
		//calc new orderedIndex from counting_array
		uint orderedIndex = --counting_array[ currentAdjsCount - minAdjsCount ];
		//set to vertex in struct the new orderedIndex
		currVertex->currentIndex = orderedIndex;

		/*
		//get original vertex position from obj_vertex_vector
		glm::vec3 *vertexPos = &obj_vertex_vector[currVertex->obj_vertex_vector_Index];

		//insert vertex in new ordered position
		vertex4_array[4*orderedIndex] = vertexPos->x;
		vertex4_array[4*orderedIndex+1] = vertexPos->y;
		vertex4_array[4*orderedIndex+2] = vertexPos->z;
		
		uint* adjIndexPtr = (uint*)(&vertex4_array[4*orderedIndex+3]);
		*adjIndexPtr = ((uint)currentAdjStartIndex)<<6;
		*adjIndexPtr += (currentAdjsCount<<26)>>26;
		currentAdjStartIndex += currentAdjsCount;*/

		orderedVertex_arrayStruct[orderedIndex] = currVertex;
	}
	
	#define ORDERED 1
	
	for(int i=0; i<nels; i++) {
		vertex_struct * currVertex = orderedVertex_arrayStruct[i];
		#if ORDERED
		glm::vec3 vertex = obj.vertex_vector[currVertex->obj_vertex_vector_Index];
		#else
		glm::vec3 vertex = obj.vertex_vector[i];
		#endif
		vertex4_array[4*i] = vertex.x;
		vertex4_array[4*i+1] = vertex.y;
		vertex4_array[4*i+2] = vertex.z;

		#if ORDERED
		uint currentAdjsCount = currVertex->adjsCount;
		#else
		uint currentAdjsCount = obj_adjacents_arrayVector[i].size();
		#endif
		uint* adjIndexPtr = (uint*)(&vertex4_array[4*i+3]);
		*adjIndexPtr = ((uint)currentAdjStartIndex)<<6;
		*adjIndexPtr += (currentAdjsCount<<26)>>26;
		
		currentAdjStartIndex += currentAdjsCount;
	}

	// Now, currentAdjStartIndex is the total adjacents number.
	nadjs = currentAdjStartIndex;
	
	adjs_array = new uint[nadjs];
	uint adjIndex = 0;
	#if ORDERED
	for(int i=0; i<nels; i++) {
		vertex_struct * currVertex = orderedVertex_arrayStruct[i];
		for( vertex_struct* adj : currVertex->adjs)
			adjs_array[adjIndex++] = adj->currentIndex;
	}
	#else
	for(int i=0; i<nels; i++) {
		for( uint adjIndex : obj_adjacents_arrayVector[i])
			adjs_array[adjIndex++] = adjIndex;
	}
	#endif
	
	printf(" > Data initialized!\n");
	printf("============================\n");
}

void writeOBJFile(std::string path, std::string out_path, float *result_vertex4_array) {
	printf("========= SAVE OBJ =========\n");
	printf(" > Saving result to %s...\n", OUT_MESH);
	char line[512];

	FILE * in_file = fopen(path.c_str(), "r");
	FILE * out_file = fopen(out_path.c_str(), "w");
	
	if(in_file == NULL){
		printf("Impossible to open the file !\n");
		return;
	}
	if(result_vertex4_array == NULL) {
		printf("Result input error !\n");
		return;
	}
	
	int i=0;
	while (fgets(line, sizeof line, in_file) != NULL) {
		if ( line[0] == 'v' && line[1] == ' ') {
			fprintf(out_file, "v %f %f %f\n", result_vertex4_array[4*i], result_vertex4_array[4*i+1], result_vertex4_array[4*i+2]);
			i++;
		}
		else fprintf(out_file, "%s", line);
	}
	printf(" > Result saved to %s!\n", OUT_MESH);
	printf("============================\n");
}

cl_event smooth(cl_command_queue queue, cl_kernel smooth_k, cl_mem cl_vertex4_array, cl_mem cl_adjs_array, cl_mem cl_result_vertex4_array, cl_uint nels, cl_float factor, cl_int waintingSize, cl_event* waitingList) {
	size_t gws[] = { round_mul_up(nels, preferred_wg_smooth) };
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
		1, NULL, gws, NULL, /* griglia di lancio */
		waintingSize, waintingSize==0?NULL:waitingList, /* waiting list */
		&smooth_evt);
	ocl_check(err, "enqueue kernel smooth");
	return smooth_evt;
}

int main(int argc, char *argv[]) {
	
	uint iterations = (argc>=2) ? atoi(argv[1]) : 1 ;
	float lambda = (argc>=4) ? atoi(argv[2]) : 0.5f;
	float mi = (argc>=4) ? atoi(argv[3]) : -0.5f;

	uint nels, nadjs, minAdjsCount, maxAdjsCount;
	float meanAdjsCount;
	float* vertex4_array, *result_vertex4_array;
	uint* adjs_array;
	size_t memsize, ajdsmemsize;
	
	cl_int err;
	printf("\n======= OPENCL INFO ========\n");
	cl_platform_id platformID = select_platform();
	cl_device_id deviceID = select_device(platformID);
	cl_context context = create_context(platformID, deviceID);
	cl_command_queue queue = create_queue(context, deviceID);
	cl_program program = create_program(OCL_FILENAME, context, deviceID);
	printf("============================\n");
		
	readOBJFile(IN_MESH, nels, vertex4_array, nadjs, adjs_array, minAdjsCount, maxAdjsCount);
	meanAdjsCount = nadjs/(float)nels;
	memsize = 4*nels*sizeof(float);
	ajdsmemsize = nadjs*sizeof(uint);
	
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
	cl_mem cl_vertex4_array = clCreateBuffer(context, CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR, memsize, vertex4_array, &err);
	cl_mem cl_adjs_array = clCreateBuffer(context, CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR, ajdsmemsize, adjs_array, &err);
	cl_mem cl_result_vertex4_array = clCreateBuffer(context, CL_MEM_READ_WRITE, memsize, NULL, &err); // ANGEL

	// Extract kernels
	cl_kernel smooth_k = clCreateKernel(program, "smooth", &err);
	ocl_check(err, "create kernel smooth");

	// Set preferred_wg size from device info
	err = clGetKernelWorkGroupInfo(smooth_k, deviceID, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(preferred_wg_smooth), &preferred_wg_smooth, NULL);
	
	
	printf("====== KERNEL LAUNCH =======\n");
	cl_event smooth_evt, smooth_evt2;
	printf("start\n");
	
	for(int iter=0; iter<iterations; iter++) {
		smooth_evt = smooth(queue, smooth_k, cl_vertex4_array, cl_adjs_array, cl_result_vertex4_array, nels, lambda, !!iter, &smooth_evt2);
		smooth_evt2 = smooth(queue, smooth_k, cl_result_vertex4_array, cl_adjs_array, cl_vertex4_array, nels, mi, 1, &smooth_evt);
	}
	
	// Copy result
	result_vertex4_array = new float[4*nels];
	if (!result_vertex4_array) printf("error res\n");

	cl_event copy_evt;
	err = clEnqueueReadBuffer(queue, cl_vertex4_array, CL_TRUE,
		0, memsize, result_vertex4_array,
		1, &smooth_evt2, &copy_evt);
	ocl_check(err, "read buffer vertex4_array");
	
	printf("stop\n");
	
	err = clWaitForEvents(1, &copy_evt);
	ocl_check(err, "clWaitForEvents");
	
	printf("smooth time:\t%gms\t%gGB/s\n", runtime_ms(smooth_evt),
		(2.0*memsize + meanAdjsCount*memsize + meanAdjsCount*nels*sizeof(int))/runtime_ns(smooth_evt));
	printf("copy time:\t%gms\t%gGB/s\n", runtime_ms(copy_evt),
		(2.0*memsize)/runtime_ns(copy_evt));
		
	printf("============================\n");
		
	//writeOBJFile(IN_MESH, OUT_MESH, result_vertex4_array);
	return 0;
}
