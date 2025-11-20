/* Copyright 2025 NTESS. See the top-level LICENSE.txt file for details.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "adc/factory.hpp"
#if ADC_BOOST_JSON_PUBLIC
#include "boost/json/src.hpp"
#endif
#include <cerrno>
#include <cstring>
#include <limits>

std::map<std::string, std::string> adc_plugin_file_config = 
	{{ "ADC_FILE_PLUGIN_DIRECTORY", "./test.outputs"}, 
	 { "ADC_FILE_PLUGIN_FILE", "out.file.log" },
	 { "ADC_FILE_PLUGIN_APPEND", "true" }
	};

std::map<std::string, std::string> file_config = 
	{{ "DIRECTORY", "./test.outputs"}, 
	 { "FILE", "out.file.log" },
	 { "APPEND", "true" }
	};

// config w/file_config and call initialize.
int test_publisher(std::shared_ptr<adc::publisher_api> pi, std::shared_ptr<adc::builder_api> b ) {
	int err = 0;
	int e = 0;
	err = pi->config(file_config);
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
	return e;
}

// check in builder named b for a field named NAME with value VAL and types as CPTYPE and CTYPE
#define ROUNDTRIP(NAME, VAL, CPTYPE, CTYPE) \
	ck = b->get_value(NAME); \
	std::cerr << NAME << " roundtrip " << std::flush ; \
	if (ck.kt != adc::k_value || ck.count != 1 || ck.st != adc::CPTYPE || \
		*((CTYPE *)ck.vp) != VAL || !( std::get<CTYPE>(ck.data) == VAL) ) { \
		std::cerr << "BAD" << std::endl; \
		std::cerr << "\tkt: " << ck.kt <<std::endl; \
		std::cerr << "\tst: " << to_string(ck.st) <<std::endl; \
		std::cerr << "\tcount: " << ck.count <<std::endl; \
		std::cerr << "\tvp: " << (CTYPE *)ck.vp <<std::endl; \
		std::cerr << "\tdata: " << std::visit(adc::var_string(), ck.data) <<std::endl; \
	} else \
                std::cerr << "ok " << std::visit(adc::var_string(), ck.data) << std::endl

template<typename STYPE>
static int cmp_argv(std::string *argv, STYPE *val, size_t count)
{
	for (size_t i = 0; i < count; i++) {
		if (argv[i] != std::string(val[i])) {
			return 1;
		}
	}
	return 0;
}

#define ROUNDTRIP_ARRAY_STRING(NAME, VAL, COUNT, CTYPE_VAL) roundtrip_array_string<CTYPE_VAL>(NAME, VAL, COUNT, b)
template <typename STYPE>
static void roundtrip_array_string(const char *name, STYPE* val, size_t count, std::shared_ptr< adc::builder_api > b)
{
	auto ck = b->get_value(name);
	std::cerr << name << " roundtrip "  << std::flush;

	if (ck.kt != adc::k_value) {
		std::cerr << "\n WRONG kt" << std::endl;
	} else if ( ck.st != adc::cp_cstr ) {
		std::cerr << "\n WRONG st" << std::endl;
	} else if (ck.count != count) {
		std::cerr << "\n WRONG: NOT len " << count <<
		       	" (" << ck.count << " instead)" << std::endl;
	} else if ( cmp_argv<STYPE>((std::string *)ck.vp, val, count) ) {
		std::cerr << "\n WRONG data" << std::endl;
		for (size_t i = 0; i < count; i++) {
			std::cerr << "[" << i << "]:" << 
				((std::string *)ck.vp)[i] << " vs " << val[i] << std::endl;
		}
	} else {
		std::cerr << " ok " << ck.container << std::endl;
		return;
	}
	std::cerr << "BAD" << std::endl;
	std::cerr << "\tkt: " << ck.kt <<std::endl;
	std::cerr << "\tst: " << to_string(ck.st) <<std::endl;
	std::cerr << "\tcount: " << ck.count <<std::endl;
	std::cerr << "\tvp: "  << std::endl;
	std::string *argv = (std::string *)ck.vp;
	for (size_t i = 0; i < count; i++) {
		std::cerr << "\t\t" << argv[i] << std::endl;
	}
	std::cerr << "\tdata: " << std::visit(adc::var_string(ck.count), ck.data) <<std::endl; \
	std::cerr << "\tcontainer: " << ck.container << std::endl; \
}


#define ROUNDTRIP_ARRAY(NAME, VAL, COUNT, CTYPE, CPTYPE) roundtrip_array<CTYPE>(NAME, VAL, adc:: CPTYPE, COUNT, b)
template<typename CTYPE >
void roundtrip_array(const char *name, CTYPE* val, adc::scalar_type cptype, size_t count, std::shared_ptr< adc::builder_api > b)
{
	auto ck = b->get_value(name);
	std::cerr << name << " roundtrip "  << std::flush;

	if (ck.kt != adc::k_value) {
		std::cerr << "\n WRONG kt" << std::endl;
	} else if ( ck.st != cptype ) {
		std::cerr << "\n WRONG st" << std::endl;
	} else if (ck.count != count) {
		std::cerr << "\n WRONG: NOT len " << count <<
		       	" (" << ck.count << " instead)" << std::endl;
	} else if ( memcmp(ck.vp, val, sizeof(CTYPE)*count) ) {
		std::cerr << "\n WRONG data" << std::endl;
		for (size_t i = 0; i < count; i++) {
			std::cerr << "[" << i << "]:" << 
				((CTYPE *)ck.vp)[i] << " vs " << val[i] << std::endl;
		}
	} else {
		std::cerr << " ok" << std::endl;
		return;
	}
	std::cerr << "BAD" << std::endl;
	std::cerr << "\tkt: " << ck.kt <<std::endl;
	std::cerr << "\tst: " << to_string(ck.st) <<std::endl;
	std::cerr << "\tcount: " << ck.count <<std::endl;
	std::cerr << "\tvp: " << (CTYPE *)ck.vp <<std::endl;
	std::cerr << "\tdata: " << std::visit(adc::var_string(ck.count), ck.data) <<std::endl; \
}

void roundtrip_string(const char *name, const char *val, adc::scalar_type cptype, std::shared_ptr< adc::builder_api > b)
{
	auto ck = b->get_value(name);
	std::cerr << name << " roundtrip "  << std::flush;
	if (ck.kt != adc::k_value) {
		std::cerr << "\n WRONG kt" << std::endl;
	} else if ( ck.count != strlen(val) ) {
		std::cerr << "\n WRONG count" << std::endl;
	} else if ( ck.st != cptype ) {
		std::cerr << "\n WRONG st" << std::endl;
	} else if ( strcmp(((const char *)ck.vp), val) != 0 ) {
		std::cerr << "\n WRONG vp" << std::endl;
	} else {
		std::cerr << " ok" << std::endl;
		return;
	}
	std::cerr << val << " vs vp " << (const char *)ck.vp << std::endl;
	std::cerr << "\tkt: " << ck.kt <<std::endl;
	std::cerr << "\tst: " << adc::to_string(ck.st) << std::endl;
	std::cerr << "\tcount: " << ck.count << std::endl;
	std::cerr << "\tdata: " << std::visit(adc::var_string(), ck.data) << std::endl;
}

#if 0
// check in builder named b for a string named NAME with char * value VAL and type as CPTYPE.
// Unclear why this macro generates useafterdelete for some cases while the function version
// does not. Some string apparently goes out of scope unexpectedly.
#define ROUNDTRIP_STRING(NAME, VAL, CPTYPE) \
	ck = b->get_value(NAME); \
	std::cerr << NAME << " roundtrip "  << std::flush; \
	if (ck.kt != adc::k_value || ck.count != strlen(VAL) || \
		ck.st != adc::CPTYPE || strcmp(((const char *)ck.vp), VAL) != 0 ) { \
		std::cerr << " BAD " << VAL << " vs " << (const char *)ck.vp << std::endl; \
		std::cerr << "\tkt: " << ck.kt <<std::endl; \
		std::cerr << "\tst: " << to_string(ck.st) <<std::endl; \
		std::cerr << "\tcount: " << ck.count << std::endl; \
		std::cerr << "\tdata: " << std::visit(adc::var_string(), ck.data) <<std::endl; \
	} else \
		std::cerr << " ok" << std::endl
#else
#define ROUNDTRIP_STRING(NAME, VAL, CPTYPE) roundtrip_string(NAME, VAL, adc:: CPTYPE, b)
#endif


void populate_builder(std::shared_ptr< adc::builder_api > b, adc::factory & f) {
#if ADC_BOOST_JSON_PUBLIC
	boost::json::object jo = {{"a","b"},{"C","d"},{"n",1}};
#endif
	adc::field ck;
	b->add("bool0", false);
#if 1
	ROUNDTRIP("bool0", false, cp_bool, bool);
#else
	ck = b->get_value("bool0"); \
	if (ck.kt != adc::k_value || ck.count != 1 || ck.st != adc::cp_bool ||
		*((bool *)ck.vp) != false ||  std::get<bool>(ck.data) != false)
		std::cerr << "bool0" << " not returned correctly" << std::endl;
#endif

	b->add("bool1", true);
	ROUNDTRIP("bool1", true, cp_bool, bool);

	b->add("char1", 'A');
	ROUNDTRIP("char1", 'A', cp_char, char);
	char16_t c16 = u'¢';
	b->add("c16", c16);
	ROUNDTRIP("c16", c16, cp_char16, char16_t);
	char32_t c32 = U'猫';
	b->add("c32", c32);
	ROUNDTRIP("c32", c32, cp_char32, char32_t);

	uint8_t u8 = std::numeric_limits<uint8_t>::max() / 2;
	uint16_t u16 = std::numeric_limits<uint16_t>::max() / 2;
	uint32_t u32 = std::numeric_limits<uint32_t>::max() / 2;
	uint64_t u64 = std::numeric_limits<uint64_t>::max() / 2;
	b->add("u8", u8);
	ROUNDTRIP("u8", u8, cp_uint8, uint8_t);
	b->add("u16", u16);
	ROUNDTRIP("u16", u16, cp_uint16, uint16_t);
	b->add("u32", u32);
	ROUNDTRIP("u32", u32, cp_uint32, uint32_t);
	b->add("u64", u64);
	ROUNDTRIP("u64", u64, cp_uint64, uint64_t);
	int8_t i8 = std::numeric_limits<int8_t>::max() / 2;
	int16_t i16 = std::numeric_limits<int16_t>::max() / 2;
	int32_t i32 = std::numeric_limits<int32_t>::max() / 2;
	int64_t i64 = std::numeric_limits<int64_t>::max() / 2;
	float flt = std::numeric_limits<float>::max() / 2;
	double dbl = std::numeric_limits<double>::max() / 2 ;
	b->add("i8", i8);
	ROUNDTRIP("i8", i8, cp_int8, int8_t);
	b->add("i16", i16);
	ROUNDTRIP("i16", i16, cp_int16, int16_t);
	b->add("i32", i32);
	ROUNDTRIP("i32", i32, cp_int32, int32_t);
	b->add("i64", i64);
	ROUNDTRIP("i64", i64, cp_int64, int64_t);
	b->add("flt", flt);
	ROUNDTRIP("flt", flt, cp_f32, float);
	b->add("dbl", dbl);
	ROUNDTRIP("dbl", dbl, cp_f64, double);

	std::complex<float> fcplx(flt, flt);
	std::complex<double> dcplx(dbl,dbl);

	b->add("fcplx", fcplx);
	ROUNDTRIP("fcplx", fcplx, cp_c_f32, std::complex<float>);

	b->add("dcplx", dcplx);
	ROUNDTRIP("dcplx", dcplx, cp_c_f64, std::complex<double>);

	const std::string ccppstr("ccppstr");
	b->add("ccppstr", ccppstr);
	ROUNDTRIP_STRING("ccppstr", ccppstr.c_str(), cp_cstr);

	std::string cppstr("cppstr");
	b->add("cppstr", cppstr);
	ROUNDTRIP_STRING("cppstr", cppstr.c_str(), cp_cstr);

	const char *cstr = "cstr_nul";
	b->add("cstr1", cstr);
	ROUNDTRIP_STRING("cstr1", cstr, cp_cstr);

	const char *jstr = "{\"a\":\"b\", \"c\":[1,2, 3]}";
	b->add_json_string("jstr1", std::string(jstr));
	ROUNDTRIP_STRING("jstr1", jstr, cp_json_str);

	const char *ystr = "---\na: b\nc: [1,2, 3]\nd:\n  e: 1\n  f: 2";
	b->add_yaml_string("ystr1", ystr);
	ROUNDTRIP_STRING("ystr1", ystr, cp_yaml_str);

	const char *xstr = "<note> <to>Tove</to> <from>Jani</from> </note>";
	b->add_xml_string("xstr1", std::string (xstr));
	ROUNDTRIP_STRING("xstr1", xstr, cp_xml_str);

	const char *nstr = "1234567890123456789012345678901234567890.123";
	b->add_number_string("number1", std::string (nstr));
	ROUNDTRIP_STRING("number1", nstr, cp_number_str);
#if ADC_BOOST_JSON_PUBLIC
	b->add("jsonobj", jo);
#endif
	// fixme roundtrip complex
	const char *cstrings[] = {"a", "B", "c2"};
	char *vcstrings[4];
	int32_t ia[4];
	float fa[4];
	double da[4];
	uint64_t ua[4];
	for (int i = 0; i < 4; i++) {
		vcstrings[i] = new char[2];
		snprintf(vcstrings[i], 2, "%d", i);
		ua[i] = i;
		ia[i] = -i;
		da[i] = 3.14*i;
		fa[i] = 3.14*i *2;
	}
	b->add_array("ia", ia, 4);
	b->add_array("ua", ua, 4);
	b->add_array("fa", fa, 4);
	b->add_array("da", da, 4);
	ROUNDTRIP_ARRAY("ia", ia, 4, int32_t, cp_int32);
	ROUNDTRIP_ARRAY("ua", ua, 4, uint64_t, cp_uint64);
	ROUNDTRIP_ARRAY("fa", fa, 4, float, cp_f32);
	ROUNDTRIP_ARRAY("da", da, 4, double, cp_f64);

	b->add_array("nulembed", "a\0b", 3);
	ROUNDTRIP_ARRAY("nulembed", "a\0b", 3, const char, cp_char);

	std::string cppstrings[] = {"ap", "Bp", "c2p"};
	b->add_array("cstrs", cstrings, 3);
	ROUNDTRIP_ARRAY_STRING("cstrs", cstrings, 3, const char *);
	b->add_array("cppstrs", cppstrings, 3);
	ROUNDTRIP_ARRAY_STRING("cppstrs", cppstrings, 3, std::string);
	b->add_array("vcstrs", vcstrings, 4);
	ROUNDTRIP_ARRAY_STRING("vcstrs", vcstrings, 4, char *);
	for (int i = 0; i < 4; i++) {
		delete [] vcstrings[i];
	}
	const char *e1="a1", *e2="a2", *e3="a3",* eb="b",* ec="c_";
	std::vector<std::string> tcsv = { e1, eb, ec };
	std::list<std::string> tcsl = { e2, eb, ec };
	std::set<std::string> tcss = {  e3, eb, ec };
	const char * tcsv_a[] = { e1, eb, ec };
	const char * tcsl_a[] = { e2, eb, ec };
	const char * tcss_a[] = { e3, eb, ec };

	b->add_array("sv", tcsv);
	b->add_array("sl", tcsl);
	b->add_array("ss", tcss);
	ROUNDTRIP_ARRAY_STRING("sv", tcsv_a, 3, const char *);
	ROUNDTRIP_ARRAY_STRING("sl", tcsl_a, 3, const char *);
	// the ss test depends on strcmp order of tcss to match sortedness of tcss_a
	ROUNDTRIP_ARRAY_STRING("ss", tcss_a, 3, const char *);

	std::vector<std::string> children = { "uuid1", "uuid2", "uuid3"};
	b->add_workflow_section();
	b->add_workflow_children(children);

	
	// section test with host-like data treated as app data
	std::shared_ptr<adc::builder_api> host = f.get_builder();
	std::shared_ptr<adc::builder_api> arch = f.get_builder();
	std::shared_ptr<adc::builder_api> cpu = f.get_builder();
	std::shared_ptr<adc::builder_api> mem = f.get_builder();

	host->add("name","myhost");
	host->add("cluster","mycluster");
	cpu->add("processor","pentium II");
	mem->add("size", "256G");

	arch->add_section("cpu", cpu);
	arch->add_section("memory", mem);
	host->add_section("architecture",arch);
	b->add_section("host", host);

#if ADC_BOOST_JSON_PUBLIC
	// section test with host-like data treated as standard schema data (json fields of default types), not app fields)
	boost::json::object host2;
	boost::json::object arch2;
	boost::json::object cpu2;
	boost::json::object mem2;
	// fix [ below
	host2["name"] = "myhost";
	host2["cluster"]="mycluster";
	cpu2["processor"]="pentium II";
	mem2["size"]="256G";
	arch2["cpu"]= cpu2;
	arch2["memory"]= mem2;
	host2["architecture"]=arch2;

	b->add("host2", host2);
#endif

	//std::vector<short> vs = { 1, 2, 3};
	//adc::builder_add_vector1(b, "v1", vs);
	//adc::builder_add_vector2(b, "v2", vs);


	std::string ss = b->serialize();
	std::cout << "-------------------------------" << std::endl;
	std::cout << ss << std::endl;
	std::cout << "-------------------------------" << std::endl;
}

int main(int /* argc */ , char ** /* argv */)
{
	std::cout << "adc pub version: " << adc::publisher_api_version.name << std::endl;
	std::cout << "adc builder version: " << adc::builder_api_version.name << std::endl;
	std::cout << "adc enum version: " << adc::enum_version.name << std::endl;

	adc::factory f;

	std::shared_ptr< adc::builder_api > b = f.get_builder();

	populate_builder(b, f);

#if 1 // switch to 0 when developing new fields and testing them
	std::shared_ptr< adc::publisher_api > p0 = f.get_publisher("none");
	std::cout << test_publisher(p0, b) << std::endl;

	std::shared_ptr< adc::publisher_api > p1 = f.get_publisher("file");
	std::cout << test_publisher(p1, b) << std::endl;

	std::shared_ptr< adc::publisher_api > p2 = f.get_publisher("stdout");
	std::cout << test_publisher(p2, b) << std::endl;

	std::shared_ptr< adc::publisher_api > p3 = f.get_publisher("syslog");
	std::cout << test_publisher(p3, b) << std::endl;

	std::shared_ptr<adc::multi_publisher_api> mp = f.get_multi_publisher();
	mp->add(p0);
	mp->add(p1);
	mp->add(p2);
	mp->add(p3);
	mp->publish(b);
	mp->pause();
	mp->publish(b);
	mp->resume();
	mp->publish(b);
	mp->terminate();
#endif
	int n;
	if ((n = adc::test_enum_strings()))
		std::cout << "scalar_type and to_string(st) are inconsistent: " << n << std::endl;
	return 0;
}
