#include <pwd.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sstream>
#include <fstream>
#include <array>
#include <ostream>
#include <utility>
#include <string>
#include <cstdio>
#include <boost/algorithm/string.hpp>
#include <uuid/uuid.h>

#include <adc/builder/impl/builder.hpp>
#include <adc/builder/impl/outpipe.ipp>

/* implementation note:
 * do not use language features beyond c++17.
 */

namespace adc {



std::string tolower_s(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::tolower(c); }
                  );
    return s;
}

std::string get_exe(size_t bufsize)
{
	ssize_t ret;
	auto buffer = std::make_unique<char[]>(bufsize);
	ret = readlink("/proc/self/exe", buffer.get(), bufsize - 1);
	if (ret < 0)
		sprintf(buffer.get(),"(nullexe)");
	else
		buffer.get()[ret] = '\0';
	return buffer.get();
}

// std date and formatters are still unstable; use c.
// fixme strftime isn't 8601 tz format compliant (no xx:yy) adc::format_timespec_8601
// fixme consider using nanoseconds instead of milli. adc::format_timespec_8601
std::string format_timespec_8601(struct timespec& ts)
{
	struct tm tm;
	char prefix[40] = "YYYY-MM-ddTHH:mm:ss";
	char suffix[40] = "0000           ";
	char millis[20];
	localtime_r(&ts.tv_sec, &tm);
	sprintf(millis, "%03ld", ts.tv_nsec / 1000000);
	strftime(prefix, sizeof(prefix), "%Y-%m-%dT%H:%M:%S.", &tm);
	strftime(suffix, sizeof(suffix), "%z", &tm);
	std::string buf = std::string(prefix) + std::string(millis) + std::string(suffix);
	return buf;

}

std::string format_timespec_utc_ns(struct timespec& ts)
{
	char str[40];
	snprintf(str, sizeof(str), "%ld.%09ld",(long)ts.tv_sec, (long)ts.tv_nsec);
	return std::string(str);
}

std::string get_lscpu(){
	pipe_data lscpu = out_pipe::run("lscpu -J");
	if (lscpu.rc) {
		return "{}";
	}
	return lscpu.output;
}

#define CPUINFO_FILE  "/proc/cpuinfo"
std::string get_default_affinity()
{
	static std::string affinity_all;
	if (affinity_all.size() < 2) {
		std::ifstream in(CPUINFO_FILE);
		std::string line;
		uint32_t pmax = 0;
		while (std::getline(in, line)) {
			size_t cp = line.find(' ');
			std::string name = line.substr(0, cp);
			if (name == "processor") {
				cp = line.find(':');
				std::istringstream iss(line.substr(cp+1));
				uint32_t val;
				iss >> val;
				if (iss.fail()) {
					return 0;
				} else {
					pmax = (val > pmax ? val : pmax);
				}
			}
		}
		if (pmax == 0) {
			affinity_all = "0-1024";
		} else {
			std::ostringstream oss;
			oss << "0-" << pmax;
			affinity_all = oss.str();
		}
	}
	return affinity_all;
}


std::vector< std::string> get_libs(std::string_view )
{
	pid_t pid = getpid();
	std::ostringstream cmd;
	cmd << "/usr/bin/grep r-xp /proc/" << pid << "/maps | /usr/bin/grep '\\.so' | /usr/bin/sed -e 's%.* /%/%g'";
	pipe_data proclibs = out_pipe::run(cmd.str());
	std::vector< std::string> libs;
	if (proclibs.rc) {
		return libs;
	}
	std::istringstream nss(proclibs.output);
	std::string line;
	while (std::getline(nss, line)) {
		libs.push_back(line);
	}
	return libs;
}

boost::json::object get_numa_hardware()
{
	static bool shell_done;
	static boost::json::object numa_json;
	if (!shell_done) {
		shell_done = 1;
		pipe_data numactl = out_pipe::run("numactl -H");
		size_t numa_node_count = 0;
		if (! numactl.rc) {
			std::vector<std::string> cpulist;
			std::vector<int64_t> sizes;
			std::vector<std::string> numa_node;
			shell_done = 1;
			adc::pipe_data numactl = adc::out_pipe::run("numactl -H");
			std::string line;
			std::istringstream nss(numactl.output);
			while (std::getline(nss, line)) {
				size_t cp = line.find(':');
				std::string name = line.substr(0, cp);
				if (name == "available") {
					std::istringstream iss(line.substr(cp+1));
					iss >> numa_node_count;
					continue;
				}
				if (name.substr(name.length()-4) == "cpus" ) {
					cpulist.push_back(line.substr(cp+2));
					continue;
				}
				if (name.substr(name.length()-4) == "size" ) {
					std::istringstream iss(line.substr(cp+1));
					int64_t mb;
					iss >> mb;
					sizes.push_back(mb);
					continue;
				}
				if (name.substr(name.length()-4) == "nces" )
					break; // stop on "node distances:"
			}
			if (! numa_node_count || sizes.size() != numa_node_count || cpulist.size() != numa_node_count)
				return numa_json; // inconsistent data
#if 0 // FLAT lists
			numa_json["numa_node_count"] =  numa_node_count;
			numa_json["numa_cpu_list"] = boost::json::value_from(cpulist);
			numa_json["numa_node_megabyte"] = boost::json::value_from(sizes);
#endif
			boost::json::array na;
			for (size_t i = 0; i < numa_node_count; i++) {
				boost::json::object o = {
					{"node_number", i},
					{"node_megabytes", sizes[i]},
					{"cpu_list", cpulist[i]}
				};
				na.emplace_back(o);
			}
			numa_json["node_list"] = na;
		}
        }
        return numa_json;
}

boost::json::object get_gpu_data()
{
	static bool shell_done;
	static boost::json::object gpu_json;
	if (!shell_done) {
#ifdef ADC_GPU_DEBUG
		std::cerr << "add_host_section: doing gpu" <<std::endl;
#endif
		size_t gpu_count = 0;
		shell_done = 1;
		pipe_data lspci = out_pipe::run("lspci  -vmm  |grep  -B1 -A 6 -i '3d controller'");
		if (! lspci.rc) {
#ifdef ADC_GPU_DEBUG
			std::cerr << "add_host_section: parsing gpu" <<std::endl;
#endif
			std::vector<std::string> vendor;
			std::vector<std::string> device;
			std::vector<std::string> rev;
			std::vector<int32_t> numa_node;
			shell_done = 1;
			std::string line;
			std::istringstream nss(lspci.output);
			while (std::getline(nss, line)) {
				if (line.substr(0,1) == "-")
					continue;
				size_t cp = line.find(':');
				std::string name = line.substr(0, cp);
				if (name == "Class") {
					std::string cname = boost::algorithm::trim_copy(line.substr(cp+1));
					if (cname == "3D controller") {
						// std::cerr << "add_host_section: found gpu " << gpu_count <<std::endl;
						gpu_count++;
					} else {
						// std::cerr << "add_host_section: found non-gpu " << cname <<std::endl;
					}
					continue;
				}
				if (name == "Vendor" ) {
					vendor.push_back(boost::algorithm::trim_copy(line.substr(cp+1)));
					continue;
				}
				if (name == "Device" ) {
					device.push_back(boost::algorithm::trim_copy(line.substr(cp+1)));
					continue;
				}
				if (name == "Rev" ) {
					rev.push_back(boost::algorithm::trim_copy(line.substr(cp+1)));
					continue;
				}
				if (name == "NUMANode" ) {
					std::istringstream iss(line.substr(cp+1));
					int32_t nn;
					iss >> nn;
					numa_node.push_back(nn);
					continue;
				}
			}
			if (vendor.size() != gpu_count ||
				device.size() != gpu_count ||
				rev.size() != gpu_count ||
				numa_node.size() != gpu_count) {
				std::cerr << "add_host_section: size mismatch " <<
					gpu_count <<
					vendor.size() <<
					device.size() <<
					rev.size() <<
					numa_node.size() <<
					std::endl;
				return gpu_json; // inconsistent data
			}
			gpu_json["gpu_count"] = gpu_count;
			boost::json::array ga;
			for (size_t i = 0; i < gpu_count; i++) {
				boost::json::object o = {
					{"gpu_number", i},
					{"numa_node", numa_node[i]},
					{"vendor", vendor[i]},
					{"device", device[i]},
					{"rev", rev[i]}
				};
				ga.emplace_back(o);
			}
			gpu_json["gpulist"] = ga;
		}
#ifdef ADC_GPU_DEBUG
		else {
			std::cerr << "add_host_section: lspci fail " << lspci.rc <<std::endl;
		}
#endif
	}
	return gpu_json;
}

