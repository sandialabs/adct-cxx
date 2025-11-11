#ifndef adc_builder_hpp
#define adc_builder_hpp
#include <string>
#include <iostream>
#include <sstream>
#include <map>
#include <memory>
#if ADC_BOOST_JSON_PUBLIC
#include "boost/json.hpp"
#endif
#include "adc/types.hpp"
#include <sys/time.h>

namespace adc {

/** \addtogroup API
 *  @{
 */

/// \return iso 8601 formatted string based on ts
std::string format_timespec_8601(struct timespec& ts);

/// \return utc seconds.nanoseconds formatted string based on ts
std::string format_timespec_utc_ns(struct timespec& ts);

inline version builder_api_version("1.0.0", {"none"});

/** \addtogroup builder_add_host_options
 *  @{
 */
/** \def ADC_HS_
 * \brief bit values to control content of the host section.
 *
 * The host data section has a variety of optional subsections.
 * The ADC_HS_* bit values desired are ORd (|) together.
 * The data for subsections with identifier >= 0x10 is expensive, constant,
 * and should not be collected multiple times per program run.
 *
 * Dynamic meminfo usage is collected with add_memory_usage_section
 * not the host section.
 */
#define ADC_HS_
/** OR-d ADC_HS_* bits */
typedef int32_t adc_hs_subsection_flags;

/// \brief ADC_HS_BASE collects just the hostname via gethostname()
#define ADC_HS_BASE 0x0

/// \brief ADC_HS_OS collects other items from uname().
#define ADC_HS_OS 0x1

/// \brief ADC_HS_RAMSIZE collects MemTotal
#define ADC_HS_RAMSIZE 0x2

/// \brief ADC_HS_ENV collects env vars listed in env("ADC_HOST_SECTION_ENV") which is :-separated.
/// Example:  ADC_HOST_SECTION_ENV="SNLCLUSTER:SNLNETWORK:SNLSITE:SNLSYSTEM:SNLOS"
#define ADC_HS_ENV 0x4

/// \brief ADC_HS_CPU collects details from lscpu -J  (requires lscpu installed)
#define ADC_HS_CPU 0x10

/// \brief ADC_HS_GPU collects gpu data available from lspci (requires lspci installed)
#define ADC_HS_GPU 0x20

/// \brief ADC_HS_NUMA collects numa node, cpu, and per node memory from numactl -H (requires numactl installed)
#define ADC_HS_NUMA 0x40

/// \brief all ADC_HS_* optional data included
#define ADC_HS_ALL (ADC_HS_OS|ADC_HS_RAMSIZE|ADC_HS_ENV|ADC_HS_CPU|ADC_HS_GPU|ADC_HS_NUMA)
/** @}*/

/** \addtogroup builder_add_mpi_options
 *  @{
 */
/** \def ADC_MPI_
 * \brief bit values to control output of the add_mpi function
 *
 * The mpi data has a variety of optional fields.
 * The ADC_MPI_* bit values desired are ORd (|) together.
 */
#define ADC_MPI_
/** OR-d ADC_MPI_* bits */
typedef int32_t adc_mpi_field_flags;

/// \brief include no mpi fields
#define ADC_MPI_NONE 0x0

/// \brief include "mpi_rank" field from mpi_comm_rank
#define ADC_MPI_RANK 0x1

/// \brief include "mpi_size" field from mpi_comm_size
#define ADC_MPI_SIZE 0x2

/// \brief include "mpi_name" field from mpi_comm_name
#define ADC_MPI_NAME 0x4

/// \brief include "mpi_hostlist" subsection from the communicator
///
/// If this value is included, then the call to add_mpi must be collective.
#define ADC_MPI_HOSTLIST 0x10

/// \brief include "mpi_rank_host" subsection from the communicator
///
/// If this value is included, then the call to add_mpi must be collective.
#define ADC_MPI_RANK_HOST 0x20

/// \brief include "mpi_version" field from MPI_VERSION.MPI_SUBVERSIUON
#define ADC_MPI_VER 0x100

/// \brief include mpi_get_library_version result.
///
/// If openmpi is not in use, no value is defined.
#define ADC_MPI_LIB_VER 0x200

/// \brief include all mpi options. 
/// If this value is used, then the call to add_mpi must be collective.
#define ADC_MPI_ALL (ADC_MPI_RANK|ADC_MPI_SIZE|ADC_MPI_NAME|ADC_MPI_HOSTLIST|ADC_MPI_RANK_HOST|ADC_MPI_VER|ADC_MPI_LIB_VER)

/// \brief include all mpi options that do not require collective work. 
#define ADC_MPI_LOCAL (ADC_MPI_RANK|ADC_MPI_SIZE|ADC_MPI_NAME|ADC_MPI_VER|ADC_MPI_LIB_VER)

/** @}*/

/*!
\brief The builder api is used to construct structured log (json) messages that follow naming conventions.

To be stored in the ADC storage system, every message must be created from the factory and then
initialized with at least the add_header_section function.

Primitives, arrays, or strings from existing application control formats including
xml, yaml, or json and common containers of strings are supported.
There are no functions here to parse external formats (e.g. xml, yaml, json) and store them
as adc-formatted trees; this would add undesirable dependencies (parsing libraries)
and (in some cases) introduce data typing ambiguities.
*/
class ADC_VISIBLE builder_api {
public:
	virtual ~builder_api() {};
	/// \brief  auto-populate the "header" section with application name
	/// and required local data
	virtual void add_header_section(std::string_view application_name) = 0;

