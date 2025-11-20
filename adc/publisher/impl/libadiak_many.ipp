/* Copyright 2025 NTESS. See the top-level LICENSE.txt file for details.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <adc/builder/builder.hpp>
#include <adc/publisher/publisher.hpp>
#include <cstdlib>
#include "adiak.hpp"
#if 0
#include <iostream>
#include <fstream>
#include <filesystem>
#endif

namespace adc {

scalar_type adiak_type_to_adc_type(adiak_type_t at)
{
	scalar_type result;
	// fixme ; implement adc::adiak_type_to_adc_type(adiak_type_t)
	return result;
}

// see also
// Adiak/tools/adc-single

/*! \brief Terminal output publisher_api implementation.
  This plugin sends messages to libadiak synchronously.
  Multiple independent instances of this plugin may be used simultaneously,
  but message integrity depends on the behavior of libadiak_many.
  As of 6/2025, adiak objects assume the caller is single-threaded.
 */
class libadiak_many_plugin : public publisher_api {
	enum state {
		ok,
		err
	};

private:
	const std::string vers;
	const std::vector<std::string> tags;
	enum state state;
	bool paused;
	std::string separator;

	inline static const char *adc_libadiak_many_plugin_separator_default = "/";
	const std::map< const string, const string > plugin_config_defaults =
	{
		{"SEPARATOR", adc_libadiak_many_plugin_separator_default}
	};
	inline static const char *plugin_prefix = "ADC_ADIAK_MANY_PLUGIN_";

	void write_adiak(std::string_view sv, const field& f)
	{
		// is there an adiak context we should have created here?
		// f.kt (none, section, value), f.st f.vp f.count
		// fixme: pending adiak api changes; libadiak_many_plugin::write_adiak
		// something like adiak::value(av,
			// adiak_type_to_adc_type ( f.st), 
			// f.vp, f.count);
	}

	void walk_builder(std::string name, std::shared_ptr<builder_api> b)
	{
		// walk fields
		std::vector<std::string> fnames = b->get_field_names();
		for (auto& n : fnames) {
			field f = b->get_value(n);
			std::string prefix = name + "/" + n;
			write_adiak(prefix, f);
		}
		// walk sections
		std::vector<std::string> names = b->get_section_names();
		for (auto& n : names) {
			std::string prefix = name + "/" + n;
			walk_builder(prefix, b->get_section(n));
		}
	}

	int config(std::string sep)
	{
		separator = sep;
		return 0;
	}

	// find field in m, then with prefix in env, then the default.
	const string get(const std::map< string, string >& m,
			string fieldname, string_view env_prefix) {
		// fields not defined in config_defaults raise an exception.
		auto it = m.find(fieldname);
		if (it != m.end()) {
			return it->second;
		}
		string en = string(env_prefix) += fieldname;
		char *ec = getenv(en.c_str());
		if (!ec) {
			return plugin_config_defaults.at(fieldname);
		} else {
			return string(ec);
		}
	}

public:
	libadiak_many_plugin() : vers("1.0.0") , tags({"none"}), state(ok), paused(false) {
		std::cout << "Constructing libadiak_many_plugin" << std::endl;
		separator = adc_libadiak_many_plugin_separator_default;
	}

	int publish(std::shared_ptr<builder_api> b) {
		if (paused)
			return 0;
		if (state != ok)
			return 1;
		walk_builder("adc", b);
		return 0;
	}

	int config(const std::map< std::string, std::string >& m) {
		string prog = get(m, "SEPARATOR", plugin_prefix);
		return  0;
	}

	int config(const std::map< std::string, std::string >& m, std::string_view env_prefix) {
		string prog = get(m, "SEPARATOR", env_prefix);
		return 0;
	}

	const std::map< const std::string, const std::string> & get_option_defaults() {
		return plugin_config_defaults;
	}

	int initialize() {
		return 0;
	}

	void finalize() {
	}

	void pause() {
		paused = true;
	}

	void resume() {
		paused = false;
	}

	std::string_view name() const {
		return "libadiak_many";
	}

	std::string_view version() const {
		return vers;
	}

	~libadiak_many_plugin() {
		std::cout << "Destructing libadiak_many_plugin" << std::endl;
	}
};


} // adc