#define MEMINFO_FILE  "/proc/meminfo"

struct item {
	std::string name;
	uint64_t value;
};

// data read from MEMINFO_FILE or computed therewith
// direct read fields are not documented.
static std::map<std::string, uint64_t> midata = {
	{"MemTotal", 0 },
	{"MemUsed", 0 }, // total - free
	{"MemFree", 0 },
	{"Shmem", 0 },
	{"SReclaimable", 0 },
	{"Buffers", 0 },
	{"Cached", 0 },
	{"CachedAll", 0 }, // Cached + SReclaimable,
	{"MemAvailable", 0 },
	{"SwapTotal", 0 },
	{"SwapUsed", 0 }, // SwapTotal - SwapFree
	{"SwapFree", 0},
	{"valid", 0} // is the full map populated
};

// the number of undocumented members of midata (those read directly)
const size_t meminfo_raw = 9;

// get MemTotal off top of /proc/meminfo
uint64_t get_memtotal()
{
	std::ifstream in(MEMINFO_FILE);
	std::string line;
	while (std::getline(in, line)) {
		size_t cp = line.find(':');
		std::string name = line.substr(0, cp);
		if (name == "MemTotal") {
			std::istringstream iss(line.substr(cp+1));
			uint64_t val;
			iss >> val;
			if (iss.fail()) {
				return 0;
			} else {
				return val;
			}
		}
	}
	return 0;
}

// on return, midata["valid"] is 1 if successful or 0 if a problem reading all
// expected values.
// produce data equivalent to free -k -w
void update_meminfo()
{
	std::ifstream in(MEMINFO_FILE);

	std::string line;
	size_t count = 0;
	while (std::getline(in, line)) {
		size_t cp = line.find(':');
		std::string name = line.substr(0, cp);
		auto it = midata.find(name);
		if (it != midata.end()) {
			std::istringstream iss(line.substr(cp+1));
			uint64_t val;
			iss >> val;
			if (iss.fail()) {
				std::cerr << "error reading value for " << it->first << std::endl;
				continue;
			}
			it->second = val;
			count++;
#ifdef ADC_MEMINFO_DEBUG
			std::cerr << "parsed meminfo." << name << std::endl;
#endif
		}
	}
	if (count == meminfo_raw) {
		midata["valid"] = 1;
		midata["MemUsed"] = midata["MemTotal"] - midata["MemFree"];
		midata["SwapUsed"] = midata["SwapTotal"] - midata["SwapFree"];
		midata["CachedAll"] = midata["Cached"] + midata["SReclaimable"];
		if (midata["MemAvailable"] > midata["MemTotal"]) {
			midata["MemAvailable"] = midata["MemFree"];
			// work around container misreporting
			// documented in procps utility 'free'
		}
		midata["valid"] = 1;
	} else {
		midata["valid"] = 0;
		std::cerr << "read meminfo failed" << std::endl;
		std::cerr << "count = " << count << std::endl;
		std::cerr << "mdsize = " << midata.size()  << std::endl;
	}
}

builder::builder(void *mpi_communicator_p) : mpi_comm_p(mpi_communicator_p) {
}

// auto-populate the header section with application name
void builder::add_header_section(std::string_view application_name)
{
	uid_t uid = geteuid();
	struct passwd pw;
	struct passwd *pwp = NULL;
	int bsize = sysconf(_SC_GETPW_R_SIZE_MAX);
	if (bsize < 0)
		bsize = 16384;
	char buf[bsize];
	getpwuid_r(uid, &pw, buf, bsize, &pwp);
	const char *uname;
	if (pwp) {
		uname = pw.pw_name;
	} else {
		uname = "<unknown_user>";
	}
	struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
	std::string ts_8601 = format_timespec_8601(ts);
	std::string ts_ns = format_timespec_utc_ns(ts);

	boost::json::object vv = {
			{ "version", adc::enum_version.name}
		};
	vv["tags"] = boost::json::value_from(adc::enum_version.tags);
	uuid_t uuid;
	uuid_generate_random(uuid);
	char uuidbuf[40];
	uuid_unparse_lower(uuid, uuidbuf);
	boost::json::value jv = {
		{"adc_api_version", vv },
		{"timestamp", ts_ns },
		{"datestamp", ts_8601 },
		{"user", uname },
		{"uid", std::to_string(uid) },
		{"application", application_name},
		{"uuid", uuidbuf }
	};
	d["header"] = jv;
}

std::vector<std::string> split_string(const std::string& s, char delimiter)
{
	std::vector<std::string> tokens;
	std::stringstream ss(s);
	std::string token;
	while (std::getline(ss, token, delimiter)) {
		if (token.length() > 0) {
			tokens.push_back(token);
		}
	}
	return tokens;
}

std::vector<std::string> builder::get_host_env_vars()
{
	const char *env = getenv("ADC_HOST_SECTION_ENV");
	if (env) {
		return split_string(std::string(env), ':');
	}
	std::vector<std::string> s;
	return s;
}

#define ADC_HS_BAD ~(ADC_HS_ALL)
// auto-populate the host section
void builder::add_host_section(int32_t subsections)
{

	if (ADC_HS_BAD & subsections) {
		std::cerr << "bad arg to add_host_section: " << subsections <<std::endl;
		return;
	}
	struct utsname ubuf;
	int uerr = uname(&ubuf);
	if (uerr < 0) {
		std::cerr << "uname failed in add_host_section" <<std::endl;
		return;
	}
	boost::json::object jv = {
		{"name", std::string(ubuf.nodename) }
	};
	if ( subsections & ADC_HS_OS ) {
		jv["os_family"] = std::string(ubuf.sysname);
		jv["os_version"] = std::string(ubuf.release);
		jv["os_arch"] = std::string(ubuf.machine);
		jv["os_build"] = std::string(ubuf.version);
	}
	if (subsections & ADC_HS_ENV) {
		std::vector<std::string> env_vars = get_host_env_vars();
		for (auto it = env_vars.begin();
			it != env_vars.end(); it++) {
				const char *s = getenv(it->c_str());
				if (!s)
					s = "";
				jv[*it] = std::string(s);
		}
	}
	if (subsections & ADC_HS_RAMSIZE) {
		if (midata["valid"] == 1) {
			jv["mem_total"] = midata["MemTotal"];
		} else {
			update_meminfo();
			if (midata["valid"] == 1)
				jv["mem_total"] = midata["MemTotal"];
			else
				jv["mem_total"] = 0;
		}
	}
	if (subsections & ADC_HS_CPU) {
		boost::system::error_code ec;
		std::string lscpu = get_lscpu();
		auto cv = boost::json::parse(lscpu, ec);
		if (ec) {
			std::cerr << "unable to parse ("<< ec <<") lscpu output: " << lscpu << std::endl;
		}
		jv["cpu"] = cv;
	}
	if (subsections & ADC_HS_GPU) {
		jv["gpu"] = get_gpu_data();
	}
	if (subsections & ADC_HS_NUMA) {
		jv["numa_hardware"] = get_numa_hardware();
	}

	d["host"] = jv;
}

