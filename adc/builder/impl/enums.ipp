/* Copyright 2025 NTESS. See the top-level LICENSE.txt file for details.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef adc_builder_impl_enums_ipp_hpp
#define adc_builder_impl_enums_ipp_hpp
#include <mutex>
#include <charconv>
namespace adc { 
namespace impl {

// This array must line up with the enum scalar_type in types.h
// If it does not, test_enum_strings() will return non-zero.
std::map< adc::scalar_type, std::string > scalar_type_name = {
	{ cp_none, "none" },
	{ cp_bool, "bool" },
	{ cp_char, "char" },
	{ cp_char16, "char16" },
	{ cp_char32, "char32" },
	{ cp_cstr, "cstr" },
	{ cp_json_str, "json_str" },
	{ cp_yaml_str, "yaml_str" },
	{ cp_xml_str, "xml_str" },
	{ cp_json, "json" },
	{ cp_path, "path" },
	{ cp_number_str, "number_str" },
	{ cp_uint8, "uint8" },
	{ cp_uint16, "uint16" },
	{ cp_uint32, "uint32" },
	{ cp_uint64, "uint64" },
	{ cp_int8, "int8" },
	{ cp_int16, "int16" },
	{ cp_int32, "int32" },
	{ cp_int64, "int64" },
	{ cp_f32, "f32" },
	{ cp_f64, "f64" },
	{ cp_f80, "f80" },
	{ cp_f128, "f128" },
	{ cp_f8_e4m3, "f8_e4m3" },
	{ cp_f8_e5m2, "f8_e5m2" },
	{ cp_f16_e5m10, "f16_e5m10" },
	{ cp_f16_e8m7, "f16_e8m7" },
	{ cp_c_f32, "c_f32" },
	{ cp_c_f64, "c_f64" },
	{ cp_c_f80, "c_f80" },
	{ cp_c_f128, "c_f128" },
	{ cp_timespec, "timespec" },
	{ cp_timeval, "timeval" },
	{ cp_epoch, "epoch" },
	{ cp_last, "last" },
};

std::map< adc::scalar_type, boost::json::kind > scalar_type_representation = { // prec fixme: adc::impl::scalar_type_representation
	{ cp_none,	boost::json::kind::null },
	{ cp_bool,	boost::json::kind::bool_ },
	{ cp_char,	boost::json::kind::int64 },
	{ cp_char16,	boost::json::kind::uint64 },
	{ cp_char32,	boost::json::kind::uint64 },
	{ cp_cstr,	boost::json::kind::string },
	{ cp_json_str,	boost::json::kind::string },
	{ cp_yaml_str,	boost::json::kind::string },
	{ cp_xml_str,	boost::json::kind::string },
	{ cp_json,	boost::json::kind::string },
	{ cp_path,	boost::json::kind::string },
	{ cp_number_str,	boost::json::kind::string },
	{ cp_uint8,	boost::json::kind::uint64 },
	{ cp_uint16,	boost::json::kind::uint64 },
	{ cp_uint32,	boost::json::kind::uint64 },
	{ cp_uint64,	boost::json::kind::string },
	{ cp_int8,	boost::json::kind::int64 },
	{ cp_int16,	boost::json::kind::int64 },
	{ cp_int32,	boost::json::kind::int64 },
	{ cp_int64,	boost::json::kind::int64 },
	{ cp_f32,	boost::json::kind::double_ },
	{ cp_f64,	boost::json::kind::double_ },
	{ cp_f80,	boost::json::kind::null /* "f80" */ }, //  prec fixme: rstring? json
	{ cp_f128,	boost::json::kind::null /* "f128" */ }, //  prec fixme: rstring? json
	{ cp_f8_e4m3,	boost::json::kind::null /* "f8_e4m3" */ }, //  prec fixme: rstring?  json
	{ cp_f8_e5m2,	boost::json::kind::null /* "f8_e5m2" */ }, //  prec fixme: rstring?  json
	{ cp_f16_e5m10,	boost::json::kind::null /* "f16_e5m10" */ }, //  prec fixme: rstring? json
	{ cp_f16_e8m7,	boost::json::kind::null /* "f16_e8m7" */ }, //  prec fixme: rstring?  json
	{ cp_c_f32,	boost::json::kind::double_ },
	{ cp_c_f64,	boost::json::kind::double_ },
	{ cp_c_f80,	boost::json::kind::null /* "c_f80" */ }, //  prec fixme: rstring?  json
	{ cp_c_f128,	boost::json::kind::null /* "c_f128" */ }, //  prec fixme: rstring?  json
	{ cp_timespec,	boost::json::kind::int64 }, // int64[2]
	{ cp_timeval,	boost::json::kind::int64 }, // int64[2]
	{ cp_epoch,	boost::json::kind::int64 },
	{ cp_last,	boost::json::kind::null }
};

} // namespace adc::impl


