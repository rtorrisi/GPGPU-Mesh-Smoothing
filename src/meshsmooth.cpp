#include "ocl_boiler.h"
#include <cstdio>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/gtx/string_cast.hpp>
#include <chrono>

#define OCL_PLATFORM 1
#define OCL_DEVICE 0
#define OCL_FILENAME "src/meshsmooth.ocl"

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

#define INIT_TIMER auto start_time = std::chrono::high_resolution_clock::now()
#define START_TIMER start_time = std::chrono::high_resolution_clock::now()
#define ELAPSED_TIME std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()-start_time).count()

typedef unsigned int uint;
size_t preferred_wg_smooth;

enum Options {
	OptNoOption    = 0x00, // 0x01 ==   1 == "00000001"
	OptSortVertex  = 0x01, // 0x02 ==   2 == "00000010"
	OptSortAdjs    = 0x02, // 0x04 ==   4 == "00000100"
	OptCoalescence = 0x04, // 0x08 ==   8 == "00001000"
	OptLocalMemory = 0x08  // 0x10 ==  16 == "00010000"
}; //OptNoOption | OptSortVertex | OptSortAdjs | OptCoalescence | OptLocalMemory

void printElapsedTime_ms(const char * str, const unsigned long long int microseconds) {
	if(microseconds==0) printf(" %s : < 15ms\n", str);
	else printf(" %s : %gms\n", str, microseconds/(double)1000);
}

class OpenCLEnvironment {
public:

	cl_platform_id platformID;
	cl_device_id deviceID;
	cl_context context;
	cl_command_queue queue;
	cl_program program;

	OpenCLEnvironment(const cl_uint platformIndex, const cl_uint deviceIndex, const char* programPath){
		printf("\n=================== OPENCL INFO ====================\n");
		platformID = select_platform(platformIndex);
		deviceID = select_device(platformID, deviceIndex);
		context = create_context(platformID, deviceID);
		queue = create_queue(context, deviceID);
		program = create_program(programPath, context, deviceID);
	}
};

class OBJ {
private:
	
	bool validData;
	uint verticesCount, uvsCount, normalsCount, facesCount;
	std::string obj_path;
	
	void init() {
		validData = false;
		verticesCount = uvsCount = normalsCount = facesCount = 0;
	}
	void clear() {
		init();
		vertex_vector.clear();
		facesVertexIndex_vector.clear();
	}
	bool OBJException(const char * strerror) {
		printf(" Error: %s!\n", strerror);
		return false;
	}
	
public:
	
	std::vector< glm::vec3 > vertex_vector;
	std::vector< glm::vec3 > normal_vector;
	std::vector< glm::vec2 > uv_vector;
	std::vector< uint > facesVertexIndex_vector;
	std::vector< uint > facesNormalIndex_vector;
	std::vector< uint > facesUVIndex_vector;

	OBJ(std::string path){
		this->obj_path = path;
		init();
		load(obj_path);
	}
	