	/** \brief auto-populate the "host" section based on bitflags.
	 * There are many optional subsections covering cpus, gpus, numa, OS. RAM, etc.
	 * @param bitflags the OR (|) of the desired ADC_HS_*.
	 */
	virtual void add_host_section(adc_hs_subsection_flags bitflags) = 0;

	/// \brief create the "app_data" section with data defined by the application writer.
	///
	/// Any relationship to previous jobs/higher level workflows goes in app_data,
	/// Example:
	///  std::shared_ptr< builder_api > app_data = factory.get_builder();
	///  app_data.add("saw_id", getenv("SAW_WORKFLOW_ID");
	///  builder->add_app_data_section(app_data);
	virtual void add_app_data_section(std::shared_ptr< builder_api > app_data) = 0;

	/// \brief  populate application run-time physics (re)configuration/result to "model_data" section.
	/// For example initial or changes in mesh/particle decomp go here.
	virtual void add_model_data_section(std::shared_ptr< builder_api > model_data) = 0;
	
	/// Auto-populate "code" section with os-derived info at time of call,
	/// and user-provided tag string, version subsection, and code_details subsction.
	///
	/// Example: pending.
	virtual void add_code_section(std::string tag, std::shared_ptr< builder_api > version, std::shared_ptr< builder_api > code_details) = 0;

	/// \brief Populate build/install "configuration" information such as options enabled
	///
	/// Example: pending.
	virtual void add_code_configuration_section(std::shared_ptr< builder_api > build_details) = 0;

	/// \brief populate "exit_data" section with code and status stream and user provided details.
	///
	/// Example: pending.
	virtual void add_exit_data_section(int return_code, std::string status, std::shared_ptr< builder_api > status_details) = 0;
	
	/// \brief populate "memory_usage" section with current host /proc/meminfo data 
	/// in the style of free(1).
	///
	/// values included are:
	/// mem_total mem_used mem_free mem_shared
	/// mem_buffers mem_cache mem_available
	/// swap_total swap_used swap_free
	virtual void add_memory_usage_section() = 0;

	/// \brief get the existing named section
	/// \return the section, or an empty pointer if it doesn't exist.
	virtual std::shared_ptr< builder_api > get_section(std::string_view name) = 0;

	/// \brief get the names of sections
	/// \return vector of names, or an empty vector.
	virtual std::vector< std::string > get_section_names() = 0;

	/// \brief get the existing named field in the section.
	/// \return the field description, with kt==k_none if not found.
	/// If the value has kt==k_section, call get_section instead.
	/// If the value was not set via this interface, k_none is returned
	virtual const field get_value(std::string_view field_name) = 0;

	/// \brief get the names of non-section fields in the section
	/// \return vector of names, or an empty vector.
	virtual std::vector< std::string > get_field_names() = 0;