// populate application run-time data to app_data section.
// any relationship to previous jobs/higher level workflows goes in app_data
// somehow.
void builder::add_app_data_section(std::shared_ptr< builder_api > app_data)
{
	auto app_data_derived = std::dynamic_pointer_cast<builder>(app_data);
	d["app_data"] = app_data_derived->flatten();
}

void builder::add_memory_usage_section() {
	update_meminfo();
	if (midata["valid"] == 0) {
		boost::json::object jv;
		d["memory_usage"] = jv;
		return;
	}

	boost::json::value jv = {
		{"mem_total", midata["MemTotal"]},
		{"mem_used", midata["MemUsed"]},
		{"mem_free", midata["MemFree"]},
		{"mem_shared", midata["Shmem"]},
		{"mem_buffers", midata["Buffers"]},
		{"mem_cache", midata["CachedAll"]},
		{"mem_available", midata["MemAvailable"]},
		{"swap_total", midata["SwapTotal"]},
		{"swap_used", midata["SwapUsed"]},
		{"swap_free", midata["SwapFree"]}
	};
	d["memory_usage"] = jv;
}

// populate application run-time physics (re)configuration/result to model_data section.
// e.g. changes in mesh/particle decomp go here.
void builder::add_model_data_section(std::shared_ptr< builder_api > model_data)
{
	auto model_data_derived = std::dynamic_pointer_cast<builder>(model_data);
	d["model_data"] = model_data_derived->flatten();
}

// auto-populate code section with os-derived info at time of call,
// tag, version, and code_details blob.
void builder::add_code_section(std::string tag, std::shared_ptr< builder_api > version, std::shared_ptr< builder_api > code_details)
{
	std::string fullpath = get_exe(4096);
	const char *basename = strrchr(fullpath.c_str(), '/');
	if (!basename)
		basename = fullpath.c_str();
	else
		basename++;
	std::vector< std::string> libs = get_libs(fullpath);
	auto code_details_derived = std::dynamic_pointer_cast<builder>(code_details);
	auto version_derived = std::dynamic_pointer_cast<builder>(version);
	boost::json::value jv = {
		{"name", tag },
		{"program", basename},
		{"path", fullpath},
		{"version", version_derived->d},
		{"libs", boost::json::value_from(libs) },
		{"details", code_details_derived->flatten()}
	};
	d["code"] = jv;
}

// populate build/install configuration information like options enabled
void builder::add_code_configuration_section(std::shared_ptr< builder_api > build_details)
{
	auto build_details_derived = std::dynamic_pointer_cast<builder>(build_details);
	d["code_configuration"] = build_details_derived->flatten();
}

// populate exit_data section
void builder::add_exit_data_section(int return_code, std::string status, std::shared_ptr< builder_api > status_details)
{
	auto status_details_derived = std::dynamic_pointer_cast<builder>(status_details);
	boost::json::value jv = {
		{ "return_code", std::to_string(return_code)},
		{ "status", status},
		{ "details", status_details_derived->flatten()}
	};
	d["exit_data"] = jv;
}


void builder::add_section(std::string_view name, std::shared_ptr< builder_api > section)
{

	auto nk = kind(name);
	if (nk == k_none) {
		auto section_derived = std::dynamic_pointer_cast<builder>(section);
		sections[std::string(name)] = std::move(section_derived);
	}
}

std::shared_ptr< builder_api > builder::get_section(std::string_view name)
{
	auto nk = kind(name);
	if (nk == k_section)
		return sections[std::string(name)];
	// should we throw here instead? probably not, for optional sections
	return std::shared_ptr< builder_api >(NULL);
}

std::vector< std::string > builder::get_section_names()
{
	std::vector< std::string > result;
	for (auto const& element : sections) {
		result.push_back(element.first);
	}
	return result;
}

std::vector< std::string > builder::get_field_names()
{
//	iterate d keys
//	can we return an interator here instead?
	std::vector< std::string > result;
	for (auto const& element : d) {
		result.push_back(element.key());
	}
	return result;
}

static void get_scalar(field& f, scalar_type st, boost::json::value *v)
{
	boost::json::string *s;
	switch (st) {
	case cp_bool:
		f.data = variant(*(v->if_bool()));
		f.vp = &std::get< bool >(f.data);
		f.count = 1;
		return;
	case cp_char:
		f.data = variant(static_cast<char>(*(v->if_int64())));
		f.vp = &std::get< char >(f.data);
		f.count = 1;
		return;
	case cp_char16:
		f.data = variant(static_cast<char16_t>(*(v->if_uint64())));
		f.vp = &std::get< char16_t >(f.data);
		f.count = 1;
		return;
	case cp_char32:
		f.data = variant(static_cast<char32_t>(*(v->if_uint64())));
		f.vp = &std::get< char32_t >(f.data);
		f.count = 1;
		return;
	case cp_uint8:
		f.data = variant(static_cast<uint8_t>(*(v->if_uint64())));
		f.vp = &std::get< uint8_t >(f.data);
		f.count = 1;
		return;
	case cp_uint16:
		f.data = variant(static_cast<uint16_t>(*(v->if_uint64())));
		f.vp = &std::get< uint16_t >(f.data);
		f.count = 1;
		return;
	case cp_uint32:
		f.data = variant(static_cast<uint32_t>(*(v->if_uint64())));
		f.vp = &std::get< uint32_t >(f.data);
		f.count = 1;
		return;
	case cp_uint64:
		s = v->if_string();
		if (s) {
			uint64_t u64;
			std::string ss(*s);
			std::istringstream iss(ss);
			iss >> u64;
			f.data = variant(u64);
			f.vp = &std::get< uint64_t >(f.data);
			f.count = 1;
		}
		return;
	case cp_int8:
		f.data = variant(static_cast<int8_t>(*(v->if_int64())));
		f.vp = &std::get< int8_t >(f.data);
		f.count = 1;
		return;
	case cp_int16:
		f.data = variant(static_cast<int16_t>(*(v->if_int64())));
		f.vp = &std::get< int16_t >(f.data);
		f.count = 1;
		return;
	case cp_int32:
		f.data = variant(static_cast<int32_t>(*(v->if_int64())));
		f.vp = &std::get< int32_t >(f.data);
		f.count = 1;
		return;
	case cp_int64:
		f.data = variant(*(v->if_int64()));
		f.vp = &std::get< int64_t >(f.data);
		f.count = 1;
		return;
	case cp_epoch:
		f.data = variant(*(v->if_int64()));
		f.vp = &std::get< int64_t >(f.data);
		f.count = 1;
		return;
	// char *
	// fallthrough block for all string variants
	case cp_cstr:
	case cp_json_str:
	case cp_yaml_str:
	case cp_xml_str:
	case cp_json:
	case cp_path:
	case cp_number_str:
		s = v->if_string();
		if (s) {
#ifdef ADC_GV_STR_DEBUG
			std::cerr << "s c ptr     " << (void*)(s->c_str()) << std::endl;
#endif
			std::string ss(*s);
#ifdef ADC_GV_STR_DEBUG
			std::cerr << "ss c ptr    " << (void*)(ss.c_str()) << std::endl;
#endif
			//f.data = variant(ss);
			f.data = ss;
			//f.vp = (&std::get< std::string >(f.data))->data();
#ifdef ADC_GV_STR_DEBUG
			std::cerr << "f ptr       " << (void*)&f.kt << std::endl;
			std::cerr << "f.data      " << (void*)&std::get< std::string >(f.data) << std::endl;
			std::cerr << "f.data>data " << (void*)std::get< std::string >(f.data).data() << std::endl;
#endif
			f.vp = (&std::get< std::string >(f.data))->data();
#ifdef ADC_GV_STR_DEBUG
			std::cerr << "f.vp        " << f.vp << std::endl;
#endif
			f.count = s->size();
		}
		return;
	case cp_f32:
		f.data = variant( static_cast<float>(*(v->if_double())));
		f.vp = &std::get< float >(f.data);
		f.count = 1;
		return;
	case cp_f64:
		f.data = variant( *(v->if_double()));
		f.vp =  &std::get< double >(f.data);
		f.count = 1;
		return;
#if ADC_SUPPORT_EXTENDED_FLOATS
	case cp_f80:
	// prec fixme cp_F80 get_value
		return;
#endif
#if ADC_SUPPORT_QUAD_FLOATS
	case cp_f128:
	// prec fixme cp_F128 get_value
		return;
#endif
#if ADC_SUPPORT_GPU_FLOATS
	// prec fixme cp_Fx gpufloats get_value
	case cp_f8_e4m3:
	case cp_f8_e5m2:
	case cp_f16_e5m10:
	case cp_f16_e8m7:
		return;
#endif
	case cp_c_f32: {
		std::complex<float> cv(0,0);
		if (v->is_array()) {
			float re = 0, im = 0;
			auto a = v->as_array();
			if (a.size() == 2 &&
				a[0].is_double() && a[1].is_double() ) {
				re = static_cast<float>(a[0].as_double());
				im = static_cast<float>(a[1].as_double());
				cv = { re, im};
			}
		}
		f.data = variant(cv) ;
		f.vp = &std::get< std::complex<float> >(f.data);
		f.count = 1;
	}
		return;
	case cp_c_f64: {
		std::complex<double> cv(0,0);
		if (v->is_array()) {
			double re = 0, im = 0;
			auto a = v->as_array();
			if (a.size() == 2 &&
				a[0].is_double() &&
				a[1].is_double() ) {
				re = a[0].as_double();
				im = a[1].as_double();
				cv = { re, im};
			}
		}
		f.data = variant(cv) ;
		f.vp = &std::get< std::complex<double> >(f.data);
		f.count = 1;
	}
		return;
#if ADC_SUPPORT_EXTENDED_FLOATS
	case cp_c_f80:
	// prec fixme cp_c_F80 get_value
#endif
#if ADC_SUPPORT_QUAD_FLOATS
	case cp_c_f128:
	// prec fixme cp_c_F128 get_value
#endif
	case cp_timespec:
	// fallthrough
	case cp_timeval: {
		std::array<int64_t, 2> av = {0,0};
		if (v->is_array()) {
			int64_t sec = 0, subsec = 0;
			auto a = v->as_array();
			if (a.size() == 2 &&
				a[0].is_int64() &&
				a[1].is_int64() ) {
				sec = a[0].as_int64();
				subsec = a[1].as_int64();
				av = { sec, subsec};
			}
		}
		f.data = variant(av);
		f.vp = &std::get< std::array<int64_t,2> >(f.data);
		f.count = 1;
	}
		return;
	default:
		return;
	}
}

