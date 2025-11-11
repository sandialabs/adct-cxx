//
// This demo shows how one might send messages from a parallel program.
// Via ldms only.
//
#ifdef ADC_BOOST_JSON_PUBLIC
#include "boost/json/src.hpp"
#endif
#include "adc/factory.hpp"
#include <cerrno>
#include <cstring>
#include <sstream>
#include <limits>
#include <unistd.h>
#ifdef ADC_HAVE_MPI
#include <mpi.h>
#endif

#define DEBUG 1

std::map<std::string, std::string> file_config =
	{{ "DIRECTORY", "./test.outputs"}, 
	 { "FILE", "out.file.log" },
	 { "APPEND", "true" }
	};

std::map<std::string, std::string> null_config;

std::map<std::string, std::string> ldmsd_stream_publish_config =
	{
		{ "STREAM", "adc_publish_api" }
	};


int start_publisher(std::shared_ptr<adc::publisher_api> pi, std::map<std::string, std::string>& pconfig ) {
	int e = 0;
	int err = pi->config(pconfig);
	if (err) {
		std::cout << "config failed " <<
			std::strerror(err) << std::endl;
		e += 1;
		err = 0;
	}
	err = pi->initialize();
	if (err) {
		std::cout << "initialize failed " <<
			std::strerror(err) << std::endl;
		e += 1;
		err = 0;
	}
	return e;
}

void populate_start(std::shared_ptr<adc::builder_api> b,  adc::factory & f) {

	// define the 'application' tag
	b->add_header_section("cxx_demo_1");

	// report lots of hardware context information
	b->add_host_section(ADC_HS_ALL);

	// report host memory at start
	b->add_memory_usage_section();

	// create a customized section for application specific data
	// in this case, mpi is reported.
	std::shared_ptr< adc::builder_api > app_data = f.get_builder();
	//
	// include customized section in parent data object 'b'
	app_data->add("foo","bar");
	b->add_app_data_section(app_data);

	// likewise, create a tailored 'version' section
	// include mpi/ompi version details
	std::shared_ptr< adc::builder_api > version = f.get_builder();
	version->add("version", "1.1.3");
	const char* tags[] = { "boca_raton", "saronida_2"};
	version->add_array("tags", tags, 2);
	version->add("mesh_version", "5.0.0");

	std::vector<std::string> children = { "uuid1", "uuid2", "uuid3"};
	b->add_workflow_section();
	b->add_workflow_children(children);

	void *commptr = NULL;
#ifdef ADC_HAVE_MPI
	MPI_Comm comm = MPI_COMM_WORLD;
	commptr = &comm;
#endif
	// b->add_mpi_section("world", commptr, ADC_MPI_LOCAL); // use this version if only calling on rank 0
	b->add_mpi_section("world", commptr, ADC_MPI_ALL); // use this version if calling on all ranks

	// likewise create a section about model parameters.
	// might including method, material names, mesh/particle domain names/sizes, etc
	// in hierarchical object summaries
	std::shared_ptr< adc::builder_api > model_data = f.get_builder();
	model_data->add("nx", 3);
	model_data->add("ny", 10);
	model_data->add("step", 0);
	b->add_model_data_section(model_data);

}

void populate_progress(std::shared_ptr<adc::builder_api> b,  adc::factory & f) {

	b->add_header_section("cxx_demo_1");
	// include just the hostname for matching the start event
	b->add_host_section(ADC_HS_BASE);

	// add app_data with some misc data
	std::shared_ptr< adc::builder_api > app_data = f.get_builder();

	void *commptr = NULL;
#ifdef ADC_HAVE_MPI
	MPI_Comm comm = MPI_COMM_WORLD;
	commptr = &comm;
#endif
	b->add_mpi_section("world", commptr, ADC_MPI_RANK);

	float x[3] = { 3,2,1 };
	app_data->add_array("direction", x, 3);
	app_data->add("foo", 12);
	app_data->add("bar", 2.5);
	app_data->add_array("direction_set", x, 3, "set");
	b->add_app_data_section(app_data);

	// add the progress status
	std::shared_ptr< adc::builder_api > progress_details = f.get_builder();
	progress_details->add("step", 100);
	b->add_model_data_section(progress_details);

}

void populate_end(std::shared_ptr<adc::builder_api> b,  adc::factory & f) {

	b->add_header_section("cxx_demo_1");
	// include just the hostname for matching the start event
	b->add_host_section(ADC_HS_BASE);
	b->add_memory_usage_section();

	// add app_data with the mpi rank for matching
	std::shared_ptr< adc::builder_api > app_data = f.get_builder();
	void *commptr = NULL;
#ifdef ADC_HAVE_MPI
	MPI_Comm comm = MPI_COMM_WORLD;
	commptr = &comm;
#endif
	app_data->add_mpi_section("world", commptr, ADC_MPI_RANK | ADC_MPI_SIZE);
	app_data->add_gitlab_ci_section();
	b->add_app_data_section(app_data);

	// add the exit status
	std::shared_ptr< adc::builder_api > exit_details = f.get_builder();
	exit_details->add("tmax", 15000.25);
	exit_details->add("tmax_loc", 10325);
	exit_details->add("step", 234);
	b->add_exit_data_section(1, "we didn't succeed due to high temperatures", exit_details);

}

#ifdef ADC_HAVE_MPI
int main(int argc, char **argv)
{
	MPI_Init(&argc, &argv);
#else
int main(int , char **)
{
#endif

	// do something, probably parse input, load mesh, etc

	adc::factory f;
#if DEBUG
	std::shared_ptr< adc::publisher_api > p = f.get_publisher("stdout");
	int pub_err = start_publisher(p, null_config);
#else
	std::shared_ptr< adc::publisher_api > p = f.get_publisher("ldmsd_stream_publish");
	int pub_err = start_publisher(p, ldmsd_stream_publish_config);
#endif

	std::cout << "DUMP initial config of model, hardware, etc" << std::endl;
	if (!pub_err) {
		std::shared_ptr< adc::builder_api > b_init = f.get_builder();
		populate_start(b_init, f);
		int msg_err = p->publish(b_init);
		if (msg_err) {
			std::cout << "publish initial model info failed " <<
				std::strerror(msg_err) << std::endl;
		}
	}

	sleep(2);
	// do something, probably a main loop
	// ...
	std::cout << "DUMP some progress" << std::endl;
	// can at some frequency create and publish a progress report
	// in machine readable form.
	if (!pub_err) {
		std::shared_ptr< adc::builder_api > b_progress = f.get_builder();
		populate_progress(b_progress, f);
		int msg_err = p->publish(b_progress);
		if (msg_err) {
			std::cout << "publish progress info failed " <<
				std::strerror(msg_err) << std::endl;
		}
	}
	
	sleep(2);
	std::cout << "DUMP result" << std::endl;
	// report final results
	if (!pub_err) {
		std::shared_ptr< adc::builder_api > b_final = f.get_builder();
		populate_end(b_final, f);
		int msg_err = p->publish(b_final);
		if (msg_err) {
			std::cout << "publish final status info failed " <<
				std::strerror(msg_err) << std::endl;
		}
	}

	p->finalize();

#ifdef ADC_HAVE_MPI
	MPI_Finalize();
#endif
	return 0;
}