	/// \brief populate mpi_comm_$name section with members mpi_* as indicated by bitflags
	/// \param name scoping label of the communicator, something like world, ocean, atmosphere.
	/// \param mpi_comm_p address of an MPI_Comm variable.
	/// If mpi_comm_p is NULL,
	/// then rank/size is 0/1 and all other fields are trivial.
	/// \param bitflags the or (|) of ADC_MPI_* flags desired.
	/// If hostlist is included, this call must be made collectively.
	///
	/// It is recommended to call add_mpi_section once per run on all ranks
	/// as add_mpi_section("world", &MPI_COMM_WORLD, ADC_MPI_ALL)
	/// (or again if communicators are created/resized).
	/// It the app_data object for rank or size
	/// and with the code_details object for mpi versions.
	//
	virtual void add_mpi_section(std::string_view name, void *mpi_comm_p, adc_mpi_field_flags bitflags) = 0;

	/// \brief add gitlab_ci environment variable dictionary.
	/// The section added is named "gitlab_ci".
	///
	/// The variables collected from env() are:
	/// - ci_runner_id
	/// - ci_runner_version
	/// - ci_project_id
	/// - ci_project_name
	/// - ci_server_fqdn
	/// - ci_server_version
	/// - ci_job_id
	/// - ci_job_started_at
	/// - ci_pipeline_id
	/// - ci_pipeline_source
	/// - ci_commit_sha
	/// - gitlab_user_login
	///
	/// Where the values are strings from the corresponding env() values.
	virtual void add_gitlab_ci_section() = 0;

	/// \brief add data from adc_wfid_ environment variables.
	/// The section name is "adc_workflow".
	///
	/// The env variables collected are:
	/// wfid: ADC_WFID
	/// wfid_parent: $ADC_WFID_PARENT
	/// wfid_path: $ADC_WFID_PATH
	/// 
	/// The suggested format of an adc workflow identifier (wfid) is as:
	/// 	uuid -v1 -F STR
	/// run at the appropriate scope.
	/// For example, when starting numerous processes with mpi under slurm,
	/// in the sbatch script before launching anything else do:
	/// 	export ADC_WFID=$(uuid -v1 -F STR)
	/// and then make sure it gets propagated to all the processes via the launch
	/// mechamism. This ties all the messages from mpi ranks together in adc.
	///
	/// Where a workflow parent (such as an agent launching multiple slurm jobs)
	/// can, 
	/// 	export ADC_WFID_PARENT=$(uuid -v1 -F STR)
	/// and then make sure this value is propagated to the slurm environments.
	///
	/// Where possible (requires coordination at all workflow levels)
	/// 	export ADC_WFID_PATH=(higher_level_wfid_path)/$ADC_WFID
	/// the entire task hierarchy identifier can be collected.
	virtual void add_workflow_section() = 0;

	/// \brief add list of child uuids to "adc_workflow" section
	/// after add_workflow_section has been called. This call is optional.
	/// 
	/// This call may be repeated if necessary, incrementally building the child list.
	/// wfid_children: [ user defined list of ids ]
	///
	/// Where a workflow can track its immediate children, it may substantially
	/// improve downstream workflow analyses if the child items can be captured.
	/// The result appears in the resulting json as
	/// 	wfid_children: $ADC_WFID_CHILDREN
	virtual void add_workflow_children(std::vector< std::string >& child_uuids) = 0;

	/// \brief add slurm output environment variable dictionary elements.
	/// The section added is named "slurm".
	///
	/// The variables collected from env() are:
	/// - cluster: SLURM_CLUSTER_NAME
	/// - job_id: SLURM_JOB_ID
	/// - num_nodes: SLURM_JOB_NUM_NODES
	/// - dependency: SLURM_JOB_DEPENDENCY 
	///
	/// Where the values are strings from the corresponding env() values.
	virtual void add_slurm_section() = 0;
	/// \brief add slurm output environment variable dictionary elements,
	/// with the names of additionally desired SLURM output variables.
	virtual void add_slurm_section(const std::vector<std::string>& slurmvars) = 0;

	/// \brief copy populated generic section into the builder under specified name.
	/// \param name key for a new object in the output.
	/// \param section the value for the object. Note conventional names for certain sections
	/// found in other add_*section functions.
	virtual void add_section(std::string_view name, std::shared_ptr< builder_api > section) = 0;

