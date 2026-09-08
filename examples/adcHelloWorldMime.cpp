/* Copyright 2026 NTESS. See the top-level LICENSE.txt file for details.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "adc/factory.hpp"
#include <cstring>
#include <cerrno>
#include <limits>
#include <cairo/cairo.h>
#include <openssl/evp.h>
#include <fontconfig/fontconfig.h>

/*! \file adcHelloWorldMime.cpp
 * This demonstrates using the adc::factory API to build and publish a mime message.
 * The message sent includes the bare minimum, plus hello world as text and png.
 * This exammple captures the case where control of publication methods is deferred to
 * the application environment variables seen at runtime.
 */

/** \addtogroup examples
 *  @{
 */

namespace adc_examples {
namespace adcHelloWorldMime {

// Custom Cairo callback to write image bytes directly into a memory stream
static cairo_status_t write_to_stream(void *closure, const unsigned char *data, unsigned int length) {
	FILE *stream = (FILE *)closure;
	size_t written = fwrite(data, 1, length, stream);
	return (written == length) ? CAIRO_STATUS_SUCCESS : CAIRO_STATUS_WRITE_ERROR;
}

/* science codes would generate their own imagery.
 * @return array for delete[] after use. */
static char *get_png_b64() {
	char *png = NULL;
	size_t size = 0;
	FILE *mem_stream = open_memstream(&png, &size);
	if (!mem_stream) {
		perror("Failed to open memory stream");
		return NULL;
	}

	cairo_font_face_t *font = cairo_toy_font_face_create("Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 100, 100);
	cairo_t *cr = cairo_create(surface);

	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); 
	cairo_paint(cr);

	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_set_font_face(cr, font);
	cairo_set_font_size(cr, 12.0);
	
	cairo_move_to(cr, 10.0, 10.0);
	cairo_show_text(cr, "Hello World");

	cairo_surface_write_to_png_stream(surface, write_to_stream, mem_stream);
	cairo_destroy(cr);
	cairo_surface_destroy(surface);
	cairo_font_face_destroy(font);

	fclose(mem_stream);

	// Calculate required buffer size: 4 * ceil(n / 3) + 1 null terminator
	size_t b64_len = ((size + 2) / 3) * 4 + 1;
	char *b64_string = new char[b64_len];

	if (b64_string) {
		EVP_EncodeBlock((unsigned char *)b64_string, (unsigned char *)png, size);
		std::cout << "data:image/png;base64," << b64_string << std::endl;
	} else {
		std::cout << "error creating mime" << std::endl;
	}

	free(png);
	cairo_debug_reset_static_data();
	FcFini();
	return b64_string;
}


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
	auto img = get_png_b64();
	app_data->add_mime("pretty_picture",  "image/png", "base64",
				"hello_world.png", img);
	delete[] img;
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

} // adcHelloWorldMime
} // adc_examples

/** @}*/
/** @}*/

int main(int argc, char **argv)
{
	return adc_examples::adcHelloWorldMime::main(argc, argv);
}