	bool load(std::string path){
		
		clear();

		printf("\n======================= LOAD =======================\n");
		printf(" > Loading %s...\n", path.c_str());
		
		INIT_TIMER;
		START_TIMER;
		
		FILE * in_file = fopen(path.c_str(), "r");
		if( in_file == NULL ) return OBJException("fopen() -> Impossible to open the file");
		while( 1 ){ // parsing
			char lineHeader[128];
			// read the first word of the line
			int res = fscanf(in_file, "%s", lineHeader);
			if (res == EOF) break; // EOF = End Of File. Quit the loop.
			// else : parse lineHeader
			if ( strcmp( lineHeader, "v" ) == 0 ){
				glm::vec3 vertex;
				fscanf(in_file, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z );
				vertex_vector.push_back(vertex);
				verticesCount++;
			} 
			else if ( strcmp( lineHeader, "vn" ) == 0 ){
				glm::vec3 normal;
				fscanf(in_file, "%f %f %f\n", &normal.x, &normal.y, &normal.z );
				normal_vector.push_back(normal);
				normalsCount++;
			}
			else if ( strcmp( lineHeader, "vt" ) == 0 ){
				glm::vec2 uv;
				fscanf(in_file, "%f %f\n", &uv.x, &uv.y);
				uv_vector.push_back(uv);
				uvsCount++;
			}
			else if ( strcmp( lineHeader, "f" ) == 0 ){
				uint faceVertexIndex[3], faceUvIndex[3], faceNormalIndex[3];
				
				char line[512];
				fgets( line, 512, in_file);
				
				int	match = sscanf(line, "%d/%d/%d %d/%d/%d %d/%d/%d\n", &faceVertexIndex[0], &faceUvIndex[0], &faceNormalIndex[0], &faceVertexIndex[1], &faceUvIndex[1], &faceNormalIndex[1], &faceVertexIndex[2], &faceUvIndex[2], &faceNormalIndex[2]);
				if(match < 9) match = sscanf(line,"%d//%d %d//%d %d//%d\n", &faceVertexIndex[0], &faceNormalIndex[0], &faceVertexIndex[1], &faceNormalIndex[1], &faceVertexIndex[2], &faceNormalIndex[2]);
				if(match < 6) match = sscanf(line,"%d %d %d\n", &faceVertexIndex[0], &faceVertexIndex[1], &faceVertexIndex[2]);
				if(match < 3) { validData=false; OBJException("parser -> Try exporting with other options");}
				{
					facesVertexIndex_vector.push_back(faceVertexIndex[0]);
					facesVertexIndex_vector.push_back(faceVertexIndex[1]);
					facesVertexIndex_vector.push_back(faceVertexIndex[2]);
				}
				if(normalsCount>0){
					facesNormalIndex_vector.push_back(faceNormalIndex[0]);
					facesNormalIndex_vector.push_back(faceNormalIndex[1]);
					facesNormalIndex_vector.push_back(faceNormalIndex[2]);
				}
				if(uvsCount>0) {
					facesUVIndex_vector.push_back(faceUvIndex[0]);
					facesUVIndex_vector.push_back(faceUvIndex[1]);
					facesUVIndex_vector.push_back(faceUvIndex[2]);
				}
				facesCount++;
			}
		}
		printElapsedTime_ms("load OBJ", ELAPSED_TIME);
		validData = true;
		return true;
	}

	bool write(std::string out_path) {
		printf("\n===================== SAVE OBJ =====================\n");
		printf(" > Saving result to %s...\n", out_path.c_str());
		INIT_TIMER;
		START_TIMER;
		char line[512];

		FILE * out_file = fopen(out_path.c_str(), "w");
		if( out_file == NULL ) return OBJException("fopen() -> Impossible to open the file");

		for(int i=0; i<verticesCount; i++){
			fprintf(out_file, "v %f %f %f\n", vertex_vector[i].x, vertex_vector[i].y, vertex_vector[i].z);
		}
		for(int i=0; i<uv_vector.size(); i++){
			fprintf(out_file, "vt %f %f\n", uv_vector[i].x, uv_vector[i].y);
		}
		for(int i=0; i<normal_vector.size(); i++){
			fprintf(out_file, "vn %f %f %f\n", normal_vector[i].x, normal_vector[i].y, normal_vector[i].z);
		}
		for(int i=0; i<facesCount; i++){
			fprintf(out_file, "f %d %d %d\n", facesVertexIndex_vector[i*3], facesVertexIndex_vector[i*3+1], facesVertexIndex_vector[i*3+2]);
		}
		printElapsedTime_ms("write OBJ", ELAPSED_TIME);
	}

	bool hasValidData() const { return validData; }
	std::string getPathName() const { return obj_path; }
	uint getVerticesCount() const { return verticesCount; }
	uint getFacesCount() const { return facesCount; }
};

class Smoothing {
private:
	bool sortVertex, sortAdjs, coalescence, localMemory;
	
	uint nels, nadjs, minAdjsCount, maxAdjsCount;
	float meanAdjsCount;
	std::vector< uint >* obj_adjacents_arrayVector;
	
	const OpenCLEnvironment* OCLenv;
	uint localWorkSize;
	OBJ* obj;
	
	struct vertex_struct{
		uint currentIndex;
		uint obj_vertex_vector_Index;
		uint adjsCount;
		std::vector<vertex_struct*> adjs;
	};
		
	struct face_struct{
		vertex_struct * faceVertices_struct_array[3];
	};

	struct vertex_struct_comparator {
		bool operator() (vertex_struct* i,vertex_struct* j) { return (i->currentIndex < j->currentIndex);}
	} vertex_struct_cmp;
	
	bool orderedUniqueInsert(std::vector< uint >*vertexAdjacents, const uint vertexID) {
		std::vector< uint >::iterator it;
		for(it = vertexAdjacents->begin(); it != vertexAdjacents->end() && *it < vertexID; it++) {}
		if(it == vertexAdjacents->end() || *it != vertexID) {
			vertexAdjacents->insert(it, vertexID);
			return true;
	}
	return false;
}
//
public:
	size_t memsize, ajdsmemsize;
	float* vertex4_array;
	uint* adjs_array;