	/*!
Conversion of the void pointer to data follows the enum type given, as listed
in this table:
        cp_bool: bool *
        cp_char: char *
        cp_char16: char16_t *
        cp_char32: char32_t *
        cp_cstr: char * a c string.
        cp_json_str: char * a json string with nul termination
        cp_xml_str: char * a xml string with nul termination
        cp_yaml_str: char * a yaml string with nul termination
        cp_json:  // pointers to json objects are ignored.
        cp_path:  char * a c string which contains a path name.
        cp_number_str:  char * a c string which contains a arbitrary precision number
        cp_uint8: uint8_t *
        cp_uint16: uint16_t *
        cp_uint32: uint32_t *v = (uint32_t *)p;
        cp_uint64: uint64_t *v = (uint64_t *)p;
        cp_int8: int8_t *v = (int8_t *)p;
        cp_int16: int16_t *v = (int16_t *)p;
        cp_int32: int32_t *v = (int32_t *)p;
        cp_int64: int64_t *v = (int64_t *)p;
        cp_f32: float *v = (float *)p;
        cp_f64: double *v = (double *)p;
 if ADC_SUPPORT_EXTENDED_FLOATS
        cp_f80: long double *v = (long double *)p;
 endif
 if ADC_SUPPORT_QUAD_FLOATS
        cp_f128: __float128 * // not yet implemented
 if
 if ADC_SUPPORT_GPU_FLOATS
        cp_f8_e4m3: // not yet implemented
        cp_f8_e5m2: // not yet implemented
        cp_f16_e5m10: // not yet implemented
        cp_f16_e8m7: // not yet implemented
 endif
        cp_c_f32: float * pointer to two adjacent floats (re, im)
        cp_c_f64: double *v  pointer to two adjacent double (re, im)
 if ADC_SUPPORT_EXTENDED_FLOATS
        cp_c_f80: long double * pointer to two adjacent extended precision (re, im)
 endif
 if ADC_SUPPORT_QUAD_FLOATS
        cp_c_f128: __float128 * pointer to two adjacent quad (re, im) // not yet implemented
 endif
        cp_timespec: struct timespec *
        cp_timeval: struct timeval *
        cp_epoch: int64_t *

	Calls with any unlisted enum value are ignored silently.
	Calls with any type not supported in the current compilation are ignored.

	 */
	virtual void add_from_pointer_type(std::string_view name, void *ref, enum scalar_type t) = 0;

	/// \brief add a named boolean
	virtual void add(std::string_view name, bool value) = 0;

	/// \brief add a named character
	virtual void add(std::string_view name, char value) = 0;

	/// \brief add a named char16_t
	virtual void add(std::string_view name, char16_t value) = 0;

	/// \brief add a named char32_t
	virtual void add(std::string_view name, char32_t value) = 0;

	/// add null-terminated string
	virtual void add(std::string_view name, char* value) = 0;
	/// add null-terminated string
	virtual void add(std::string_view name, const char* value) = 0;
	/// add null-terminated string
	virtual void add(std::string_view name, std::string& value) = 0;
	/// add null-terminated string
	virtual void add(std::string_view name, std::string_view value) = 0;

	/// add null-terminated string filepath
	virtual void add_path(std::string_view name, char* value) = 0;
	/// add null-terminated string filepath
	virtual void add_path(std::string_view name, const char* value) = 0;
	/// add null-terminated string filepath
	virtual void add_path(std::string_view name, std::string& value) = 0;
	/// add null-terminated string filepath
	virtual void add_path(std::string_view name, std::string_view value) = 0;

	/// \brief add string which is serialized json.
	virtual void add_json_string(std::string_view name, std::string_view value) = 0;

	/// \brief add string which is yaml.
	virtual void add_yaml_string(std::string_view name, std::string_view value) = 0;

	/// \brief add string which is xml.
	virtual void add_xml_string(std::string_view name, std::string_view value) = 0;

	/// \brief add string which is an arbitrary precision decimal number
	virtual void add_number_string(std::string_view name, std::string_view value) = 0;

#if ADC_BOOST_JSON_PUBLIC
	// add a named raw json value (array, obj, or value)
	virtual void add(std::string_view name, boost::json::value value) = 0;
#endif

	/// add named uint8_t
	virtual void add(std::string_view name, uint8_t value) = 0;
	/// add named uint16_t
	virtual void add(std::string_view name, uint16_t value) = 0;
	/// add named uint32_t
	virtual void add(std::string_view name, uint32_t value) = 0;
	/// add named uint64_t
	virtual void add(std::string_view name, uint64_t value) = 0;

