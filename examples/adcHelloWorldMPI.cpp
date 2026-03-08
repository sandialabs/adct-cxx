/* Copyright 2026 NTESS. See the top-level LICENSE.txt file for details.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "adc/adc.hpp"
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <limits>
#include <mpi.h>

/*! \file adcHelloWorldMPI.cpp
 * This demonstrates using the adc::factory API to build and publish a message from
 * all ranks of an mpi program.
 * The message sent includes the bare minimum, plus hello world.
 */

/** \addtogroup examples
 *  @{
 */

namespace adc_examples {
namespace adcHelloWorldMPI {

/**
 * \brief adc c++ hello world without hard-coded publisher choices.
 */
int main(int argc, char ** argv) {



	MPI_Init(&argc, &argv);
	MPI_Comm comm = MPI_COMM_WORLD;

	// create a factory
	adc::factory f;

	// create a message and add header
	std::shared_ptr< adc::builder_api > b = f.get_builder();
	b->add_header_section("cxx_demo_1");

	// add an application-defined payload to the message
	auto app_data = f.get_builder();
	app_data->add("hello", "world");
	b->add_app_data_section(app_data);

	// add environment chunks of interest on at least the first message in parallel production
	b->add_host_section(ADC_HS_ALL);
	b->add_slurm_section(); // or add_flux_section
	b->add_mpi_section("world", &comm, ADC_MPI_LOCAL);

	// could add lots of other sections, as needed.


	// export rank from mpi to ADC_MULTIFILE_PLUGIN_RANK ?
	// or create multipublisher manually?
	int rank;
	(void)MPI_Comm_rank(comm, &rank);
	if (!rank) {
		std::cout << "adc pub version: " << adc::publisher_api_version.name << std::endl;
		std::cout << "adc builder version: " << adc::builder_api_version.name << std::endl;
		std::cout << "adc enum version: " << adc::enum_version.name << std::endl;
	}

	std::string rank_str = std::to_string(rank);
	setenv("ADC_MULTIFILE_PLUGIN_RANK", rank_str.c_str(), 1);

	// create publishers following runtime environment variables and defaults
        auto mp = f.get_multi_publisher_from_env("");

	// send built message b to all publishers 
	int err = mp->publish(b);
	if (err) {
		std::cout << "got " << err << " publication errors." << std::endl;
	}

	// do some work
	
	// send the final status; updating the header to get new timestamp and uuid
	b->add_exit_data_section(0, "all good", nullptr);
	b->add_header_section("cxx_demo_1");
	err = mp->publish(b);
	if (err) {
		std::cout << "got " << err << " publication errors." << std::endl;
	}

	// clean up all publishers
	mp->terminate();
	
	// consolidate files; this could be done with a separate utility script
	// if you don't like how the provided function does it.
	MPI_Barrier(comm);
	if (rank == 0) {
		// may need to sleep here to give local fs a chance to catch up
		// dir/user/[wfid.].host.Ppid.Tstarttime.pptr/application.Rrank.XXXXXX
		// -->
		// dir/user/consolidated.[wfid].adct-json.multi.xml
		auto path = std::getenv("ADC_MULTIFILE_PLUGIN_DIRECTORY");
		auto wfid = std::getenv("ADC_WFID");
		auto pattern = adc::get_multifile_log_path(path, wfid);

		std::vector< std::string > old_paths;
		auto new_files = adc::consolidate_multifile_logs(pattern.c_str(), old_paths);
		if (old_paths.size()) {
			for (auto i : old_paths) {
				std::cout << "consolidating from:" << i << std::endl;
				if (false) {
					std::remove(i.c_str()); // we could delete the merged files.
				}
			}
			for (auto i : new_files) {
				std::cout << "consolidated to:" << i << std::endl;
			}
		} else {
			std::cout << "no consolidation done." << std::endl;
		}
	}

	MPI_Finalize();

	return 0;
}

} // adcHelloWorldMPI
} // adc_examples

/** @}*/
/** @}*/

int main(int argc, char **argv)
{
	return adc_examples::adcHelloWorldMPI::main(argc, argv);
}
