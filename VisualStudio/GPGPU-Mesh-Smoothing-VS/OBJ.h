#pragma once

#include <string>
#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

typedef unsigned int uint;

class OBJ {
private:

	bool validData;
	uint verticesCount, uvsCount, normalsCount, facesCount;
	std::string obj_path;

	bool OBJException(const char * strerror);

	void init();

	inline void split(std::string const& s, const char delimiter, uint output[]) const;

public:

	std::vector< glm::vec3 > vertex_vector;
	std::vector< glm::vec3 > normal_vector;
	std::vector< glm::vec2 > uv_vector;
	std::vector< uint > facesVertexIndex_vector;
	std::vector< uint > facesNormalIndex_vector;
	std::vector< uint > facesUVIndex_vector;

	OBJ(std::string path);

	~OBJ();

	void clear();

	bool load(std::string path);
	bool write(std::string out_path);

	bool hasValidData() const;
	
	std::string getPathName() const;

	uint getVerticesCount() const;
	uint getFacesCount() const;
	uint getNormalsCount() const;
	uint getUVsCount() const;
};

