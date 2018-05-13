#include <iostream>
#include <string>
#include <sstream>

#include "OpenCLEnvironment.h"
#include "Smoothing.h"
#include "OBJ.h"

#define OCL_FILENAME "src/meshsmooth.ocl"

typedef unsigned int uint;

OBJ * validateOBJInput(std::string & input_mesh_path) {
	OBJ *newOBJ = nullptr;

	std::string input;
	while (newOBJ == nullptr || !newOBJ->hasValidData()) {
		if (newOBJ != nullptr) {
			delete newOBJ;
			std::cout << "Error: .obj load failed.\n";
			std::cout << "Insert a different file path or exit (-exit or -e): ";
			getline(std::cin, input);
			if (input == "-exit" || input == "-e" || input == "exit") exit(0);
			else input_mesh_path = input;
		}
		newOBJ = new OBJ(input_mesh_path);
	}
	return newOBJ;
}

const char *byte_to_binary(int x)
{
	static char b[9];
	b[0] = '\0';

	int z;
	for (z = 128; z > 0; z >>= 1)
	{
		strcat(b, ((x & z) == z) ? "1" : "0");
	}

	return b;
}

int main(const int argc, char *argv[]) {

	bool readArgv = true;
	std::vector<std::string> all_args(argv + 1, argv + argc);
	CommandOptions cmdOptions((uint)all_args.size(), all_args);

	OpenCLEnvironment *OCLenv = nullptr;
	OBJ * obj = nullptr;

	std::string currentOBJLoaded = "";
	int currentPlatform = -1, currentDevice = -1;

	while (1) {
		
		bool isInputValid = false;
		do {

			cmdOptions.initCmdOptions();

			if (!readArgv) {
				std::cout << "\n Type args, newline to use default or exit (-exit or -e).\n $ " << std::flush;
				std::string cmdInput, param;
				getline(std::cin, cmdInput);

				if (cmdInput == "-exit" || cmdInput == "-e" || cmdInput == "exit") exit(0);

				std::istringstream iss(cmdInput);
				for (all_args.clear(); iss >> param; ) all_args.push_back(param);
			}
			else readArgv = false; //read argv the first time, only once

			isInputValid = cmdOptions.cmdOptionsParser((uint)all_args.size(), all_args);
		}
		while(!isInputValid);


		//check if we changed device or platform
		if (cmdOptions.platformID != currentPlatform || cmdOptions.deviceID != currentDevice) {
			if (OCLenv) delete OCLenv;
			OCLenv = new OpenCLEnvironment(cmdOptions.platformID, cmdOptions.deviceID, OCL_FILENAME);

			currentPlatform = cmdOptions.platformID;
			currentDevice = cmdOptions.deviceID;
		}
		
		//check if we have to load a new .obj file
		if (cmdOptions.input_mesh != currentOBJLoaded) {
			if (obj) delete obj;
			obj = validateOBJInput(cmdOptions.input_mesh);

			currentOBJLoaded = cmdOptions.input_mesh;
		}

		/*we need a new copy from *obj to avoid to re-read the .obj file
		each time we pass different command options until we change .obj file*/
		OBJ *newObjInstance = new OBJ(*obj);

		Smoothing s(OCLenv, cmdOptions.kernelOptions, cmdOptions.lws);
		if(s.init(newObjInstance)) s.execute(cmdOptions.iterations, cmdOptions.lambda, cmdOptions.mi, cmdOptions.writeObj);

		if (newObjInstance) delete newObjInstance;
	}

	if(obj) delete obj;
	if (OCLenv) delete OCLenv;

	return 0;
}