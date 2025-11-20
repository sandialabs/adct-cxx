/* Copyright 2025 NTESS. See the top-level LICENSE.txt file for details.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef adc_builder_impl_hpp
#define adc_builder_impl_hpp
#include <string>
#include <iostream>
#include <sstream>
#include <map>
#include <memory>
#include <boost/dll/import.hpp>
#include <boost/algorithm/string.hpp>
#include "boost/json.hpp"
#include "adc/types.hpp"
#include <sys/time.h>

namespace adc {

std::string format_timespec_8601(struct timespec& ts);
std::string format_timespec_utc_ns(struct timespec& ts);

/// \return true if uuid is in json array.
bool array_contains_string( boost::json::array& av, string_view uuid);

/// \return values in s when split at delimiter.
std::vector<std::string> split_string(const std::string& s, char delimiter);


inline version builder_version("1.0.0", {"none"});

/*! \brief Implementation of builder_api with optional (compile-time)
 support of MPI. If compiled without MPI, the mpi-related calls devolve
 to serial behavior.
 */
class BOOST_SYMBOL_VISIBLE builder : public builder_api {
public:
	builder(void *mpi_communicator_p=NULL);

	// copy populated generic section into the builder under specified name.
	void add_section(std::string_view name, std::shared_ptr< builder_api > section);

	// auto-populate the header section with application name
	void add_header_section(std::string_view application_name);

	// auto-populate the host section
	void add_host_section(int32_t subsections);

	// populate application run-time data to app_data section.
	// any relationship to previous jobs/higher level workflows goes in app_data
	// somehow.
	void add_app_data_section(std::shared_ptr< builder_api > app_data);

	// populate application run-time physics (re)configuration/result to model_data section.
	// e.g. changes in mesh/particle decomp go here.
	void add_model_data_section(std::shared_ptr< builder_api > model_data);

	// auto-populate code section with os-derived info at time of call,
	// tag, version, and code_details blob.
	void add_code_section(std::string tag, std::shared_ptr< builder_api > version, std::shared_ptr< builder_api > code_details);

	// populate build/install configuration information like options enabled
	void add_code_configuration_section(std::shared_ptr< builder_api > build_details);

	// populate exit_data section
	void add_exit_data_section(int return_code, std::string status, std::shared_ptr< builder_api > status_details);

	void add_memory_usage_section();

	/// \brief add data about a named mpi communicator.
	/// In most applications, "mpi_comm_world" is the recommended name.
	/// Applications with multiple communicators for data separation can make
	/// multiple calls to add_mpi_section with distinct names, such as "comm_ocean" or "comm_atmosphere".
	void add_mpi_section(std::string_view name, void *mpi_comm_p, adc_mpi_field_flags bitflags);

	void add_workflow_section();
	void add_workflow_children(std::vector< std::string >& child_uuids);

	void add_slurm_section();
	void add_slurm_section(const std::vector< std::string >& slurmvars);

	void add_gitlab_ci_section();

	// return the section, or an empty pointer if it doesn't exist.
	std::shared_ptr< builder_api > get_section(std::string_view name);
	std::vector< std::string > get_section_names();
	std::vector< std::string > get_field_names();
	const field get_value(std::string_view name);

	void add(std::string_view name, bool value);

	void add(std::string_view name, char value);

	void add(std::string_view name, char16_t value);

	void add(std::string_view name, char32_t value);

	// add null-terminated string
	void add(std::string_view name, char* value);
	void add(std::string_view name, const char* value);
	void add(std::string_view name, std::string& value);
	void add(std::string_view name, std::string_view value);

	// add null-terminated string filepath
	void add_path(std::string_view name, char* value);
	void add_path(std::string_view name, const char* value);
	void add_path(std::string_view name, std::string& value);
	void add_path(std::string_view name, std::string_view value);