	/// add named int8_t
	virtual void add(std::string_view name, int8_t value) = 0;
	/// add named int16_t
	virtual void add(std::string_view name, int16_t value) = 0;
	/// add named int32_t
	virtual void add(std::string_view name, int32_t value) = 0;
	/// add named int64_t
	virtual void add(std::string_view name, int64_t value) = 0;

	/// add named 32-bit float
	virtual void add(std::string_view name, float value) = 0;
	/// add named 32-bit complex
	virtual void add(std::string_view name, const std::complex<float>& value) = 0;
	/// add named 64-bit float
	virtual void add(std::string_view name, double value) = 0;
	/// add named 64-bit complex
	virtual void add(std::string_view name, const std::complex<double>& value) = 0;

	/// add timeval
	virtual void add(std::string_view name, const struct timeval& tv) = 0;
	/// add timespec (e.g. clock_gettime)
	virtual void add(std::string_view name, const struct timespec& ts) = 0;
	/// add unix epoch seconds (gettimeofday)
	virtual void add_epoch(std::string_view name, int64_t epoch) = 0;

	/// Fixed length arrays of scalar members.
	/// \param name the label for the array
	/// \param value the array pointer
	/// \param len the number of elements in the array
	/// \param container is a hint about an original container such as
	///        "pointer", "set", "vector", "list", "range", "array".
	///
	/// Helper functions are provided to map std containers to the add_array
	virtual void add_array(std::string_view name, bool value[], size_t len, std::string_view container = "pointer") = 0;

	/// store fixed length array. this is how a character buffer containing nuls is stored.
	virtual void add_array(std::string_view name, const char *value, size_t len, std::string_view container = "pointer") = 0;
	virtual void add_array(std::string_view name, char16_t value[], size_t len, std::string_view container = "pointer") = 0;
	virtual void add_array(std::string_view name, char32_t value[], size_t len, std::string_view container = "pointer") = 0;

	virtual void add_array(std::string_view name, uint8_t value[], size_t len, std::string_view container = "pointer") = 0;
	virtual void add_array(std::string_view name, uint16_t value[], size_t len, std::string_view container = "pointer") = 0;
	virtual void add_array(std::string_view name, uint32_t value[], size_t len, std::string_view container = "pointer") = 0;
	virtual void add_array(std::string_view name, uint64_t value[], size_t len, std::string_view container = "pointer") = 0;

	virtual void add_array(std::string_view name, int8_t value[], size_t len, std::string_view container = "pointer") = 0;
	virtual void add_array(std::string_view name, int16_t value[], size_t len, std::string_view container = "pointer") = 0;
	virtual void add_array(std::string_view name, int32_t value[], size_t len, std::string_view container = "pointer") = 0;
	virtual void add_array(std::string_view name, int64_t value[], size_t len, std::string_view container = "pointer") = 0;

	virtual void add_array(std::string_view name, float value[], size_t len, std::string_view container = "pointer") = 0;
#if 0
	virtual void add_array(std::string_view name, const std::complex<float> value[], size_t len, std::string_view container = "pointer") = 0;
#endif
	virtual void add_array(std::string_view name, double value[], size_t len, std::string_view container = "pointer") = 0;
#if 0
	virtual void add_array(std::string_view name, const std::complex<double> value[], size_t len, std::string_view container = "pointer") = 0;
#endif
	// Arrays of null-terminated strings
	virtual void add_array(std::string_view name, char* value[], size_t len, std::string_view container = "pointer") = 0;
	virtual void add_array(std::string_view name, const char* value[], size_t len, std::string_view container = "pointer") = 0;
	virtual void add_array(std::string_view name, std::string value[], size_t len, std::string_view container = "pointer") = 0;
	virtual void add_array(std::string_view name, const std::string value[], size_t len, std::string_view container = "pointer") = 0;
	virtual void add_array(std::string_view name, const std::vector<std::string> sv, std::string_view container = "vector") = 0;
	virtual void add_array(std::string_view name, const std::set<std::string> ss, std::string_view container = "set") = 0;
	virtual void add_array(std::string_view name, const std::list<std::string> sl, std::string_view container = "list") = 0;

	/// Add Array of strings which are serialized json.
	virtual void add_array_json_string(std::string_view name, const std::string value[], size_t len, std::string_view container = "pointer") = 0;

	/// convert object to a json string reflecting the section hierarchy.
	virtual std::string serialize() = 0;

}; // class builder_api

/** @}*/

} // namespace adc
#endif // adc_builder_hpp