	uint countingSize;
	uint* adjCounter;
	
	void parseBitFlags(const unsigned char flagsOpt) {				
		coalescence = ( flagsOpt & OptCoalescence );
		localMemory = ( flagsOpt & OptLocalMemory );
		// coalescence access requires sortVertex = true
		sortVertex  = ( flagsOpt & OptSortVertex ) || coalescence;
		sortAdjs    = ( flagsOpt & OptSortAdjs );
	}
	
	Smoothing(const OpenCLEnvironment* OCLenv, OBJ* obj, const unsigned char flagsOpt, const uint localWorkSize){
		this->OCLenv = OCLenv;
		this->localWorkSize = localWorkSize;
		this->obj = new OBJ(*obj);
		parseBitFlags(flagsOpt);
		init();
	}
	
	unsigned int discoverAdjacents(const bool orderedInsert){
		unsigned int adjsCount = 0;
		// Discover adjacents vertex for each vertex
		obj_adjacents_arrayVector = new std::vector< uint >[nels];

		for(int i=0; i<obj->facesVertexIndex_vector.size(); i+=3){
			uint vertexID1 = obj->facesVertexIndex_vector[i] - 1;
			uint vertexID2 = obj->facesVertexIndex_vector[i+1] - 1;
			uint vertexID3 = obj->facesVertexIndex_vector[i+2] - 1;

			std::vector< uint >* adjacent1 = &obj_adjacents_arrayVector[vertexID1];
			std::vector< uint >* adjacent2 = &obj_adjacents_arrayVector[vertexID2];
			std::vector< uint >* adjacent3 = &obj_adjacents_arrayVector[vertexID3];
			
			if(orderedInsert && !sortVertex){
				if(orderedUniqueInsert(adjacent1, vertexID2)) adjsCount++;
				if(orderedUniqueInsert(adjacent1, vertexID3)) adjsCount++;
				if(orderedUniqueInsert(adjacent2, vertexID1)) adjsCount++;
				if(orderedUniqueInsert(adjacent2, vertexID3)) adjsCount++;
				if(orderedUniqueInsert(adjacent3, vertexID1)) adjsCount++;
				if(orderedUniqueInsert(adjacent3, vertexID2)) adjsCount++;
			} else {
				if (std::find(adjacent1->begin(), adjacent1->end(), vertexID2) == adjacent1->end()) { adjacent1->push_back(vertexID2); adjsCount++; }
				if (std::find(adjacent1->begin(), adjacent1->end(), vertexID3) == adjacent1->end()) { adjacent1->push_back(vertexID3); adjsCount++; }
				if (std::find(adjacent2->begin(), adjacent2->end(), vertexID1) == adjacent2->end()) { adjacent2->push_back(vertexID1); adjsCount++; }
				if (std::find(adjacent2->begin(), adjacent2->end(), vertexID3) == adjacent2->end()) { adjacent2->push_back(vertexID3); adjsCount++; }
				if (std::find(adjacent3->begin(), adjacent3->end(), vertexID1) == adjacent3->end()) { adjacent3->push_back(vertexID1); adjsCount++; }
				if (std::find(adjacent3->begin(), adjacent3->end(), vertexID2) == adjacent3->end()) { adjacent3->push_back(vertexID2); adjsCount++; }
			}
		}
				
		return adjsCount;
	}

	void calcMinMaxAdjCount() {
		maxAdjsCount = minAdjsCount = obj_adjacents_arrayVector[0].size();

		for(int i=0; i<nels; i++) {
			uint currentAdjsCount = obj_adjacents_arrayVector[i].size();
			if(currentAdjsCount > maxAdjsCount) maxAdjsCount = currentAdjsCount;
			else if(currentAdjsCount < minAdjsCount) minAdjsCount = currentAdjsCount;
		}
	}