/* copy all elements of a matching type st into matching positions
 * of a shared array. type mismatches are silently ignored, their
 * values being converted to 0.
 *
 * As we are querying arrays we built, there should never be a mismatch.
 */
template<typename T>
static void fill_array(field& f, scalar_type st, boost::json::array& a) {
	auto a_len = a.size();
	//c++20 std::shared_ptr<T[]> sa = std::make_shared<T[]>(a_len); 
	std::shared_ptr<T[]> sa(new T[a_len]); 
	size_t i;
	auto json_type = scalar_type_representation(st);
	for (i = 0; i < a_len; i++) {
		if ( a[i].kind() == json_type) {
			boost::system::error_code ec;
			T x = a[i].to_number<T>(ec);
			if (!ec.failed()) {
				sa[i] = x;
			} else {
				sa[i] = 0;
			}
		} else {
			sa[i] = 0;
		}
	}
	f.data = variant( sa ) ;
	f.vp = (std::get< std::shared_ptr<T[]> >(f.data)).get();
	f.count = a_len;
}; 

static void fill_array_u64(field& f, boost::json::array& a) {
	auto a_len = a.size();
	//c++20 std::shared_ptr<uint64_t[]> sa = std::make_shared<uint64_t[]>(a_len); 
	std::shared_ptr<uint64_t[]> sa(new uint64_t[a_len]); 
	size_t i;
	for (i = 0; i < a_len; i++) {
		boost::json::string *s = a[i].if_string();
		if ( s ) {
			std::string ss (*s);
			std::istringstream iss(ss);
			uint64_t x;
			iss >> x; 
			if (!iss.fail()) {
				sa[i] = x;
			} else {
				sa[i] = 0;
			}
		} else {
			sa[i] = 0;
		}
	}
	f.data = variant( sa ) ;
	f.vp = (std::get< std::shared_ptr<uint64_t[]> >(f.data)).get();
	f.count = a_len;
}; 

/* copy pairs of a matching type st into matching positions
 * of a shared array of complex. type mismatches are silently ignored, their
 * values being converted to 0.
 *
 * As we are querying arrays we built, there should never be a mismatch.
 */
template<typename T>
static void fill_array_complex(field& f, boost::json::array& a) {
	auto a_len = a.size();
	// c++20: std::shared_ptr<std::complex<T>[]> sa = std::make_shared<std::complex<T>[]>(a_len); 
	std::shared_ptr<std::complex<T>[]> sa(new std::complex<T>[a_len]); 
	size_t i;
	for (i = 0; i < a_len; i++) {
		if ( a[i].kind() == boost::json::kind::array) {
			auto pair = a[i];
			if (pair.as_array().size() != 2) {
				sa[i] = { 0, 0 };
				continue;
			}
			boost::system::error_code ecr;
			boost::system::error_code eci;
		       	T re = pair.as_array()[0].to_number<T>(ecr);
		       	T im = pair.as_array()[1].to_number<T>(eci);
			if (!(ecr.failed() || eci.failed())) {
				sa[i] = { re, im };
			} else {
				sa[i] = { 0, 0 };
			}
		} else {
			sa[i] = { 0, 0 };
		}
	}
	f.data = variant( sa ) ;
	f.vp = (std::get< std::shared_ptr<std::complex<T>[]> >(f.data)).get();
	f.count = a_len;
};

/* expects a json array of boolean values. any non-boolean value is
 * mapped to false in the output array.
 */
void fill_array_bool(field& f, boost::json::array& a) {
	auto a_len = a.size();
	//c++20 std::shared_ptr<T[]> sa = std::make_shared<T[]>(a_len); 
	std::shared_ptr<bool[]> sa(new bool[a_len]); 
	size_t i;
	for (i = 0; i < a_len; i++) {
		bool* bptr = a[i].if_bool();
		if (bptr) {
			sa[i] = *bptr;
		} else {
			sa[i] = false;
		}
	}
	f.data = variant( sa ) ;
	f.vp = (std::get< std::shared_ptr<bool[]> >(f.data)).get();
	f.count = a_len;
}

void fill_array_string(field& f, boost::json::array& a) {
	auto a_len = a.size();
	//c++20 std::shared_ptr<T[]> sa = std::make_shared<T[]>(a_len); 
	std::shared_ptr<std::string[]> sa(new std::string[a_len]); 
	size_t i;
	for (i = 0; i < a_len; i++) {
		boost::json::string* sptr = a[i].if_string();
		if (sptr) {
			sa[i] = std::string(sptr->c_str());
		} else {
			sa[i] = std::string("");
		}
	}
	f.data = variant( sa ) ;
	f.vp = (std::get< std::shared_ptr<std::string[]> >(f.data)).get();
	f.count = a_len;
}

