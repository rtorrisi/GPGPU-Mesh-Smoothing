#include <iostream>
#include <string>
#include <sstream>

#include "OpenCLEnvironment.h"
#include "Smoothing.h"
#include "OBJ.h"

#define OCL_FILENAME "src/meshsmooth.ocl"

typedef unsigned int uint;

int main(const int argc, char *argv[]) {

	std::vector<std::string> all_args(argv + 1, argv + argc);

	CommandOptions cmdOptions((uint)all_args.size(), all_args);

	OpenCLEnvironment *OCLenv = new OpenCLEnvironment(cmdOptions.platformID, cmdOptions.deviceID, OCL_FILENAME);
	OBJ *obj = new OBJ(cmdOptions.input_mesh);

	std::string cmdInput, param;
	while (1) {

		Smoothing s(OCLenv, obj, cmdOptions.kernelOptions, cmdOptions.lws);
		s.execute(cmdOptions.iterations, cmdOptions.lambda, cmdOptions.mi, cmdOptions.writeObj);

		std::cout << "\n Type new args, newline to use default or exit (-e). (can't change mesh, plat & dev)\n";
		std::cout << " $ " << std::flush;
		getline(std::cin, cmdInput);

		if (cmdInput == "exit" || cmdInput == "-e") break;
		std::istringstream iss(cmdInput);
		all_args.clear();
		while (iss >> param) all_args.push_back(param);

		cmdOptions.initCmdOptions();
		cmdOptions.cmdOptionsParser((uint)all_args.size(), all_args);
	}

	delete OCLenv;
	delete obj;

	system("PAUSE");
	return 0;
}