	vertex_struct** orderVertexByAdjCount(const bool sortAdjs) {
		
		// vertex_arrayStruct creation
		vertex_struct** vertex_arrayStruct = new vertex_struct* [nels];

		for(int i=0; i<nels; i++){
			vertex_struct* v = new vertex_struct();
			v->currentIndex = i;
			v->obj_vertex_vector_Index = i;
			
			//number of vertices adjacents to vertex i
			v->adjsCount = obj_adjacents_arrayVector[i].size();
			vertex_arrayStruct[i] = v;
		}

		for(int i=0; i<nels; i++)
			for(uint index : obj_adjacents_arrayVector[i])
				vertex_arrayStruct[i]->adjs.push_back(vertex_arrayStruct[index]);

		// faces_arrayStruct creation
		uint facesCount = obj->getFacesCount();
		face_struct* faces_arrayStruct = new face_struct[facesCount];

		for(int i=0; i<facesCount; i++){
			faces_arrayStruct[i].faceVertices_struct_array[0] = vertex_arrayStruct[ obj->facesVertexIndex_vector[i*3] - 1 ];
			faces_arrayStruct[i].faceVertices_struct_array[1] = vertex_arrayStruct[ obj->facesVertexIndex_vector[i*3+1] - 1 ];
			faces_arrayStruct[i].faceVertices_struct_array[2] = vertex_arrayStruct[ obj->facesVertexIndex_vector[i*3+2] - 1 ];
		}


		//COUNTING SORT
		countingSize = maxAdjsCount-minAdjsCount+1;
		uint counting_array[countingSize] = {0};

		for(int i=0; i<nels; i++) ++counting_array[obj_adjacents_arrayVector[i].size() - minAdjsCount];

		//counting sort sums
		for(int i=1; i<countingSize; i++) counting_array[i] += counting_array[i-1];

		vertex_struct** orderedVertex_arrayStruct = new vertex_struct*[nels];
		//insert ordered vertex
		for(int i=0; i<nels; i++) {
			vertex_struct * currVertex = vertex_arrayStruct[i];
			uint currentAdjsCount = currVertex->adjsCount;
			
			//calc new orderedIndex from counting_array
			uint orderedIndex = --counting_array[ currentAdjsCount - minAdjsCount ];
			
			//set to vertex in struct the new orderedIndex
			currVertex->currentIndex = nels-1-orderedIndex;

			orderedVertex_arrayStruct[nels-1 - orderedIndex] = currVertex;
		}

		// Update vertex index [1-based] of faces in obj
		for(int i=0; i<obj->getFacesCount(); i++){
			obj->facesVertexIndex_vector[i*3] = faces_arrayStruct[i].faceVertices_struct_array[0]->currentIndex + 1;
			obj->facesVertexIndex_vector[i*3+1] = faces_arrayStruct[i].faceVertices_struct_array[1]->currentIndex + 1;
			obj->facesVertexIndex_vector[i*3+2] = faces_arrayStruct[i].faceVertices_struct_array[2]->currentIndex + 1;
		}


		adjCounter = new uint[maxAdjsCount];
		for(int i=0; i<maxAdjsCount; i++) adjCounter[i] = 0;
		for(int i=0; i<maxAdjsCount; i++){
			for(int j=0; j<nels; j++){
				vertex_struct * currVertex = vertex_arrayStruct[j];
				if(currVertex->adjsCount>=i+1) adjCounter[i]++;
			}
		}
		
		if(sortAdjs) {
			for(int i=0; i<nels; i++) {
				vertex_struct * currVertex = orderedVertex_arrayStruct[i];
				std::vector<vertex_struct*> * v = &(currVertex->adjs);
				
				std::sort(v->begin(), v->end(), vertex_struct_cmp);
			}
		}
		
		return orderedVertex_arrayStruct;
	}
	
	void fillVertex4Array(float* vertex4_array){
		INIT_TIMER;
		START_TIMER;
		uint currentAdjStartIndex = 0;
		
		for(int i=0; i<nels; i++) {	
			glm::vec3 vertex = obj->vertex_vector[i];
			vertex4_array[4*i] = vertex.x;
			vertex4_array[4*i+1] = vertex.y;
			vertex4_array[4*i+2] = vertex.z;

			uint currentAdjsCount = obj_adjacents_arrayVector[i].size();
			uint* adjIndexPtr = (uint*)(&vertex4_array[4*i+3]);
			*adjIndexPtr = ((uint)currentAdjStartIndex)<<6;
			*adjIndexPtr += (currentAdjsCount<<26)>>26;
			
			currentAdjStartIndex += currentAdjsCount;
		}
		printElapsedTime_ms("fillVertex4Array", ELAPSED_TIME);
	}
	