	// add string which is serialized json.
	void add_json_string(std::string_view name, std::string_view value);
	// add string which is yaml
	void add_yaml_string(std::string_view name, std::string_view value);
	// add string which is xml
	void add_xml_string(std::string_view name, std::string_view value);
	// add string which is an arbitrary precision number
	void add_number_string(std::string_view name, std::string_view value);
#if ADC_BOOST_JSON_PUBLIC
	// add a named raw json value (array, obj, or value)
	void add(std::string_view name, boost::json::value value);
#endif

	void add(std::string_view name, uint8_t value);
	void add(std::string_view name, uint16_t value);
	void add(std::string_view name, uint32_t value);
	void add(std::string_view name, uint64_t value);

	void add(std::string_view name, int8_t value);
	void add(std::string_view name, int16_t value);
	void add(std::string_view name, int32_t value);
	void add(std::string_view name, int64_t value);

	void add(std::string_view name, float value);
	void add(std::string_view name, const std::complex<float>& value);
	void add(std::string_view name, double value);
	void add(std::string_view name, const std::complex<double>& value);

	/// add timeval
	void add(std::string_view name, const struct timeval& tv);
	/// add timespec
	void add(std::string_view name, const struct timespec& ts);
	/// add unix epoch seconds (gettimeofday)
	void add_epoch(std::string_view name, int64_t epoch);

	/// add data from a c pointer
	void add_from_pointer_type(std::string_view name, void*, enum scalar_type t);

	/// Fixed length arrays of scalar members.
	void add_array(std::string_view name, bool value[], size_t len, std::string_view c);

	// this is how a character buffer containing nuls is stored.
	void add_array(std::string_view name, const char *value, size_t len, std::string_view c);
	void add_array(std::string_view name, char16_t value[], size_t len, std::string_view c);
	void add_array(std::string_view name, char32_t value[], size_t len, std::string_view c);

	void add_array(std::string_view name, uint8_t value[], size_t len, std::string_view c);
	void add_array(std::string_view name, uint16_t value[], size_t len, std::string_view c);
	void add_array(std::string_view name, uint32_t value[], size_t len, std::string_view c);
	void add_array(std::string_view name, uint64_t value[], size_t len, std::string_view c);

	void add_array(std::string_view name, int8_t value[], size_t len, std::string_view c);
	void add_array(std::string_view name, int16_t value[], size_t len, std::string_view c);
	void add_array(std::string_view name, int32_t value[], size_t len, std::string_view c);
	void add_array(std::string_view name, int64_t value[], size_t len, std::string_view c);

	void add_array(std::string_view name, float value[], size_t len, std::string_view c);
#if 0
	void add_array(std::string_view name, const std::complex<float> value[], size_t len);
#endif
	void add_array(std::string_view name, double value[], size_t len, std::string_view c);
#if 0
	void add_array(std::string_view name, const std::complex<double> value[], size_t len);
#endif
	/// Irregular element size array members.
	// Arrays of null-terminated strings
	void add_array(std::string_view name, char* value[], size_t len, std::string_view c);
	void add_array(std::string_view name, const char* value[], size_t len, std::string_view c);
	void add_array(std::string_view name, std::string value[], size_t len, std::string_view c);
	void add_array(std::string_view name, const std::string value[], size_t len, std::string_view c);
	void add_array(std::string_view name, const std::vector<std::string> sv, std::string_view c);
	void add_array(std::string_view name, const std::set<std::string> sv, std::string_view  container);
	void add_array(std::string_view name, const std::list<std::string> sv, std::string_view  container);

	// Array of strings which are serialized json.
	void add_array_json_string(std::string_view name, const std::string value[], size_t len, std::string_view c);

	std::string serialize();

private:
	// this is private because it must be of a specific structure, not arbitrary json
	boost::json::object d;

	key_type kind(std::string_view name);

private:
	std::map< std::string, std::shared_ptr< builder > > sections;
	// merge sections recursively into a pure json object
	boost::json::object flatten();
	void *mpi_comm_p;

	/// \return names of env vars listed in colon-separated env(ADC_HOST_SECTION_ENV)
	std::vector<std::string> get_host_env_vars();

}; // class builder



} // namespace adc
#endif // adc_builder_impl_hpp
