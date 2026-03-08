/* Copyright 2026 NTESS. See the top-level LICENSE.txt file for details.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "adc/factory.hpp"
#include <cstring>
#include <cerrno>
#include <limits>

/*! \file adcHelloWorldAuto.cpp
 * This demonstrates using the adc::factory API to build and publish a message.
 * The message sent includes the bare minimum, plus hello world.
 * This exammple captures the case where control of publication methods is deferred to
 * the application environment variables seen at runtime.
 */

/** \addtogroup examples
 *  @{
 */

namespace adc_examples {
namespace adcHelloWorldAuto {

/**
 * \brief adc c++ hello world without hard-coded publisher choices.
 */
int main(int /* argc */, char ** /* argv */) {
	std::cout << "adc pub version: " << adc::publisher_api_version.name << std::endl;
	std::cout << "adc builder version: " << adc::builder_api_version.name << std::endl;
	std::cout << "adc enum version: " << adc::enum_version.name << std::endl;

	// create a factory
	adc::factory f;

	// create a message and add header
	std::shared_ptr< adc::builder_api > b = f.get_builder();
	b->add_header_section("cxx_demo_1");

	// add an application-defined payload to the message
	auto app_data = f.get_builder();
	app_data->add("hello", "world");
	b->add_app_data_section(app_data);

	// could add lots of other sections, as needed.

	std::cout << "Available publishers are: ";
	for (auto n : f.get_publisher_names()) {
		std::cout << " " << n;
	}
	std::cout << std::endl;

	// create publishers following runtime environment variables and defaults
        auto mp = f.get_multi_publisher_from_env("");

	// send built message b to all publishers found as default configured
	int err = mp->publish(b);
	if (err) {
		std::cout << "got " << err << " publication errors." << std::endl;
	}

	// clean up all publishers
	mp->terminate();

	return 0;
}

} // adcHelloWorldAuto
} // adc_examples

/** @}*/
/** @}*/

int main(int argc, char **argv)
{
	return adc_examples::adcHelloWorldAuto::main(argc, argv);
}