static void get_array(field& f, scalar_type st, boost::json::value *v)
{
	if (!v->is_array()) {
		return;
	}

	auto a = v->as_array();
	switch (st) {
	case cp_bool:
		fill_array_bool(f, a);
		return;
	case cp_char:
		fill_array<char>(f, st, a);
		return;
	case cp_char16:
		fill_array<char16_t>(f, st, a);
		return;
	case cp_char32:
		fill_array<char32_t>(f, st, a);
		return;
	case cp_uint8:
		fill_array<uint8_t>(f, st, a);
		return;
	case cp_uint16:
		fill_array<uint16_t>(f, st, a);
		return;
	case cp_uint32:
		fill_array<uint32_t>(f, st, a);
		return;
	case cp_uint64:
		fill_array_u64(f, a);
		return;
	case cp_int8:
		fill_array<int8_t>(f, st, a);
		return;
	case cp_int16:
		fill_array<int16_t>(f, st, a);
		return;
	case cp_int32:
		fill_array<int32_t>(f, st, a);
		return;
	case cp_int64:
		fill_array<int64_t>(f, st, a);
		return;
	// fallthrough block for all string variants
	case cp_cstr:
	case cp_json_str:
	case cp_yaml_str:
	case cp_xml_str:
	case cp_json:
	case cp_path:
	case cp_number_str:
		fill_array_string(f, a);
		return;
	case cp_f32:
		fill_array<float>(f, st, a);
		return;
	case cp_f64:
		fill_array<double>(f, st, a);
		return;
#if ADC_SUPPORT_EXTENDED_FLOATS
	case cp_f80:
	// prec fixme cp_F80 get_value
		return;
#endif
#if ADC_SUPPORT_QUAD_FLOATS
	case cp_f128:
	// prec fixme cp_F128 get_value
		return;
#endif
#if ADC_SUPPORT_GPU_FLOATS
	// prec fixme cp_Fx gpufloats get_value
	case cp_f8_e4m3:
	case cp_f8_e5m2:
	case cp_f16_e5m10:
	case cp_f16_e8m7:
		return;
#endif
	case cp_c_f32:
		fill_array_complex<float>(f, a);
		return;
	case cp_c_f64:
		fill_array_complex<double>(f, a);
		return;
#if ADC_SUPPORT_EXTENDED_FLOATS
	case cp_c_f80:
	// prec fixme cp_c_F80 get_value
#endif
#if ADC_SUPPORT_QUAD_FLOATS
	case cp_c_f128:
	// prec fixme cp_c_F128 get_value
#endif
	default:
		return;
	}
}

const field builder::get_value(std::string_view name)
{
	field f = { k_none, cp_none, NULL, 0, "", variant() };
	f.kt = kind(name);
	if (f.kt != k_value)
		return f;
	auto jit = d[name]; // see also at() and at_pointer()
	if (jit.kind() == boost::json::kind::object) {
		auto obj = jit.as_object();
		auto v = obj.if_contains("value");
		auto type_name_v = obj.if_contains("type");
		std::string type_name;
		if (type_name_v) {
			type_name = *(type_name_v->if_string());
		}
		if (v && type_name.size()) {
			auto c = obj.if_contains("container_type");
			auto st = scalar_type_from_name(type_name);
			if (!c) {
				f.st = st;
				get_scalar(f, st, v);
				return f;
			} else {
				f.st = st;
				f.container = *(c->if_string());
				get_array(f, st, v);
				return f;
			}
		}
	}
	if (jit.kind() == boost::json::kind::array) {
		// untyped json arrays in get_value make no sense
		return f;
	}
	if (jit.kind() == boost::json::kind::object) {
		// object in get_value make no sense
		return f;
	}
	if (jit.kind() == boost::json::kind::string) {
		// bare strings default to cp_cstr
		f.st = cp_cstr;
		f.vp = jit.if_string()->c_str();
		f.count = jit.if_string()->size();
		return f;
	}
	if (jit.kind() == boost::json::kind::bool_) {
		f.st = cp_bool;
		f.vp = jit.if_bool();
		f.count = 1;
		return f;
	}
	if (jit.kind() == boost::json::kind::int64) {
		f.st = cp_int64;
		f.vp = jit.if_int64();
		f.count = 1;
		return f;
	}
	if (jit.kind() == boost::json::kind::uint64) {
		f.st = cp_uint64;
		f.vp = jit.if_uint64();
		f.count = 1;
		return f;
	}
	if (jit.kind() == boost::json::kind::double_) {
		f.st = cp_f64;
		f.vp = jit.if_double();
		f.count = 1;
		return f;
	}
	return f;
}

#if 0
// fixme implement json-path lookup of boost.json items, maybe; builder::get_boost_json_value
// accept paths of /a/b/c
// split to vector walk down sections for prefix
// of a, b, c
// assemble remaining path and call at_pointer.
void *builder::get_boost_json_value(std::string_view path)
{
	auto sit = sections.find(name);
	if (sit != sections.end())
		return k_section;
	auto jit = d.find(name);
	if (jit != d.end())
		return k_value;
	boost::json::value jv = d.at_pointer(path);
}
#endif

void builder::add_mpi_section(std::string_view name, void *mpi_comm_p, adc_mpi_field_flags bitflags)
{
	if (!mpi_comm_p || bitflags == ADC_MPI_NONE)
		return;
	std::string commname = std::string("mpi_comm_") += name;
#ifdef ADC_HAVE_MPI
	if (*(MPI_Comm *)mpi_comm_p == MPI_COMM_NULL)
		return;
	MPI_Comm *comm = (MPI_Comm *)mpi_comm_p;
	boost::json::object jv;
	int err;
	int rank = -1;
	if ( bitflags & ADC_MPI_RANK) {
		err = MPI_Comm_rank(*comm, &rank);
		if (!err) {
			jv["mpi_rank"] = rank;
		}
	}

	int size = -1;
	if ( bitflags & ADC_MPI_SIZE) {
		err = MPI_Comm_size(*comm, &size);
		if (!err) {
			jv["mpi_size"] = size;
		}
	}

	if ( bitflags & ADC_MPI_NAME) {
		char name[MPI_MAX_OBJECT_NAME];
		int len = 0;
		err = MPI_Comm_get_name(*comm, name, &len);
		if (!err) {
			jv["mpi_name"] = name;
		}
	}

	int major=0, minor=0;
	if ( bitflags & ADC_MPI_VER) {
		err = MPI_Get_version(&major, &minor);
		std::ostringstream mversion;
		mversion << MPI_VERSION << "." << MPI_SUBVERSION;
		if (!err) {
			jv["mpi_version"] = mversion.str();
		}
	}

	err = 0;
	if ( bitflags & ADC_MPI_LIB_VER) {
		std::ostringstream lversion;
#ifdef OMPI_VERSION
#define USE_set_lib_version
		lversion << "OpenMPI " << OMPI_MAJOR_VERSION << "." <<
			OMPI_MINOR_VERSION << "." << OMPI_RELEASE_VERSION;
		goto set_lib_version;
#else
#ifdef MVAPICH2_VERSION
#define USE_set_lib_version
		lversion << MVAPICH2_VERSION;
		goto set_lib_version;
#else
#ifdef MPICH_VERSION
#define USE_set_lib_version
		lversion << MPICH_VERSION;
		goto set_lib_version;
#endif
#endif
#endif
		char lv[MPI_MAX_LIBRARY_VERSION_STRING];
		int sz = 0;
		err = MPI_Get_library_version(lv, &sz);
		lversion << lv;
#ifdef USE_set_lib_version
set_lib_version:
#endif
		if (!err) {
			jv["mpi_library_version"] = lversion.str();
		}
	}

	if (bitflags & (ADC_MPI_HOSTLIST | ADC_MPI_RANK_HOST)) {
		if (rank < 0) {
			err = MPI_Comm_rank(*comm, &rank);
			if (err)
				goto mpi_out;
		}
		if (size < 0) {
			err = MPI_Comm_size(*comm, &size);
			if (err)
				goto mpi_out;
		}
		char *hostnames = (char *)calloc(size*MPI_MAX_PROCESSOR_NAME, 1); 
		int nlen;
		MPI_Get_processor_name(hostnames + rank * MPI_MAX_PROCESSOR_NAME, &nlen);
		MPI_Allgather(hostnames + rank * MPI_MAX_PROCESSOR_NAME,
			    	MPI_MAX_PROCESSOR_NAME, 
				MPI_CHAR,
				hostnames,
				MPI_MAX_PROCESSOR_NAME,
				MPI_CHAR,
				*comm);

		std::vector<std::string> rh;
		for (auto i = 0; i < size; i++)
			rh.push_back(std::string(hostnames + rank * MPI_MAX_PROCESSOR_NAME));

		if (bitflags & ADC_MPI_RANK_HOST) {
			boost::json::array av(rh.begin(), rh.end());
			jv["mpi_rank_host"] = av;
		}

		if (bitflags & ADC_MPI_HOSTLIST ) {
			std::vector<std::string> hl;
			std::set<std::string> hm;
			for (auto i = 0; i < size; i++)
				hm.insert(std::string(hostnames + rank * MPI_MAX_PROCESSOR_NAME));
			for (auto i = 0; i < size; i++) {
				std::string stmp(hostnames + rank * MPI_MAX_PROCESSOR_NAME);
				if (hm.count(stmp)) {
					hl.push_back(stmp);
					hm.erase(stmp);
				}
			}
			boost::json::array hv(hl.begin(), hl.end());
			jv["mpi_hostlist"] = hv;
		}

		free(hostnames);
	}

mpi_out:

#else
	// add fake rank 0, size 1, versions and names "none" per bitflags
	boost::json::object jv;
	int rank = 0;
	if ( bitflags & ADC_MPI_RANK) {
		jv["mpi_rank"] = rank;
	}

	int size = 1;
	if ( bitflags & ADC_MPI_SIZE) {
		jv["mpi_size"] = size;
	}

	if ( bitflags & ADC_MPI_NAME) {
		jv["mpi_name"] = "none";
	}

	if ( bitflags & ADC_MPI_VER) {
		jv["mpi_version"] = "none";
	}

	if ( bitflags & ADC_MPI_LIB_VER) {
		jv["mpi_library_version"] = "none";
	}

	if (bitflags & (ADC_MPI_HOSTLIST | ADC_MPI_RANK_HOST)) {
		char myhost[HOST_NAME_MAX];
		int herr = gethostname(myhost, HOST_NAME_MAX);
		if (herr < 0)
			sprintf(myhost, "name_unavailable");

		std::vector<std::string> rh;
		rh.push_back(myhost);
		boost::json::array av(rh.begin(), rh.end());

		if (bitflags & ADC_MPI_RANK_HOST) {
			jv["mpi_rank_host"] = av;
		}

		if (bitflags & ADC_MPI_HOSTLIST ) {
			jv["mpi_hostlist"] = av;
		}
	}
#endif // ADC_HAVE_MPI
	d[commname] = jv;
}

