#pragma once

#include <vector>
#include "CL/cl.h"

class OBJ;
class OpenCLEnvironment;

typedef unsigned int uint;

class CommandOptions {
private:

	void cmdOptionsHelp();

public:

	enum Options {
		OptNoOption		   = 0x00, // 0x00 ==  0 == "00000000"
		OptSortVertex	   = 0x01, // 0x01 ==  1 == "00000001"
		OptSortAdjs		   = 0x02, // 0x02 ==  2 == "00000010"
		OptCoalescence	   = 0x04, // 0x04 ==  4 == "00000100"
		OptLocalMemory	   = 0x08, // 0x08 ==  8 == "00001000"
		OptWideLocalMemory = 0x10  // 0x10 == 16 == "00010000"
	}; //OptNoOption | OptSortVertex | OptSortAdjs | OptCoalescence | OptLocalMemory | OptLocalMemoryWide

	uint platformID;
	uint deviceID;
	std::string input_mesh;
	uint iterations;
	float lambda;
	float mi;
	bool writeObj;
	char kernelOptions;
	uint lws;

	CommandOptions(const int argc, const std::vector<std::string> argv);

	void initCmdOptions();

	void cmdOptionsParser(const int argc, const std::vector<std::string> argv);
};

class Smoothing {
private:
	size_t preferred_wg_smooth;

	bool sortVertex, sortAdjs, coalescence, localMemory, wideLocaMemory;

	uint nels, nadjs, minAdjsCount, maxAdjsCount;
	float meanAdjsCount;
	std::vector< uint >* obj_adjacents_arrayVector;

	const OpenCLEnvironment* OCLenv;
	uint localWorkSize;
	OBJ* obj;

	struct vertex_struct {
		uint currentIndex;
		uint obj_vertex_vector_Index;
		uint adjsCount;
		std::vector<vertex_struct*> adjs;
	};

	struct face_struct {
		vertex_struct * faceVertices_struct_array[3];
	};

	struct vertex_struct_comparator {
		bool operator() (vertex_struct* i, vertex_struct* j) { return (i->currentIndex < j->currentIndex); }
	} vertex_struct_cmp;

	bool orderedUniqueInsert(std::vector< uint >*vertexAdjacents, const uint vertexID);
	
public:
	size_t memsize, ajdsmemsize;
	float* vertex4_array;
	uint* adjs_array;

	uint countingSize;
	uint* adjCounter_array;

	void parseBitFlags(const unsigned char flagsOpt);

	Smoothing(const OpenCLEnvironment* OCLenv, OBJ* obj, const unsigned char flagsOpt, const uint localWorkSize);

	~Smoothing();

	unsigned int discoverAdjacents(const bool orderedInsert);

	void calcMinMaxAdjCount();

	vertex_struct** orderVertexByAdjCount(const bool sortAdjs);

	void fillVertex4Array(float* vertex4_array);

	void fillOrderedVertex4Array(float* vertex4_array, vertex_struct** orderedVertex_arrayStruct, const bool coalescence);

	void fillVertexAdjsArray();

	void fillOrderedVertexAdjsArray(vertex_struct** orderedVertex_arrayStruct, const bool coalescence);

	void init();

	void execute(uint iterations, float lambda, float mi, const bool writeOBJ);

	cl_event smooth(cl_command_queue queue, cl_kernel smooth_k, cl_mem cl_vertex4_array, cl_mem cl_adjs_array, cl_mem cl_result_vertex4_array, cl_uint nels, cl_float factor, cl_int waintingSize, cl_event* waitingList);

	cl_event smooth_lmem(cl_command_queue queue, cl_kernel smooth_k, cl_mem cl_vertex4_array, cl_mem cl_adjs_array, cl_mem cl_result_vertex4_array, cl_uint nels, cl_float factor, cl_int waintingSize, cl_event* waitingList);
	
	cl_event smooth_lmem_wide(cl_command_queue queue, cl_kernel smooth_k, cl_mem cl_vertex4_array, cl_mem cl_adjs_array, cl_mem cl_result_vertex4_array, cl_uint nels, cl_float factor, cl_int waintingSize, cl_event* waitingList);

	cl_event smooth_coalescence(cl_command_queue queue, cl_kernel smooth_k, cl_mem cl_vertex4_array, cl_mem cl_adjs_array, cl_mem cl_adjsCounter, cl_mem cl_result_vertex4_array, cl_uint nels, cl_float factor, cl_int waintingSize, cl_event* waitingList);

	cl_event smooth_coalescence_lmem(cl_command_queue queue, cl_kernel smooth_k, cl_mem cl_vertex4_array, cl_mem cl_adjs_array, cl_mem cl_adjsCounter, cl_mem cl_result_vertex4_array, cl_uint nels, cl_float factor, cl_int waintingSize, cl_event* waitingList);
};

