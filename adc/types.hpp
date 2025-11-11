#ifndef adc_types_hpp
#define adc_types_hpp
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <list>
#include <set>
#include <complex>
#include <variant>
#include <array>
#include <sys/time.h>

#include "adc/adc_config.h"

namespace adc { 

/** \addtogroup API
 *  @{
 */

/*! \brief A version with tags list.
 */
struct ADC_VISIBLE version {
	const std::string name;
	const std::vector <std::string> tags;
	version(const std::string n, std::vector <std::string> t) : name(n), tags(t) {}
};

/// \brief Set to 1 if 8/16 float types for gpus are supported.
/// ADC_SUPPORT_GPU_FLOATS should be defined by build-time configuration
#define ADC_SUPPORT_GPU_FLOATS 0

/// \brief Set to 1 if 80 bit floats for cpus are supported.
/// ADC_SUPPORT_EXTENDED_FLOATS should be defined by build-time configuration
#define ADC_SUPPORT_EXTENDED_FLOATS 0

/// \brief Set to 1 if 128 bit floats for cpus are supported.
/// ADC_SUPPORT_QUAD_FLOATS should be defined by build-time configuration
#define ADC_SUPPORT_QUAD_FLOATS 0

/// \brief include boost::json support in the API
/// ADC_BOOST_JSON_PUBLIC could be defined by build-time configuration.
/// If it is, this library forces boost::json and other boost dependencies
/// on the build of any application which uses it.
#define ADC_BOOST_JSON_PUBLIC 0

/*! \brief the version number of enum scalar_type and object_type 
 */
inline version enum_version("1.0.0", {"none"});

/*! \brief field types for scientific data encode/decode with json.
 *
 * Bit precision and C vs specialized strings are preserved
 * when data is tagged following this enum.
 * The add() functions on the builder api automatically tag with this.
 */
enum scalar_type {
	cp_none,
	cp_bool,	//!< bool (true/false,1/0)
	cp_char,	//!< char (8 bit)
	cp_char16,	//!< char16_t
	cp_char32,	//!< char32_t
	// string is problematic. For embedded nul data or utf-8, use array of char8
	cp_cstr,	//!< c null-terminated string
	cp_json_str,	//!< c null-terminated string that contains valid json
	cp_yaml_str,	//!< c null-terminated string that contains valid yaml
	cp_xml_str,	//!< c null-terminated string that contains valid xml
	cp_json,	//!< json value (object, list, etc)
	cp_path,	//!< c null-terminated string which names a file-system path
	cp_number_str,	//!< c null-terminated string containing an exact decimal representation of arbitrary precision
	// unsigned int types
	cp_uint8,	//!< uint8_t
	cp_uint16,	//!< uint16_t
	cp_uint32,	//!< uint32_t
	cp_uint64,	//!< uint64_t
	// signed int
	cp_int8,	//!< int8_t
	cp_int16,	//!< int16_t
	cp_int32,	//!< int32_t
	cp_int64,	//!< int64_t
	// float
	cp_f32,		//!< 32 bit float
	cp_f64,		//!< 64 bit float
	cp_f80,		//!< 80 bit float; requires ADC_SUPPORT_EXTENDED_FLOATS support
	cp_f128,	//!< 128 bit float; requires ADC_SUPPORT_QUAD_FLOATS support
	// mini float types
	cp_f8_e4m3,	//!< 8 bit float (3 mantissa, 4 exponent); requires ADC_SUPPORT_GPU_FLOATS support
	cp_f8_e5m2,	//!< 8 bit float (2 mantissa, 5 exponent); requires ADC_SUPPORT_GPU_FLOATS support
	cp_f16_e5m10,	//!< 16 bit float (10 mantissa, 5 exponent); requires ADC_SUPPORT_GPU_FLOATS support
	cp_f16_e8m7,	//!< 16 bit bfloat (7 mantissa, 8 exponent); requires ADC_SUPPORT_GPU_FLOATS support
	// complex float types
	cp_c_f32,	//!< complex<float>
	cp_c_f64,	//!< complex<double>
	cp_c_f80,	//!< complex<extended>; requires ADC_SUPPORT_EXTENDED_FLOATS support
	cp_c_f128,	//!< complex<quad>; requires ADC_SUPPORT_QUAD_FLOATS support
	// time types
	cp_timespec,	//!< (second, nanosecond) as int64_t, int64_t pair from clock_gettime
	cp_timeval,	//!< gettimeofday struct timeval (second, microsecond) as int64_t pair
	cp_epoch,	//!< time(NULL) seconds since the epoch (UNIX) as int64_t
	// end mark
	cp_last
};
/// when expanding scalar_type, always update enum.ipp to match.

enum key_type {
	k_none,
	k_section,
	k_value
};

/*! \brief variant for querying builder data.
 *
 * Any changes here must be reflected in the var_string operations.
 */
typedef std::variant<
		bool,
		char,
		char16_t,
		char32_t,
		int8_t,
		int16_t,
		int32_t,
		int64_t,
		uint8_t,
		uint16_t,
		uint32_t,
		uint64_t,
		float,
		double,
		std::complex<float>,
		std::complex<double>,
		std::array<int64_t, 2>,
		std::string,
		std::shared_ptr<bool[]>,
		std::shared_ptr<char[]>,
		std::shared_ptr<char16_t[]>,
		std::shared_ptr<char32_t[]>,
		std::shared_ptr<int8_t[]>,
		std::shared_ptr<int16_t[]>,
		std::shared_ptr<int32_t[]>,
		std::shared_ptr<int64_t[]>,
		std::shared_ptr<uint8_t[]>,
		std::shared_ptr<uint16_t[]>,
		std::shared_ptr<uint32_t[]>,
		std::shared_ptr<uint64_t[]>,
		std::shared_ptr<float[]>,
		std::shared_ptr<double[]>,
		std::shared_ptr<std::complex<float>[]>,
		std::shared_ptr<std::complex<double>[]>,
		std::shared_ptr<std::string[]>
		// fixme: need to ifdef extended/quad/gpu types here in the variant definition.
	> variant;

// these indicate how to handle a data pointer returned from builder lookups.
// vp is often pointing into data, but not in all cases (string, 8byte types).
struct field {
	key_type kt;           //!< kind of data associated with the name queried
	scalar_type st;        //!< scalar type of the data as published,
	const void *vp;        //!< address of data to be cast according to st for use with c/fortran
	size_t count;          //!< number of elements in vp.
	std::string container; //!< name of the container variety given to see builder::add_array
	variant data;
};

/*! \brief get the string representation of a scalar_type value */
ADC_VISIBLE const std::string to_string(scalar_type st);

/*! \brief get string of float using to_chars. */
ADC_VISIBLE const std::string to_string(float);

/*! \brief get string of double using to_chars. */
ADC_VISIBLE const std::string to_string(double);

/*! \brief get string of array */
ADC_VISIBLE const std::string to_string(void *data, scalar_type cptype, size_t count);

/*! \brief get the enum representation of a scalar_type string */
ADC_VISIBLE scalar_type scalar_type_from_name(const std::string& name);

/*! \brief return non-zero if to_string and enum scalar_type are inconsisent. */
ADC_VISIBLE int test_enum_strings();

/*! \brief classification of json-adjacent structure elements.
 * This is not currently in use and may be retired soon.
 */
enum object_type {
	co_list = cp_last, //!< ordered list of arbitrary values
	co_map,	//!< string keyed map of arbitrary values
	co_array, //!< 0-indexed continguous array of type-identical values
	co_scalar //!< single value
};

/*! \brief return string for printing from variant v.  */
class var_string {
public:
	std::string operator()(const bool x) const { return std::string(x ? "true": "false"); }
	std::string operator()(char x) const { return std::string(1, x); }
	std::string operator()(char16_t x) const { return std::to_string((uint16_t) x); }
	std::string operator()(char32_t x) const { return std::to_string((uint32_t) x); }
	std::string operator()(int8_t x) const { return std::to_string(x); }
	std::string operator()(int16_t x) const { return std::to_string(x); }
	std::string operator()(int32_t x) const { return std::to_string(x); }
	std::string operator()(int64_t x) const { return std::to_string(x); }
	std::string operator()(uint8_t x) const { return std::to_string(x); }
	std::string operator()(uint16_t x) const { return std::to_string(x); }
	std::string operator()(uint32_t x) const { return std::to_string(x); }
	std::string operator()(uint64_t x) const { return std::to_string(x); }
	std::string operator()(float x) const { return to_string(x); }
	std::string operator()(double x) const { return to_string(x); }
	std::string operator()(std::complex<float> x) const { return to_string(x.real()) + ",i" + to_string(x.imag()); }
	std::string operator()(std::complex<double> x) const { return to_string(x.real()) + ",i" + to_string(x.imag()); }
	std::string operator()(std::array<int64_t, 2> x) const { return std::to_string(x[0]) + "," + std::to_string(x[1]); }
	std::string operator()(std::string x) const { return std::string(x); }
	std::string operator()(std::shared_ptr<bool[]> x) const { return to_string((void *)x.get(), cp_bool, count); }
	std::string operator()(std::shared_ptr<char[]> x) const { return to_string((void *)x.get(), cp_char, count); }
	std::string operator()(std::shared_ptr<char16_t[]> x) const { return to_string((void *)x.get(), cp_char16, count); }
	std::string operator()(std::shared_ptr<char32_t[]> x) const { return to_string((void *)x.get(), cp_char32, count); }
	std::string operator()(std::shared_ptr<int8_t[]> x) const { return to_string((void *)x.get(), cp_int8, count); }
	std::string operator()(std::shared_ptr<int16_t[]> x) const { return to_string((void *)x.get(), cp_int16, count); }
	std::string operator()(std::shared_ptr<int32_t[]> x) const { return to_string((void *)x.get(), cp_int32, count); }
	std::string operator()(std::shared_ptr<int64_t[]> x) const { return to_string((void *)x.get(), cp_int64, count); }
	std::string operator()(std::shared_ptr<uint8_t[]> x) const { return to_string((void *)x.get(), cp_uint8, count); }
	std::string operator()(std::shared_ptr<uint16_t[]> x) const { return to_string((void *)x.get(), cp_uint16, count); }
	std::string operator()(std::shared_ptr<uint32_t[]> x) const { return to_string((void *)x.get(), cp_uint32, count); }
	std::string operator()(std::shared_ptr<uint64_t[]> x) const { return to_string((void *)x.get(), cp_uint64, count); }
	std::string operator()(std::shared_ptr<float[]> x) const { return to_string((void *)x.get(), cp_f32, count); }
	std::string operator()(std::shared_ptr<double[]> x) const { return to_string((void *)x.get(), cp_f64, count); }
	std::string operator()(std::shared_ptr<std::complex<float>[]> x) const { return to_string((void *)x.get(), cp_c_f32, count); }
	std::string operator()(std::shared_ptr<std::complex<double>[]> x) const { return to_string((void *)x.get(), cp_c_f64, count); }
	std::string operator()(std::shared_ptr<std::string[]> x) const { return to_string((void *)x.get(), cp_cstr, count); }
	// fixme: need to ifdef extended/quad/gpu types here in the variant definition.

	var_string() : count(1) {}
	var_string(size_t count) : count(count) {}

private:
	size_t count;
};

/** @}*/
};
#endif // adc_types_hpp
