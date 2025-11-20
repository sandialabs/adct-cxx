/* Copyright 2025 NTESS. See the top-level LICENSE.txt file for details.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
//
// This demo shows how one might send messages from a parallel program.
// Via ldms only.
//
#if ADC_BOOST_JSON_PUBLIC
#include "boost/json/src.hpp"
#endif
#include "adc/factory.hpp"
#include <cstring>
#include <cerrno>
#include <limits>
#ifdef ADC_HAVE_MPI
#include <mpi.h>
#endif

/*! \file mpiDemoBuilder.cpp
 */


/** \addtogroup examples
 *  @{
 */
/** \addtogroup mpi
 *  @{
 */
namespace adc_examples {
namespace mpiDemoBuider {

std::map<std::string, std::string> file_config =
	{{ "DIRECTORY", "./test.outputs"}, 
	 { "FILE", "out.file.mpi.log" },
	 { "APPEND", "true" }
	};

std::map<std::string, std::string> ldmsd_stream_publish_config =
	{
		{ "STREAM", "adc_publish_api" }
	};

int test_publisher(std::shared_ptr<adc::publisher_api> pi, std::shared_ptr< adc::builder_api> b, std::map<std::string, std::string>& pconfig ) {
	std::cout << "------------------- begin --------------------" << std::endl;
	int err = 0;
	int e = 0;
	err = pi->config(pconfig);
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
	err = pi->publish(b); // there should be 1 b in the output
	if (err) {
		std::cout << "publish 1 failed " <<
			std::strerror(err) << std::endl;
		e += 1;
		err = 0;
	}
	pi->pause();
	err = pi->publish(b); // this publication should be silent in output
	if (err) {
		std::cout << "publish 2 failed " <<
			std::strerror(err) << std::endl;
		e += 1;
		err = 0;
	}
	pi->resume();
	err = pi->publish(b); // there should be 2 b in the output
	if (err) {
		std::cout << "publish 3 failed " <<
			std::strerror(err) << std::endl;
		e += 1;
		err = 0;
	}
	pi->finalize();
	std::cout << "------------------- end --------------------" << std::endl;
	return e;
}


void populate_builder(std::shared_ptr<adc::builder_api> b,  adc::factory & f) {

	std::shared_ptr< adc::builder_api > app_data = f.get_builder();
#if ADC_BOOST_JSON_PUBLIC
	boost::json::object jo = {{"a","b"},{"C","d"},{"n",1}};
#endif
#ifdef ADC_HAVE_MPI
	MPI_Comm comm = MPI_COMM_WORLD;
	void *commptr = &comm;
#else
	void *commptr = NULL;
#endif
	b->add_mpi_section("mpi_comm_world", commptr, ADC_MPI_RANK | ADC_MPI_SIZE);

	std::vector<std::string> children = { "uuid1", "uuid2", "uuid3"};
	b->add_workflow_section();
	b->add_workflow_children(children);

	app_data->add("bool0", false);
	app_data->add("bool1", true);
	app_data->add("char1", 'A');
	app_data->add("c16", u'¢');
	// there are intentionally non-ascii characters in the next line.
	app_data->add("c32", U'猫');
	const std::string ccppstr("ccppstr");
	app_data->add("ccppstr", ccppstr);
	std::string cppstr("cppstr");
	app_data->add("cppstr", cppstr);
	// add null-terminated string
	const char *cstr = "cstr_nul";
	app_data->add("cstr1", cstr);
	// add string which is serialized json.
	app_data->add_json_string("jstr1", std::string ("{\"a\":\"b\", \"c\":[1,2, 3]}"));
#if ADC_BOOST_JSON_PUBLIC
	app_data->add("jsonobj", jo);
#endif
	uint8_t u8 = std::numeric_limits<uint8_t>::max();
	uint16_t u16 = std::numeric_limits<uint16_t>::max();
	uint32_t u32 = std::numeric_limits<uint32_t>::max();
	uint64_t u64 = std::numeric_limits<uint64_t>::max();
	app_data->add("u8", u8);
	app_data->add("u16", u16);
	app_data->add("u32", u32);
	app_data->add("u64", u64);
	int8_t i8 = std::numeric_limits<int8_t>::max();
	int16_t i16 = std::numeric_limits<int16_t>::max();
	int32_t i32 = std::numeric_limits<int32_t>::max();
	int64_t i64 = std::numeric_limits<int64_t>::max();
	app_data->add("i8", i8);
	app_data->add("i16", i16);
	app_data->add("i32", i32);
	app_data->add("i64", i64);
	app_data->add("flt", std::numeric_limits<float>::max());
	app_data->add("dbl", std::numeric_limits<double>::max() );
	app_data->add("fcplx", std::complex<float>(
		std::numeric_limits<float>::max(),std::numeric_limits<float>::max()));
	app_data->add("dcplx", std::complex<double>(
		std::numeric_limits<double>::max(),std::numeric_limits<double>::max()));
	const char *cstrings[] = {"a", "B", "c2"};
	char *vcstrings[4];
	int ia[4];
	float fa[4];
	double da[4];
	unsigned ua[4];
	uint64_t ua64[4];
	for (int i = 0; i < 4; i++) {
		vcstrings[i] = new char[2];
		snprintf(vcstrings[i], 2, "%d", i);
		ua[i] = i;
		ua64[i] = std::numeric_limits<uint64_t>::max() - i;
		ia[i] = -i;
		da[i] = 3.14*i;
		fa[i] = 3.14*i *2;
	}
	std::string cppstrings[] = {"ap", "Bp", "c2p"};
	app_data->add_array("nulembed", "a\0b", 3);
	app_data->add_array("cstrs", cstrings, 3);
	app_data->add_array("cppstrs", cppstrings, 3);
	app_data->add_array("vcstrs", vcstrings, 4);
	for (int i = 0; i < 4; i++) {
		delete [] vcstrings[i];
	}
	app_data->add_array("ia", ia, 4);
	app_data->add_array("ua", ua, 4);
	app_data->add_array("ua64", ua64, 4);
	app_data->add_array("fa", fa, 4);
	app_data->add_array("da", da, 4);
	
	b->add_header_section("cxx_demo_1");

	b->add_host_section(ADC_HS_ALL);

	b->add_app_data_section(app_data);

	b->add_memory_usage_section();

	std::shared_ptr< adc::builder_api > code_details = f.get_builder();
	// more here

	std::shared_ptr< adc::builder_api > version = f.get_builder();
	version->add("version", "1.1.3");
	const char* tags[] = { "boca_raton", "saronida_2", "mpi"};
	version->add_array("tags", tags ,2);
	version->add("mesh_version", "5.0.0");

	b->add_code_section("repartitioner", version, code_details);

	std::shared_ptr< adc::builder_api > build_details = f.get_builder();
	b->add_code_configuration_section(build_details);

	std::shared_ptr< adc::builder_api > model_data = f.get_builder();
	model_data->add("nx", 3);
	model_data->add("ny", 10);
	model_data->add("step", 0);
	b->add_model_data_section(model_data);

	std::shared_ptr< adc::builder_api > status_details = f.get_builder();
	status_details->add("tmax", 15000.25);
	status_details->add("tmax_loc", 10325);
	status_details->add("step", 234);
	b->add_exit_data_section(1,"we didn't succeed due to high temperatures", status_details);

#if 0
	std::string ss = b->serialize();
	std::cout << "-------------------------------" << std::endl;
	std::cout << ss << std::endl;
	std::cout << "-------------------------------" << std::endl;
#endif
}

/*! \brief test mpi, all data types, and several publishers
 */
#ifdef ADC_HAVE_MPI
int main(int argc, char **argv)
{
	MPI_Init(&argc, &argv);
#else
int main(int , char **)
{
#endif
	std::cout << "adc pub version: " << adc::publisher_api_version.name << std::endl;
	std::cout << "adc builder version: " << adc::builder_api_version.name << std::endl;
	std::cout << "adc enum version: " << adc::enum_version.name << std::endl;

	adc::factory f;
	std::shared_ptr< adc::builder_api > b = f.get_builder();

	populate_builder(b, f);

	// get publisher from the factory class.
	std::shared_ptr< adc::publisher_api > p1 = f.get_publisher("file");
	std::cout << test_publisher(p1, b, file_config) << std::endl;

	std::shared_ptr< adc::publisher_api > p2 = f.get_publisher("stdout");
	std::cout << test_publisher(p2, b, file_config) << std::endl;

	std::shared_ptr< adc::publisher_api > p4 = f.get_publisher("ldmsd_stream_publish");
	std::cout << test_publisher(p4, b, ldmsd_stream_publish_config) << std::endl;

#ifdef ADC_HAVE_MPI
	MPI_Finalize();
#endif
	return 0;
}


} // end mpiDemoBuider
} // end adc_examples

/** @} mpi*/
/** @} examples*/

int main(int argc, char **argv)
{
	return adc_examples::mpiDemoBuider::main(argc, argv);
}