	void fillOrderedVertex4Array(float* vertex4_array, vertex_struct** orderedVertex_arrayStruct, const bool coalescence){
		INIT_TIMER;
		START_TIMER;
		uint currentAdjStartIndex = 0;
		
		for(int i=0; i<nels; i++) {
			vertex_struct * currVertex = orderedVertex_arrayStruct[i];
			
			glm::vec3 vertex = obj->vertex_vector[currVertex->obj_vertex_vector_Index];
			vertex4_array[4*i] = vertex.x;
			vertex4_array[4*i+1] = vertex.y;
			vertex4_array[4*i+2] = vertex.z;

			uint currentAdjsCount = currVertex->adjsCount;
			uint* adjIndexPtr = (uint*)(&vertex4_array[4*i+3]);
			
			if(coalescence) *adjIndexPtr = currentAdjsCount;
			else {
				*adjIndexPtr = ((uint)currentAdjStartIndex)<<6;
				*adjIndexPtr += (currentAdjsCount<<26)>>26;
			}
			
			currentAdjStartIndex += currentAdjsCount;
		}
		printElapsedTime_ms("fillOrderedVertex4Array", ELAPSED_TIME);
	}
	
	void fillVertexAdjsArray() {
		INIT_TIMER;
		START_TIMER;
		uint adjsIndex = 0;
		for(int i=0; i<nels; i++)
			for( uint currentAdjIndex : obj_adjacents_arrayVector[i])
				adjs_array[adjsIndex++] = currentAdjIndex;
		printElapsedTime_ms("fillVertexAdjsArray", ELAPSED_TIME);
	}
	
	void fillOrderedVertexAdjsArray(vertex_struct** orderedVertex_arrayStruct, const bool coalescence) {
		INIT_TIMER;
		START_TIMER;
		uint adjsIndex = 0;
		
		if(coalescence) {
			for(int i=0; i<maxAdjsCount; i++)
				for(int j=0; j<nels; j++){
					vertex_struct * currVertex = orderedVertex_arrayStruct[j];
					if( currVertex->adjsCount >= i+1 )
						adjs_array[adjsIndex++] = currVertex->adjs[i]->currentIndex;
				}
		}
		else { //TO-DO non coalescence access (change kernel adjs access)
			for(int i=0; i<nels; i++) {
				vertex_struct * currVertex = orderedVertex_arrayStruct[i];
				for( vertex_struct* adj : currVertex->adjs)
					adjs_array[adjsIndex++] = adj->currentIndex;
			}
		}
		
		printElapsedTime_ms("fillOrderedVertexAdjsArray", ELAPSED_TIME);
	}
	
	void init(){
		printf("\n===================== INIT DATA ====================\n");
		printf(" > Initializing data...\n");
		
		nels = nadjs = minAdjsCount = maxAdjsCount = 0;
		meanAdjsCount = 0.0f;
		
		if(obj == nullptr || !obj->hasValidData()) {
			printf(" Error: Smoothing(OBJ) -> obj null or invalid data");
			exit(-1);
		}
	
		nels          = obj->getVerticesCount();
		nadjs         = discoverAdjacents(sortAdjs);
		meanAdjsCount = nadjs/(float)nels;
		//-----------------------------------
		vertex4_array = new float[4*nels];
		adjs_array    = new uint[nadjs];
		//-----------------------------------
		memsize       = 4*nels*sizeof(float);
		ajdsmemsize   =  nadjs*sizeof(uint);
		
		calcMinMaxAdjCount();

		if(sortVertex) {
			vertex_struct** orderedVertex_arrayStruct;
			orderedVertex_arrayStruct = orderVertexByAdjCount(sortAdjs);
			fillOrderedVertex4Array(vertex4_array, orderedVertex_arrayStruct, coalescence);
			fillOrderedVertexAdjsArray(orderedVertex_arrayStruct, coalescence);
		}
		else {
			fillVertex4Array(vertex4_array);
			fillVertexAdjsArray();
		}
	}

