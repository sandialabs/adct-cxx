/* Copyright 2025 NTESS. See the top-level LICENSE.txt file for details.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <vector>
#include <array>
#include <iostream>
#include <sstream>
#include <ostream>
#include <string>
#include <cstdio>

namespace adc {

struct pipe_data {
	std::string output;
	int rc;
	friend std::ostream &operator<<(std::ostream &os, const pipe_data &pdata) {
		os << "proc rc: " << pdata.rc << " output: " << pdata.output;
		return os;
	}
	bool operator==(const pipe_data &rhs) const {
		return output == rhs.output &&
			   rc == rhs.rc;
	}
	bool operator!=(const pipe_data &rhs) const {
		return !(rhs == *this);
	}
};

class out_pipe {
public:
	static pipe_data run(const std::string &proc) {
		int rc = 0;
		std::array<char, 8192> scratch {};
		std::string text;
		FILE *pipe = popen(proc.c_str(), "r");
		if (pipe == nullptr) {
			throw std::runtime_error("popen() fails: " + proc);
		}
		try {
			std::size_t inbytes;
			while ((inbytes = std::fread(scratch.data(), sizeof(scratch.at(0)), sizeof(scratch), pipe)) != 0) {
				text += std::string(scratch.data(), inbytes);
			}
		} catch (...) {
			pclose(pipe);
			throw;
		}
		int res = pclose(pipe);
		rc = WEXITSTATUS(res);
		return pipe_data{text, rc};
	}

};

} // end adc

#ifdef MAIN_out_pipe
int main() {
	pipe_data lscpu = out_pipe::run("lscpu -J");
	std::cout << lscpu.rc << std::endl;
	std::cout << lscpu.output << std::endl;
	return 0;
}
#endif
#ifdef MAIN_numactl
#if 0
available: 8 nodes (0-7)
node 0 cpus: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 112 113 114 115 116 117 118 119 120 121 122 123 124 125
node 0 size: 31699 MB
node 0 free: 8914 MB
node distances:
#endif

int main() {
        static bool shell_done;
	int numa_json = -1;
        if (!shell_done) {
                size_t numa_node_count = 0;
		std::vector<std::string> sizes;
		std::vector<std::string> cpulist;
                shell_done = 1;
                pipe_data numactl = out_pipe::run("numactl -H");
                if (! numactl.rc) {
			std::string line;
			std::istringstream nss(numactl.output);
			while (std::getline(nss, line)) {
				size_t cp = line.find(':');
				std::string name = line.substr(0, cp);
				if (name == "available") {
					std::istringstream iss(line.substr(cp+1));
					iss >> numa_node_count;
				}
				if (name.substr(name.length()-4) == "cpus" ) {
					cpulist.push_back(line.substr(cp+2));
				}
				if (name.substr(name.length()-4) == "size" ) {
					sizes.push_back(line.substr(cp+2));
				}
				if (name.substr(name.length()-4) == "nces" )
					break; // stop on "node distances:"
			}
			if (sizes.size() != numa_node_count || cpulist.size() != numa_node_count)
				return numa_json; // inconsistent data
			std::cout << numa_node_count << std::endl;
			for (size_t i = 0; i < numa_node_count; i++) {
				std::cout << sizes[i] <<std::endl;
				std::cout << cpulist[i] <<std::endl;
			}
                }
        }
        return numa_json;
}
#endif