void builder::add_slurm_section()
{
	std::vector< std::string >slurmvars;
	add_slurm_section(slurmvars);
}

void builder::add_slurm_section(const std::vector< std::string >& slurmvars)
{
	static std::vector< std::pair<const char *, const char *> > slurm_names =
	{
		{ "cluster", "SLURM_CLUSTER_NAME"},
		{ "job_id", "SLURM_JOB_ID"},
		{ "num_nodes", "SLURM_JOB_NUM_NODES"},
		{ "dependency", "SLURM_JOB_DEPENDENCY"},
	};

	boost::json::object jv;
	for (const auto& i : slurm_names) {
		const char *val = getenv(i.second);
		if (val)
			jv[i.first] = val;
		else
			jv[i.first] = "";
	}
	for (const auto& i : slurmvars) {
		const char *val = getenv(i.c_str());
		if (val)
			jv[i] = val;
		else
			jv[i] = "";
	}
	d["slurm"] = jv;
}

void builder::add_workflow_section()
{
	static std::vector< std::pair<const char *, const char *> > names =
	{
		{ "wfid", "ADC_WFID"},
		{ "wfid_parent", "ADC_WFID_PARENT"},
		{ "wfid_path", "ADC_WFID_PATH"}
	};

	boost::json::object jv;
	for (const auto& i : names) {
		const char *val = getenv(i.second);
		if (val)
			jv[i.first] = val;
		else
			jv[i.first] = "";
	}
	boost::json::array av;
	const char *cenv = getenv("ADC_WFID_CHILDREN");
	if (cenv) {
		// split and append to wfid_children
		std::stringstream ss(cenv);
		string elt;
		char pathsep = ':';
		while (getline(ss, elt, pathsep)) {
			if (elt.size() > 0) {
#ifdef ADC_WORKFLOW_DEBUG
				std::cerr << __func__ << ": add child " << elt << std::endl;
#endif
				av.emplace_back(elt);
			}
		}
	}
	jv["wfid_children"] = av;
	d["adc_workflow"] = jv;
}

bool array_contains_string( boost::json::array& av, string_view uuid)
{
	for (const auto& v : av) {
		if (v.is_string() && v.as_string() == uuid) {
			return true;
		}
	}
	return false;
}

void builder::add_workflow_children(std::vector< std::string >& child_uuids)
{
	auto wsection = d.if_contains("adc_workflow");
	if (! wsection) {
		std::cerr << __func__ << ": called before add_workflow_section" << std::endl;
		return;
	}
	if (! wsection->is_object()) {
		std::cerr << __func__ << ": adc_workflow is not an object. (?!)" << std::endl;
		return;
	}
	auto haschildren = wsection->as_object().if_contains("wfid_children");
	if (!haschildren) {
		std::cerr << __func__ << ": called before successful add_workflow_section" << std::endl;
		return;
	}
	auto children = wsection->as_object()["wfid_children"];
	if (!children.is_array()) {
		std::cerr << __func__ << ": wfid_children is not an array. (?!)" << std::endl;
		return;
	}
	for (auto const& uuid : child_uuids) {
		if (array_contains_string(wsection->as_object()["wfid_children"].as_array(), uuid)) {
			continue;
		}
		wsection->as_object()["wfid_children"].as_array().emplace_back(uuid);
	}
}


void builder::add_gitlab_ci_section()
{
	static const std::vector<std::string > gitlab_ci_names = {
		"CI_RUNNER_ID",
		"CI_RUNNER_VERSION",
		"CI_PROJECT_ID",
		"CI_PROJECT_NAME",
		"CI_SERVER_FQDN",
		"CI_SERVER_VERSION",
		"CI_JOB_ID",
		"CI_JOB_STARTED_AT",
		"CI_PIPELINE_ID",
		"CI_PIPELINE_SOURCE",
		"CI_COMMIT_SHA",
		"GITLAB_USER_LOGIN"
	};
	boost::json::object jv;
	for (const auto& i : gitlab_ci_names) {
		const char *val = getenv(i.c_str());
		if (val)
			jv[tolower_s(i)] = val;
		else
			jv[tolower_s(i)] = "";
	}
	d["gitlab_ci"] = jv;
}

void builder::add(std::string_view name, bool value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_bool)},
		{"value", value}
	};
	d[name] = jv;
}

void builder::add(std::string_view name, char value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_char)},
		{"value", value }
	};
	d[name] = jv;
}

void builder::add(std::string_view name, char16_t value) {
	uint64_t ivalue = value;
	boost::json::value jv = {
		{"type", adc::to_string(cp_char16)},
		{"value", ivalue}
	};
	d[name] = jv;
}

void builder::add(std::string_view name, char32_t value) {
	uint64_t ivalue = value;
	boost::json::value jv = {
		{"type", adc::to_string(cp_char32)},
		{"value", ivalue}
	};
	d[name] = jv;
}