	void execute(uint iterations, float lambda, float mi, const bool writeOBJ){

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
		std::cout << " SortVertex: " << (sortVertex?"true":"false") << std::endl;
		std::cout << " SortAdjs: " << (sortAdjs?"true":"false") << std::endl;
		std::cout << " Coalescence: " << (coalescence?"true":"false") << std::endl;
		std::cout << " LocalMemory: " << (localMemory?"true":"false") << std::endl;
		std::cout << " LocalWorkSize: " << localWorkSize << std::endl;
		
		cl_int err;
		// Create Buffers
		cl_mem cl_vertex4_array = clCreateBuffer(OCLenv->context, CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR, memsize, vertex4_array, &err);
		cl_mem cl_adjs_array = clCreateBuffer(OCLenv->context, CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR, ajdsmemsize, adjs_array, &err);
		cl_mem cl_result_vertex4_array = clCreateBuffer(OCLenv->context, CL_MEM_READ_WRITE, memsize, NULL, &err);
		cl_mem cl_adjsCounter;
		if(coalescence) cl_adjsCounter = clCreateBuffer(OCLenv->context, CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR, maxAdjsCount*sizeof(uint), adjCounter, &err);
		
		// Extract kernels
		cl_kernel smooth_k;
		if(coalescence && localMemory) smooth_k = clCreateKernel(OCLenv->program, "smooth_coalescence_lmem", &err);
		else if(coalescence) smooth_k = clCreateKernel(OCLenv->program, "smooth_coalescence", &err);
		else if(localMemory) smooth_k = clCreateKernel(OCLenv->program, "smooth_lmem", &err);
		else smooth_k = clCreateKernel(OCLenv->program, "smooth", &err);
		ocl_check(err, "create kernel smooth");

		// Set preferred_wg size from device info
		err = clGetKernelWorkGroupInfo(smooth_k, OCLenv->deviceID, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(preferred_wg_smooth), &preferred_wg_smooth, NULL);
		
		printf("\n================== KERNEL LAUNCH ===================\n");
		
		cl_event smooth_evts[iterations*2];
		if(coalescence && localMemory) {
			for(int iter=0; iter<iterations; iter++) {
				smooth_evts[iter*2] = smooth_coalescence_lmem(OCLenv->queue, smooth_k, cl_vertex4_array, cl_adjs_array, cl_adjsCounter, cl_result_vertex4_array, nels, lambda, !!iter, smooth_evts+(iter*2)-1);
				smooth_evts[iter*2+1] = smooth_coalescence_lmem(OCLenv->queue, smooth_k, cl_result_vertex4_array, cl_adjs_array, cl_adjsCounter, cl_vertex4_array, nels, mi, 1, smooth_evts+(iter*2));
			}
		}
		else if(coalescence) {
			for(int iter=0; iter<iterations; iter++) {
				smooth_evts[iter*2] = smooth_coalescence(OCLenv->queue, smooth_k, cl_vertex4_array, cl_adjs_array, cl_adjsCounter, cl_result_vertex4_array, nels, lambda, !!iter, smooth_evts+(iter*2)-1);
				smooth_evts[iter*2+1] = smooth_coalescence(OCLenv->queue, smooth_k, cl_result_vertex4_array, cl_adjs_array, cl_adjsCounter, cl_vertex4_array, nels, mi, 1, smooth_evts+(iter*2));
			}
		}
		else if(localMemory) {
			for(int iter=0; iter<iterations; iter++) {
				smooth_evts[iter*2] = smooth_lmem(OCLenv->queue, smooth_k, cl_vertex4_array, cl_adjs_array, cl_result_vertex4_array, nels, lambda, !!iter, smooth_evts+(iter*2)-1);
				smooth_evts[iter*2+1] = smooth_lmem(OCLenv->queue, smooth_k, cl_result_vertex4_array, cl_adjs_array, cl_vertex4_array, nels, mi, 1, smooth_evts+(iter*2));
			}
		}
		else {
			for(int iter=0; iter<iterations; iter++) {
				smooth_evts[iter*2] = smooth(OCLenv->queue, smooth_k, cl_vertex4_array, cl_adjs_array, cl_result_vertex4_array, nels, lambda, !!iter, smooth_evts+(iter*2)-1);
				smooth_evts[iter*2+1] = smooth(OCLenv->queue, smooth_k, cl_result_vertex4_array, cl_adjs_array, cl_vertex4_array, nels, mi, 1, smooth_evts+(iter*2));
			}
		}
		
		// Copy result
		cl_event copy_evt;
		float *result_vertex4_array = (float *)clEnqueueMapBuffer(
			OCLenv->queue, cl_vertex4_array,
			CL_TRUE, CL_MAP_READ,
			0, memsize,
			1, &smooth_evts[iterations*2-1],
			&copy_evt, &err);
		ocl_check(err, "map buffer vertex4_array");
		
		for(int i=0; i<nels; i++) {
			obj->vertex_vector[i].x = result_vertex4_array[i*4];
			obj->vertex_vector[i].y = result_vertex4_array[i*4+1];
			obj->vertex_vector[i].z = result_vertex4_array[i*4+2];
		}

		double totalRuntime_ms = 0.0;
		double meanRuntime_ms = 0.0;
		double meanRuntime_ns = 0.0;

		for(int i=0; i<iterations*2; i++) totalRuntime_ms+=runtime_ms(smooth_evts[i]);

		meanRuntime_ms = totalRuntime_ms/(iterations*2.0);
		meanRuntime_ns = meanRuntime_ms*1.0e6;

		if(coalescence && localMemory)
			printf(" coal lmem smooth time:\t%gms\n", meanRuntime_ms);
		else if(coalescence) {
			printf(" coal smooth time:\t%gms\t%gGB/s\n", meanRuntime_ms,
			(2.0*memsize + meanAdjsCount*memsize + meanAdjsCount*sizeof(int)*nels + meanAdjsCount*sizeof(int)*nels)/meanRuntime_ns);
		}
		else if(localMemory)
			printf(" lmem smooth time:\t%gms\n", meanRuntime_ms);
		else{
			printf(" smooth time:\t%gms\t%gGB/s\n", meanRuntime_ms,
			(2.0*memsize + meanAdjsCount*4*sizeof(float)*nels + meanAdjsCount*sizeof(int)*nels)/meanRuntime_ns);
		}
		
		printf(" copy time:\t%gms\t%gGB/s\n", runtime_ms(copy_evt), (2.0*memsize)/runtime_ns(copy_evt));
		
		printf(" kernels total time \t%gms\n", totalRuntime_ms);
		printf(" ~ %g smooth pass(es)/sec\n", (iterations*2) / (totalRuntime_ms / 1.0e3) );
		
		if(writeOBJ) obj->write(OUT_MESH);
	}