boost::json::kind scalar_type_representation(scalar_type st) {
	if (st >= cp_none && st <= cp_last)
		return adc::impl::scalar_type_representation[st];
	return boost::json::kind::null;
};

/* return the conversion of name to scalar type, 
 * after stripping prefix "array_" if present.
 */
scalar_type scalar_type_from_name(const std::string& name)
{

	
	std::string basename, prefix = "array_";
	if (name.length() >= prefix.length() &&
		name.substr(0, prefix.length()) == prefix) {
		basename = name.substr(prefix.length());
	} else {
		basename = name;
	}

	static std::map<std::string, adc::scalar_type> type_to_name;
	static std::mutex mtx;
	{
		std::lock_guard<std::mutex> guard(mtx);
		if (type_to_name.size() == 0) {
			for (const auto& pair : adc::impl::scalar_type_name) {
				type_to_name[pair.second] = pair.first;
			}
		}
	}
	if (type_to_name.count(basename)) {
		return type_to_name[basename];
	}
	return cp_none;
}

const std::string to_string(float f)
{
	const size_t size = 20;
	char buf[size]{};
	std::to_chars_result r = std::to_chars(buf, buf + size, f);

	if (r.ec != std::errc()) {
		std::cout << std::make_error_code(r.ec).message() << '\n';
		return std::string("NaN_unexpected");
	} else {
		std::string str(buf, r.ptr - buf);
		return str;
	}
}

const std::string to_string(double f)
{
	const size_t size = 40;
	char buf[size]{};
	std::to_chars_result r = std::to_chars(buf, buf + size, f);

	if (r.ec != std::errc()) {
		std::cout << std::make_error_code(r.ec).message() << '\n';
		return std::string("NaN_unexpected");
	} else {
		std::string str(buf, r.ptr - buf);
		return str;
	}
}

#define elt_to_string(CTYPE) \
{ \
	CTYPE *a = (CTYPE *)data; \
	for (size_t i=0; i < count; i++) { \
		o << a[i]; \
		if (i < count-1) \
			o << ", "; \
	} \
}

#define f_elt_to_string(CTYPE) \
{ \
	CTYPE *a = (CTYPE *)data; \
	for (size_t i=0; i < count; i++) { \
		o << to_string(a[i]); \
		if (i < count-1) \
			o << ", "; \
	} \
}

#define cf_elt_to_string(CTYPE) \
{ \
	std::complex<CTYPE> *a = (std::complex<CTYPE> *)data; \
	for (size_t i=0; i < count; i++) { \
		o << "{" << to_string(a[i].real()); \
		o << ", " << to_string(a[i].imag()); \
		o <<  " }"; \
		if (i < count-1) \
			o << ", "; \
	} \
}