// builder::add null-terminated string
void builder::add(std::string_view name, char* value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_cstr)},
		{"value", value}
	};
	d[name] = jv;
}
void builder::add(std::string_view name, const char* value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_cstr)},
		{"value", value}
	};
	d[name] = jv;
}
void builder::add(std::string_view name, std::string_view value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_cstr)},
		{"value", value}
	};
	d[name] = jv;
}
void builder::add(std::string_view name, std::string& value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_cstr)},
		{"value", value}
	};
	d[name] = jv;
}

// builder::add null-terminated string file path
void builder::add_path(std::string_view name, char* value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_path)},
		{"value", value}
	};
	d[name] = jv;
}
void builder::add_path(std::string_view name, const char* value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_path)},
		{"value", value}
	};
	d[name] = jv;
}
void builder::add_path(std::string_view name, std::string_view value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_path)},
		{"value", value}
	};
	d[name] = jv;
}
void builder::add_path(std::string_view name, std::string& value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_path)},
		{"value", value}
	};
	d[name] = jv;
}

// builder::add string which is serialized json.
void builder::add_json_string(std::string_view name, std::string_view value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_json_str)},
		{"value", value}
	};
	d[name] = jv;
}

void builder::add_yaml_string(std::string_view name, std::string_view value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_yaml_str)},
		{"value", value}
	};
	d[name] = jv;
}

void builder::add_xml_string(std::string_view name, std::string_view value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_xml_str)},
		{"value", value}
	};
	d[name] = jv;
}

void builder::add_number_string(std::string_view name, std::string_view value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_number_str)},
		{"value", value}
	};
	d[name] = jv;
}

#if ADC_BOOST_JSON_PUBLIC
void builder::add(std::string_view name, boost::json::value value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_json)},
		{"value", value}
	};
	d[name] = jv;
}
#endif


void builder::add(std::string_view name, uint8_t value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_uint8)},
		{"value", value}
	};
	d[name] = jv;
}
void builder::add(std::string_view name, uint16_t value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_uint16)},
		{"value", value}
	};
	d[name] = jv;
}
void builder::add(std::string_view name, uint32_t value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_uint32)},
		{"value", value}
	};
	d[name] = jv;
}
void builder::add(std::string_view name, uint64_t value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_uint64)},
		{"value", std::to_string(value)}
	};
	d[name] = jv;
}
void builder::add(std::string_view name, int8_t value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_int8)},
		{"value", value}
	};
	d[name] = jv;
}
void builder::add(std::string_view name, int16_t value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_int16)},
		{"value", value}
	};
	d[name] = jv;
}
void builder::add(std::string_view name, int32_t value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_int32)},
		{"value", value }
	};
	d[name] = jv;
}
void builder::add(std::string_view name, int64_t value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_int64)},
		{"value", value}
	};
	d[name] = jv;
}
void builder::add(std::string_view name, float value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_f32)},
		{"value", value }
	};
	d[name] = jv;
}
void builder::add(std::string_view name, const std::complex<float>& value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_c_f32)},
		{"value", { value.real(), value.imag() }}
	};
	d[name] = jv;
}
void builder::add(std::string_view name, double value) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_f64)},
		{"value", value }
	};
	d[name] = jv;
}
void builder::add(std::string_view name, const std::complex<double>& value) {
	boost::json::value jv = {
		{"type" , adc::to_string(cp_c_f64)},
		{"value", { value.real(), value.imag() }}
	};
	d[name] = jv;
}


void builder::add(std::string_view name, const struct timeval& tv) {
	boost::json::value jv = {
		{"type" , adc::to_string(cp_timeval)},
		{"value", { (int64_t)tv.tv_sec, (int64_t)tv.tv_usec }}
	};
	d[name] = jv;
}

void builder::add(std::string_view name, const struct timespec& ts) {
	boost::json::value jv = {
		{"type" , adc::to_string(cp_timespec)},
		{"value", { (int64_t)ts.tv_sec, (int64_t)ts.tv_nsec }}
	};
	d[name] = jv;
}

void builder::add_epoch(std::string_view name, int64_t epoch) {
	boost::json::value jv = {
		{"type", adc::to_string(cp_epoch)},
		{"value", epoch }
	};
	d[name] = jv;
}


void builder::add_from_pointer_type(std::string_view name, void* p, enum scalar_type t)
{
	switch (t) {
	case cp_bool:
		{
			bool *v = (bool *)p;
			add(name, *v);
		}
		break;
	case cp_char:
		{
			char *v = (char *)p;
			add(name, *v);
		}
		break;
	case cp_char16:
		{
			char16_t *v = (char16_t *)p;
			add(name, *v);
		}
		break;
	case cp_char32:
		{
			char32_t *v = (char32_t *)p;
			add(name, *v);
		}
		break;
	case cp_cstr:
		{
			char *v = (char *)p;
			add(name, v);
		}
		break;
	case cp_json_str:
		{
			char *v = (char *)p;
			add_json_string(name, v);
		}
		break;
	case cp_yaml_str:
		{
			char *v = (char *)p;
			add_yaml_string(name, v);
		}
		break;
	case cp_xml_str:
		{
			char *v = (char *)p;
			add_xml_string(name, v);
		}
		break;
#if ADC_BOOST_JSON_PUBLIC
	case cp_json:
			return; // pointers to json objects are ignored. fixme? ADC_BOOST_JSON_PUBLIC in builder::add_from_pointer_type
#endif
	case cp_path:
		{
			char *v = (char *)p;
			add_path(name, v);
		}
		break;
	case cp_number_str:
		{
			char *v = (char *)p;
			add_number_string(name, v);
		}
		break;
	case cp_uint8:
		{
			uint8_t *v = (uint8_t *)p;
			add(name, *v);
		}
		break;
	case cp_uint16:
		{
			uint16_t *v = (uint16_t *)p;
			add(name, *v);
		}
		break;
	case cp_uint32:
		{
			uint32_t *v = (uint32_t *)p;
			add(name, *v);
		}
		break;
	case cp_uint64:
		{
			uint64_t *v = (uint64_t *)p;
			add(name, *v);
		}
		break;
	case cp_int8:
		{
			int8_t *v = (int8_t *)p;
			add(name, *v);
		}
		break;
	case cp_int16:
		{
			int16_t *v = (int16_t *)p;
			add(name, *v);
		}
		break;
	case cp_int32:
		{
			int32_t *v = (int32_t *)p;
			add(name, *v);
		}
		break;
	case cp_int64:
		{
			int64_t *v = (int64_t *)p;
			add(name, *v);
		}
		break;
	case cp_f32:
		{
			float *v = (float *)p;
			add(name, *v);
		}
		break;
	case cp_f64:
		{
			double *v = (double *)p;
			add(name, *v);
		}
		break;
#if ADC_SUPPORT_EXTENDED_FLOATS
	case cp_f80:
		{
			// prec fixme? verify cp_f80 in supporting compiler builder::add_from_pointer_type
			__float80 *v = (__float80 *)p;
			add(name, *v);
		}
		break;
#endif
#if ADC_SUPPORT_QUAD_FLOATS
	case cp_f128:
		{
			__float128 *v = (__float128 *)p;
			add(name, *v);
		}
		break;
#endif
#if ADC_SUPPORT_GPU_FLOATS
	case cp_f8_e4m3:
		{
			// prec fixme verify cp_f8_e4m3 in supporting compiler ; builder::add_from_pointer_type
		}
		break;
	case cp_f8_e5m2:
		{
			// prec fixme verify cp_f8_e5m2 in supporting compiler ; builder::add_from_pointer_type
		}
		break;
	case cp_f16_e5m10:
		{
			// prec fixme verify cp_f16_e5m10 in supporting compiler ; builder::add_from_pointer_type
		}
		break;
	case cp_f16_e8m7:
		{
			// prec fixme verify cp_f16_e8m7 in supporting compiler ; builder::add_from_pointer_type
		}
		break;
#endif
	case cp_c_f32:
		{
			float *v = (float *)p;
			std::complex<float> c(v[0], v[1]);
			add(name, c);
		}
		break;
	case cp_c_f64:
		{
			double *v = (double *)p;
			std::complex<double> c(v[0], v[1]);
			add(name, c);
		}
		break;
#if ADC_SUPPORT_QUAD_FLOATS
	case cp_c_f128:
		{
			__float128 *v = (__float128 *)p;
			std::complex<__float128> c(v[0], v[1]);
			add(name, c);
		}
		break;
#endif
#if ADC_SUPPORT_EXTENDED_FLOATS
	case cp_c_f80:
		{
			// prec fixme? cp_c_f80 ; builder::add_from_pointer_type
			__float80 *v = (__float80 *)p;
			std::complex<__float80> c(v[0], v[1]);
			add(name, c);
		}
		break;
#endif
	case cp_timespec:
		{
			struct timespec *v = (struct timespec *)p;
			add(name, *v);
		}
		break;
	case cp_timeval:
		{
			struct timeval *v = (struct timeval *)p;
			add(name, *v);
		}
		break;
	case cp_epoch:
		{
			int64_t *v = (int64_t *)p;
			add_epoch(name, *v);
		}
		break;
	default:
		break;
	}
}

