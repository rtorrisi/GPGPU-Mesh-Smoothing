#include "OBJ.h"
#include "Timer.h"
#include <fstream>
#include <sstream>

OBJ::OBJ(std::string path)
{
	obj_path = path;
	init();
	validData = load(obj_path);
}

OBJ::~OBJ() { clear(); }

bool OBJ::OBJException(const char * strerror)
{
	printf(" Error: %s!\n", strerror);
	return false;
}

void OBJ::init()
{
	validData = false;
	verticesCount = uvsCount = normalsCount = facesCount = 0;
}

void OBJ::clear()
{
	init();
	vertex_vector.clear();
	facesVertexIndex_vector.clear();
}

inline void OBJ::split(std::string const& s, const char delimiter, uint output[]) const
{
	size_t start = 0;
	size_t end = s.find_first_of(delimiter);

	std::string str;
	int i = 0;

	output[1] = 0;
	output[2] = 0;

	while (end <= std::string::npos)
	{
		str = s.substr(start, end - start);

		output[i++] = (str != "") ? stoi(str) : 0;

		if (end == std::string::npos) break;

		start = end + 1;
		end = s.find_first_of(delimiter, start);
	}
}

bool OBJ::load(std::string path)
{

	clear();

	printf("\n======================= LOAD =======================\n");
	printf(" > Loading %s...\n", path.c_str());

	INIT_TIMER;
	START_TIMER;

	std::ifstream in;
	in.open(path);
	
	if (in.fail()) return false;

	std::stringstream ss;
	ss << in.rdbuf();

	std::string keyword, a, b, c;

	uint s1[3]{ 0 }, s2[3]{ 0 }, s3[3]{ 0 };

	while (ss >> keyword)
	{
		if (keyword == "v")
		{
			ss >> a >> b >> c;
			vertex_vector.push_back(glm::vec3(stof(a), stof(b), stof(c)));
			verticesCount++;
		}
		else if (keyword == "vn")
		{
			ss >> a >> b >> c;
			normal_vector.push_back(glm::vec3(stof(a), stof(b), stof(c)));
			normalsCount++;
		}
		else if (keyword == "vt")
		{
			ss >> a >> b;
			uv_vector.push_back(glm::vec2(stof(a), stof(b)));
			uvsCount++;
		}
		else if (keyword == "f")
		{
			ss >> a >> b >> c;

			split(a, '/', s1);
			split(b, '/', s2);
			split(c, '/', s3);

			facesVertexIndex_vector.push_back(s1[0]);
			facesNormalIndex_vector.push_back(s1[1]);
			facesUVIndex_vector.push_back(s1[2]);

			facesVertexIndex_vector.push_back(s2[0]);
			facesNormalIndex_vector.push_back(s2[1]);
			facesUVIndex_vector.push_back(s2[2]);

			facesVertexIndex_vector.push_back(s3[0]);
			facesNormalIndex_vector.push_back(s3[1]);
			facesUVIndex_vector.push_back(s3[2]);

			facesCount++;
		}
	}

	PRINT_ELAPSED_TIME("load OBJ", ELAPSED_TIME);
	return true;
}

bool OBJ::write(std::string out_path)
{
	printf("\n===================== SAVE OBJ =====================\n");
	printf(" > Saving result to %s...\n", out_path.c_str());
	INIT_TIMER;
	START_TIMER;

	FILE * out_file = fopen(out_path.c_str(), "w");
	if (out_file == NULL) return OBJException("fopen() -> Impossible to open the file");

	for (uint i = 0; i<verticesCount; i++) {
		fprintf(out_file, "v %f %f %f\n", vertex_vector[i].x, vertex_vector[i].y, vertex_vector[i].z);
	}
	for (uint i = 0; i<uv_vector.size(); i++) {
		fprintf(out_file, "vt %f %f\n", uv_vector[i].x, uv_vector[i].y);
	}
	for (uint i = 0; i<normal_vector.size(); i++) {
		fprintf(out_file, "vn %f %f %f\n", normal_vector[i].x, normal_vector[i].y, normal_vector[i].z);
	}
	for (uint i = 0; i<facesCount; i++) {
		fprintf(out_file, "f %d %d %d\n", facesVertexIndex_vector[i * 3], facesVertexIndex_vector[i * 3 + 1], facesVertexIndex_vector[i * 3 + 2]);
	}
	PRINT_ELAPSED_TIME("write OBJ", ELAPSED_TIME);

	fclose(out_file);
	return true;
}

bool OBJ::hasValidData() const { return validData; }
std::string OBJ::getPathName() const { return obj_path; }

uint OBJ::getVerticesCount() const { return verticesCount; }
uint OBJ::getFacesCount() const { return facesCount; }
uint OBJ::getNormalsCount() const { return normalsCount; }
uint OBJ::getUVsCount() const { return uvsCount; }