const std::string to_string(void *data, scalar_type cptype, size_t count) // prec fixme
{
	// [elts*count]
	std::string s;
	s.reserve(count*30);
	std::ostringstream o(s);
	o << "[";

	switch (cptype) {
	case cp_last:
		[[fallthrough]];
	case cp_none:
		o << "bug1";
		break;
	case cp_bool:
		{
			bool *a = (bool *)data;
			for (size_t i; i < count; i++) {
				o << (a[i] ? "true" : "false");
				if (i < count-1)
					o << ", ";
			}
		}
		break;
	case cp_char:
		elt_to_string(char);
		break;
	case cp_char16:
		elt_to_string(char16_t);
		break;
	case cp_char32:
		elt_to_string(char32_t);
		break;
	case cp_cstr:
		[[fallthrough]];
	case cp_json_str:
		[[fallthrough]];
	case cp_yaml_str:
		[[fallthrough]];
	case cp_xml_str:
		[[fallthrough]];
	case cp_json:
		[[fallthrough]];
	case cp_path:
		[[fallthrough]];
	case cp_number_str:
		elt_to_string(std::string);
		break;
	case cp_uint8:
		elt_to_string(uint8_t);
		break;
	case cp_uint16:
		elt_to_string(uint16_t);
		break;
	case cp_uint32:
		elt_to_string(uint32_t);
		break;
	case cp_uint64:
		elt_to_string(uint64_t);
		break;
	case cp_int8:
		elt_to_string(int8_t);
		break;
	case cp_int16:
		elt_to_string(int16_t);
		break;
	case cp_int32:
		elt_to_string(int32_t);
		break;
	case cp_int64:
		elt_to_string(int64_t);
		break;
	case cp_f32:
		f_elt_to_string(float);
		break;
	case cp_f64:
		f_elt_to_string(double);
		break;
#if ADC_SUPPORT_EXTENDED_FLOATS
	case cp_f80:
		f_elt_to_string(FLOAT80);
		break;
#else
	case cp_f80:
		o << "unsupported-by-build-" << to_string(cptype);
		break;
#endif
#if ADC_SUPPORT_QUAD_FLOATS
	case cp_f128:
		f_elt_to_string(FLOAT128);
		break;
#else
	case cp_f128:
		o << "unsupported-by-build-" << to_string(cptype);
		break;
#endif
#if ADC_SUPPORT_GPU_FLOATS
	case cp_f8_e4m3:
		f_elt_to_string(FLOAT_4_3);
		break;
	case cp_f8_e5m2:
		f_elt_to_string(FLOAT_5_2);
		break;
	case cp_f16_e5m10:
		f_elt_to_string(FLOAT_5_10);
		break;
	case cp_f16_e8m7:
		f_elt_to_string(FLOAT_8_7);
		break;
#else
	case cp_f8_e4m3:
		o << "unsupported-by-build-" << to_string(cptype);
		break;
	case cp_f8_e5m2:
		o << "unsupported-by-build-" << to_string(cptype);
		break;
	case cp_f16_e5m10:
		o << "unsupported-by-build-" << to_string(cptype);
		break;
	case cp_f16_e8m7:
		o << "unsupported-by-build-" << to_string(cptype);
		break;
#endif
	case cp_c_f32:
		cf_elt_to_string(float);
		break;
	case cp_c_f64:
		cf_elt_to_string(double);
		break;
#if ADC_SUPPORT_EXTENDED_FLOATS
	case cp_c_f80:
		cf_elt_to_string(std::complex<FLOAT80>);
		break;
#else
	case cp_c_f80:
		o << "unsupported-by-build-" << to_string(cptype);
		break;
#endif
#if ADC_SUPPORT_QUAD_FLOATS
	case cp_c_f128:
		cf_elt_to_string(std::complex<FLOAT128>);
		break;
#else
	case cp_c_f128:
		o << "unsupported-by-build-" << to_string(cptype);
		break;
#endif
	case cp_timespec:
		[[fallthrough]];
	case cp_timeval:
		[[fallthrough]];
	case cp_epoch: // fixme? array of epochs
		o << "bug2";
		break;
	}
	o << "]";
	return o.str();
}


#define BAD_ST "UNKNOWN_scalar_type"

const std::string to_string(scalar_type st) {
	if (st >= cp_none && st <= cp_last)
		return adc::impl::scalar_type_name[st];
	return BAD_ST;
};

int test_enum_strings() {
	int err = 0;
#define check_enum(X) if (to_string(cp_##X) != #X) { err++; std::cout << "enum " << #X << " is inconsistent with " << cp_##X <<  " from " << to_string(cp_##X) << std::endl; }
	check_enum(none);	
	check_enum(bool);
	check_enum(char);
	check_enum(char16);
	check_enum(char32);
	check_enum(cstr);
	check_enum(json_str);
	check_enum(yaml_str);
	check_enum(xml_str);
	check_enum(number_str);
	check_enum(json);
	check_enum(path);
	check_enum(uint8);
	check_enum(uint16);
	check_enum(uint32);
	check_enum(uint64);
	check_enum(int8);
	check_enum(int16);
	check_enum(int32);
	check_enum(int64);
	check_enum(f32);
	check_enum(f64);
	check_enum(f80);
	check_enum(f128);
	check_enum(f8_e4m3);
	check_enum(f8_e5m2);
	check_enum(f16_e5m10);
	check_enum(f16_e8m7);
	check_enum(c_f32);
	check_enum(c_f64);
	check_enum(c_f80);
	check_enum(c_f128);
	check_enum(timespec);
	check_enum(timeval);
	check_enum(epoch);
	check_enum(last);
        if (to_string((enum scalar_type)1000) != BAD_ST) { 
		err++;
		std::cout << "out of range input to to_string() returned incorrect result" << std::endl;
	}
	return err;
}

} // namespace adc
#endif // adc_builder_impl_enums_ipp_hpp