void builder::add_array(std::string_view name, bool value[], size_t len, std::string_view c) {
	boost::json::array av(value, value+len);
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_bool)},
		{"container_type", c},
		{"value", av}
	};
	d[name] = jv;
}

void builder::add_array(std::string_view name, const char *value, size_t len, std::string_view c) {
	boost::json::array av(value, value+len);
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_char)},
		{"container_type", c},
		{"value", av}
	};
	d[name] = jv;
}

void builder::add_array(std::string_view name, char16_t value[], size_t len, std::string_view c) {
	boost::json::array av(value, value+len);
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_char16)},
		{"container_type", c},
		{"value", av}
	};
	d[name] = jv;
}

void builder::add_array(std::string_view name, char32_t value[], size_t len, std::string_view c) {
	boost::json::array av(value, value+len);
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_char32)},
		{"container_type", c},
		{"value", av}
	};
	d[name] = jv;
}

void builder::add_array(std::string_view name, uint8_t value[], size_t len, std::string_view c) {
	boost::json::array av(value, value+len);
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_uint8)},
		{"container_type", c},
		{"value", av}
	};
	d[name] = jv;
}
void builder::add_array(std::string_view name, uint16_t value[], size_t len, std::string_view c) {
	boost::json::array av(value, value+len);
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_uint16)},
		{"container_type", c},
		{"value", av}
	};
	d[name] = jv;
}
void builder::add_array(std::string_view name, uint32_t value[], size_t len, std::string_view c) {
	boost::json::array av(value, value+len);
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_uint32)},
		{"container_type", c},
		{"value", av}
	};
	d[name] = jv;
}
void builder::add_array(std::string_view name, uint64_t value[], size_t len, std::string_view c) {
	std::vector<std::string> sv(len);
	for (size_t i = 0; i < len; i++)
		sv[i] = std::to_string(value[i]);
	boost::json::array av(sv.begin(), sv.end());
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_uint64)},
		{"container_type", c},
		{"value", av}
	};
	d[name] = jv;
}
void builder::add_array(std::string_view name, int8_t value[], size_t len, std::string_view c) {
	boost::json::array av(value, value+len);
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_int8)},
		{"container_type", c},
		{"value", av}
	};
	d[name] = jv;
}
void builder::add_array(std::string_view name, int16_t value[], size_t len, std::string_view c) {
	boost::json::array av(value, value+len);
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_int16)},
		{"container_type", c},
		{"value", av}
	};
	d[name] = jv;
}
void builder::add_array(std::string_view name, int32_t value[], size_t len, std::string_view c) {
	boost::json::array av(value, value+len);
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_int32)},
		{"container_type", c},
		{"value", av}
	};
	d[name] = jv;
}
void builder::add_array(std::string_view name, int64_t value[], size_t len, std::string_view c) {
	boost::json::array av(value, value+len);
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_int64)},
		{"container_type", c},
		{"value", av}
	};
	d[name] = jv;
}
void builder::add_array(std::string_view name, float value[], size_t len, std::string_view c) {
	boost::json::array av(value, value+len);
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_f32)},
		{"container_type", c},
		{"value", av}
	};
	d[name] = jv;
}
#if 0
void builder::add_array(std::string_view name, const std::complex<float> value[], size_t len, std::string_view c) {
	boost::json::array av(value, value+len);
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_c_f32)},
		{"container_type", c},
		{"value", { value.real(), value.imag() }}
	};
	d[name] = jv;
}
#endif
void builder::add_array(std::string_view name, double value[], size_t len, std::string_view c) {
	boost::json::array av(value, value+len);
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_f64)},
		{"container_type", c},
		{"value", av}
	};
	d[name] = jv;
}
#if 0
void builder::add_array(std::string_view name, const std::complex<double> value[], size_t len, std::string_view c) {
	boost::json::array av(value, value+len);
	boost::json::value jv = {
		{"type" , "array_" + adc::to_string(cp_c_f64)},
		{"container_type", c},
		{"value", { value.real(), value.imag() }}
	};
	d[name] = jv;
}
#endif

void builder::add_array(std::string_view name, char* value[], size_t len, std::string_view c) {
	boost::json::array av(value, value+len);
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_cstr)},
		{"container_type", c},
		{"value", av}
	};
	d[name] = jv;
}
void builder::add_array(std::string_view name, const char* value[], size_t len, std::string_view c) {
	boost::json::array av(value, value+len);
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_cstr)},
		{"container_type", c},
		{"value", av}
	};
	d[name] = jv;
}
void builder::add_array(std::string_view name, std::string value[], size_t len, std::string_view c) {
	boost::json::array av(value, value+len);
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_cstr)},
		{"container_type", c},
		{"value", av}
	};
	d[name] = jv;
}
void builder::add_array(std::string_view name, const std::string value[], size_t len, std::string_view c) {
	boost::json::array av(value, value+len);
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_cstr)},
		{"container_type", c},
		{"value", av}
	};
	d[name] = jv;
}
void builder::add_array(std::string_view name, const std::vector<std::string> value, std::string_view c) {
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_cstr)},
		{"container_type", c},
		{"value", boost::json::value_from(value)}
	};
	d[name] = jv;
}
void builder::add_array(std::string_view name, const std::set<std::string> value, std::string_view c) {
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_cstr)},
		{"container_type", c},
		{"value", boost::json::value_from(value)}
	};
	d[name] = jv;
}
void builder::add_array(std::string_view name, const std::list<std::string> value, std::string_view c) {
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_cstr)},
		{"container_type", c},
		{"value", boost::json::value_from(value)}
	};
	d[name] = jv;
}
// Array of strings which are serialized json.
void builder::add_array_json_string(std::string_view name, const std::string value[], size_t len, std::string_view c) {
	boost::json::array av(value, value+len);
	boost::json::value jv = {
		{"type", "array_" + adc::to_string(cp_json_str)},
		{"container_type", c},
		{"value", av}
	};
	d[name] = jv;
}


/*
 * recursively merge sections into a single json tree
 */
boost::json::object builder::flatten()
{
	boost::json::object tot(d); // copy
	for (auto it = sections.begin(); it != sections.end(); it++) {
		tot[it->first] = it->second->flatten();
	}
	return tot;
}

BOOST_SYMBOL_VISIBLE std::string builder::serialize() {
	boost::json::object total;
	total = flatten();
	return boost::json::serialize(total);
}

key_type builder::kind(std::string_view name) {
	auto sit = sections.find(std::string(name));
	if (sit != sections.end())
		return k_section;
	auto jit = d.find(name);
	if (jit != d.end())
		return k_value;
	return k_none;
}

} // end namespace adc