	cl_event smooth(cl_command_queue queue, cl_kernel smooth_k, cl_mem cl_vertex4_array, cl_mem cl_adjs_array, cl_mem cl_result_vertex4_array, cl_uint nels, cl_float factor, cl_int waintingSize, cl_event* waitingList) {
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
			waintingSize, waintingSize==0?NULL:waitingList, /* waiting list */
			&smooth_evt);
		ocl_check(err, "enqueue kernel smooth");
		return smooth_evt;
	}
	
	cl_event smooth_lmem(cl_command_queue queue, cl_kernel smooth_k, cl_mem cl_vertex4_array, cl_mem cl_adjs_array, cl_mem cl_result_vertex4_array, cl_uint nels, cl_float factor, cl_int waintingSize, cl_event* waitingList) {
		size_t lws[] = { localWorkSize };
		size_t gws[] = { round_mul_up(nels, localWorkSize) };
		cl_event smooth_evt;
		cl_int err;

		//printf("smooth gws: %d | %zu => %zu\n", nels, preferred_wg_smooth, gws[0]);

		// Setting arguments
		err = clSetKernelArg(smooth_k, 0, sizeof(cl_vertex4_array), &cl_vertex4_array);
		ocl_check(err, "set smooth arg 0");
		err = clSetKernelArg(smooth_k, 1, lws[0]*sizeof(cl_float4), NULL);
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
			waintingSize, waintingSize==0?NULL:waitingList, /* waiting list */
			&smooth_evt);
		ocl_check(err, "enqueue kernel smooth");
		return smooth_evt;
	}

	cl_event smooth_coalescence(cl_command_queue queue, cl_kernel smooth_k, cl_mem cl_vertex4_array, cl_mem cl_adjs_array, cl_mem cl_adjsCounter, cl_mem cl_result_vertex4_array, cl_uint nels, cl_float factor, cl_int waintingSize, cl_event* waitingList) {
		size_t lws[] = { localWorkSize };
		size_t gws[] = { round_mul_up(nels, localWorkSize)};
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
			waintingSize, waintingSize==0?NULL:waitingList, /* waiting list */
			&smooth_evt);
		ocl_check(err, "enqueue kernel smooth");
		return smooth_evt;
	}
	
	cl_event smooth_coalescence_lmem(cl_command_queue queue, cl_kernel smooth_k, cl_mem cl_vertex4_array, cl_mem cl_adjs_array, cl_mem cl_adjsCounter, cl_mem cl_result_vertex4_array, cl_uint nels, cl_float factor, cl_int waintingSize, cl_event* waitingList) {
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
		err = clSetKernelArg(smooth_k, 3, maxAdjsCount*sizeof(cl_uint), NULL);
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
			waintingSize, waintingSize==0?NULL:waitingList, /* waiting list */
			&smooth_evt);
		ocl_check(err, "enqueue kernel smooth");
		return smooth_evt;
	}	
};

class CommandOptions {
private:

