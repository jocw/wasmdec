#define __WASMDEC_VERSION "1.2.1"

#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iostream>
#include <iterator>

#include "cxxopts.hpp"
#include "decompiler/MultiDecompiler.h"

// Global variables to be passed to the decompiler
bool debugging = false,
		extra = false,
		memdump = false;
std::string infile, outfile;
std::vector<std::string> infiles; // will be empty if there's only one file to decompile
DisasmMode dmode;

// Helper functions
bool readFile(vector<char>* data, string path) {
	ifstream file(path);
	if (!file.eof() && !file.fail()) {
	    file.seekg(0, ios_base::end);
	    streampos fileSize = file.tellg();
	    data->resize(fileSize);
	    file.seekg(0, ios_base::beg);
	    file.read(&(data->operator[](0)), fileSize);
		return true;
	} else {
		return false;
	}
}
bool writeFile(string path, string data) {
	ofstream file(path);
	if (!file.eof() && !file.fail()) {
		file << data;
		file.close();
		return true;
	} else {
		return false;
	}
}
string getFileExt(string fname) {
	string::size_type idx = fname.rfind('.');
	if(idx != string::npos) {
	    string extension = fname.substr(idx + 1);
	    return extension;
	} else {
	    return "";
	}
}
DisasmMode getDisasmMode(string infile) {
	// Convert file extension to disassembler mode
	string ext = getFileExt(infile);
	if (ext == "wasm") {
		return DisasmMode::Wasm;
	} else if (ext == "wast") {
		return DisasmMode::Wast;
	} else if (ext == "js") {
		return DisasmMode::AsmJs;
	} else {
		return DisasmMode::None;
	}
}
int printVersion() {
	cerr << "wasmdec v" << __WASMDEC_VERSION << endl;
	return 0;
}
void enableDebugging() {
	debugging = true;
}
void enableExtra() {
	extra = true;
}
void enableMemdump() {
	memdump = true;
}
void setOutfile(string _outf) {
	outfile = _outf;
}
void setInfile(string _inf) {
	infile = _inf;
}
int performMemdump() {
	// Initialize a decompiler for memory dumping
	dmode = getDisasmMode(infile);
	DisasmConfig conf(debugging, extra, dmode);
	std::vector<char>* input = new std::vector<char>();
	if (!readFile(input, infile)) {
		std::cout << "ERROR: failed to read the input file!" << std::endl;
		return 1;
	}
	Decompiler decompiler(conf, input);

	// Dump the memory and table
	std::vector<char> mem = decompiler.dumpMemory();
	std::vector<char> table = decompiler.dumpTable();
	std::string memOutFile = outfile + ".mem",
		tableOutFile = outfile + ".table.bin",
		stringMemory = std::string(mem.begin(), mem.end()),
		stringTable = std::string(table.begin(), table.end());
	if (!writeFile(memOutFile, stringMemory)) {
		std::cout << "ERROR: failed to write memory file '" << memOutFile << '"' << std::endl;
		return 1;
	} else if (!writeFile(tableOutFile, stringTable)) {
		std::cout << "ERROR: failed to write memory file '" << tableOutFile << '"' << std::endl;
		return 1;
	}
	return 0;
}
int decompile(Decompiler* decompiler) {
	decompiler->decompile();
	if (decompiler->failed()) {
		std::cout << "ERROR: failed to decompile the binary." << std::endl;
		return 1;
	}
	string decompiledCode = decompiler->getEmittedCode();
	if (!writeFile(outfile, decompiledCode)) {
		std::cout << "ERROR: failed to write the output file." << std::endl;
		return 1;
	}
	return 0;
}
int multiDecompile(void) {
	DisasmConfig conf(debugging, extra, DisasmMode::Wasm);
	MultiDecompiler m(infiles, conf);
	if (m.failed) {
		std::cout << "ERROR: MultiDecompiler failed to decompile input." << std::endl;
		return 1;
	}
	string out = m.getOutput();
	if (!writeFile(outfile, out)) {
		std::cout << "ERROR: failed to write output file to disk!" << std::endl;
		return 1;
	}
	return 0;
}
int main(int argc, char* argv[]) {
	// Set up options
	cxxopts::Options opt("wasmdec", "WebAssembly to C decompiler");
	opt.add_options()
		("v,version", "Print wasmdec version")
		("d,debug", "Enable debug output")
		("m,memdump", "Dump memory instead of decompiling")
		("e,extra", "Output extra information to decompiled binary")
		("o,output", "Output C file", cxxopts::value<string>(outfile))
		("positional", "Input file", cxxopts::value<std::vector<std::string>>())
		("h,help", "Print usage")
		;
	opt.parse_positional({"positional"});
	opt.positional_help("[input file]")
      .show_positional_help();
	auto res = opt.parse(argc, argv);
	// Help and version, boring
	if (res.count("v")) {
		// version argument passed
		return printVersion();
	}
	if (res.count("h")) {
		std::cout << opt.help({"", "Group"}) << std::endl;
		return 0;
	}
	// Set default output file if there is none
	if (!res.count("o")) {
		outfile = "out.c";
	}
	// Parse decompiler flags
	if (res.count("d")) {
		enableDebugging();
	}
	if (res.count("m")) {
		enableMemdump();
	}

	if (res.count("e")) {
		enableExtra();
	}
	// Parse input file(s)
	if (res.count("positional")) {
		std::vector<std::string> _infiles;
		try {
			_infiles = res["positional"].as<std::vector<std::string>>();
		} catch (std::exception& e) {
			std::cout << "ERROR: invalid input files!" << std::endl
				<< std::endl << opt.help({"", "Group"}) << std::endl;
			return 1;
		}
		if (_infiles.size() > 1) {
			infile = "";
			infiles = _infiles;
		} else {
			infile = _infiles.at(0);
		}
	} else {
		std::cout << "ERROR: no input file provided!" << std::endl
			<< opt.help({"", "Group"}) << std::endl;
		return 1;
	}

	if (!memdump) {
		if (!infile.size()) {
			return multiDecompile();
		} else {
			// there's only one infile, use a regular decompiler

			// Configure the decompiler
			dmode = getDisasmMode(infile);
			DisasmConfig conf(debugging, extra, dmode);
			std::vector<char>* input = new std::vector<char>();
			if (!readFile(input, infile)) {
				std::cout << "ERROR: failed to read the input file!" << std::endl;
				return 1;
			}

			// Now that everything is parsed, initialize the decompiler
			Decompiler decompiler(conf, input);
			return decompile(&decompiler);
		}
	} else {
		return performMemdump();
	}

	return 0;
}