	void cmdOptionsHelp() {
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

public:
	uint platformID;
	uint deviceID;
	std::string input_mesh;
	uint iterations;
	float lambda;
	float mi;
	bool writeObj;
	char kernelOptions;
	uint lws;
	
	CommandOptions(const int argc, const std::vector<std::string> argv) {
		initCmdOptions();
		cmdOptionsParser(argc, argv);
	}
	
	void initCmdOptions() {
		platformID    = OCL_PLATFORM;
		deviceID      = OCL_DEVICE;
		input_mesh    = IN_MESH;
		iterations    = 1;
		lambda        = 0.5f;
		mi            = -0.5f;
		writeObj      = 0;
		kernelOptions = OptNoOption;
		lws           = 32;
	}
	
	void cmdOptionsParser(const int argc, const std::vector<std::string> argv) {
		if(argc == 1) {
			std::string str = argv[0];
			if(str == "-h" || str == "-help") cmdOptionsHelp();
			else { printf(" Missing -parameter specification. (use -help)\n"); exit(-1); }
		}
		
		for(int i=0; i<argc-1; i+=2) {
			std::string param = argv[i];
			std::string value = argv[i+1];
			
			if(param[0] != '-') { printf(" Missing -parameter specification. (use -help)\n"); exit(-1); }
			if(value[0] == '-') { printf(" Missing value after -parameter. (use -help)\n"); exit(-1); }
			
			if(param == "-m" || param == "-mesh") input_mesh = "res/"+value+".obj";
			else if(param == "-d" || param == "-dev" || param == "-device") deviceID = stoi(value);
			else if(param == "-p" || param == "-plat" || param == "-platform") platformID = stoi(value);
			else if(param == "-i" || param == "-iter" || param == "-iterations") iterations = stoi(value);
			else if(param == "-f" || param == "-facts" || param == "-factors") {
				std::string factor1 = argv[i+1];
				std::string factor2;
				lambda = stof(factor1);
				if(i+2 < argc && (argv[i+2][0] >= '0' && argv[i+2][0] <= '9')) {
					factor2 = argv[i+2];
					mi = -stof(factor2);
					i++;
				}
				else mi = -lambda;
			}
			else if(param == "-w" || param == "-write" || param == "-b_write") writeObj = !!stoi(value);
			else if(param == "-o" || param == "-opt" || param == "-options") {
				std::string currentOpt;
				while(i+1 < argc && argv[i+1][0] != '-') {
					currentOpt = argv[i+1];
					
					if(currentOpt == "sortVertex") kernelOptions |= OptSortVertex;
					else if(currentOpt == "sortAdjs") kernelOptions |=  OptSortAdjs;
					else if(currentOpt == "coalescence" || currentOpt == "coal") kernelOptions |= OptCoalescence;
					else if(currentOpt == "localMemory" || currentOpt == "lmem") kernelOptions |= OptLocalMemory;
					else { printf(" Wrong value after -options. (use -help)\n"); exit(-1); }
					i++;
				}
				i--;
			}
			else if(param == "-g" || param == "-l" || param == "-lws") lws = stoi(value);
			else { printf(" Wrong -parameter. (use -help)\n"); exit(-1); }
		}
	}

};

int main(const int argc, char *argv[]) {
	
	std::vector<std::string> all_args(argv+1, argv+argc);

	CommandOptions cmdOptions(all_args.size(), all_args);
	
	OpenCLEnvironment *OCLenv = new OpenCLEnvironment(cmdOptions.platformID, cmdOptions.deviceID, OCL_FILENAME);
	OBJ *obj = new OBJ(cmdOptions.input_mesh);
	
	std::string cmdInput, param;
	do{
		Smoothing s(OCLenv, obj, cmdOptions.kernelOptions, cmdOptions.lws);
		s.execute(cmdOptions.iterations, cmdOptions.lambda, cmdOptions.mi, cmdOptions.writeObj);
		
		std::cout << "\n\n > Type new args, newline to use default values or exit (-e). (can't change input mesh)\n";
		std::cout << " $ " << cmdOptions.input_mesh  << " " << std::flush;
		getline(std::cin, cmdInput);
		if(cmdInput == "exit" || cmdInput == "-e") break;
		std::istringstream iss(cmdInput);
		all_args.clear();
		while( iss >> param ) all_args.push_back(param);
		cmdOptions.initCmdOptions();
		cmdOptions.cmdOptionsParser(all_args.size(), all_args);
	}while(1);
	
	delete OCLenv;
	delete obj;
	return 0;
}