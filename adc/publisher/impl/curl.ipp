#include <adc/builder/builder.hpp>
#include <adc/publisher/publisher.hpp>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <unistd.h>

namespace adc {

using std::cout;

typedef std::string string;
typedef std::string_view string_view;

/*! \brief Curl utility publisher_api implementation.
  This plugin generates a scratch file (in-memory) and asynchronously
  sends it to the configured web service by invoking the 'curl' utility.
  Multiple independent instances of this plugin may be used simultaneously.
 */
class curl_plugin : public publisher_api {
	enum state {
		ok,
		err
	};
	enum mode {
		/* the next mode needed for correct operation */
		pi_config,
		pi_init,
		pi_pub_or_final
	};
	
public:
	/// \brief default scratch directory for curl utility messages.
	/// This directory must be globally writable and should be fast.
	/// Overridden with env("ADC_CURL_PLUGIN_DIRECTORY").
	inline static const char *adc_curl_plugin_directory_default = "/dev/shm/adc";

	/// \brief default full path to curl utility
	/// This program should be world accessible.
	/// Overridden with env("ADC_CURL_PLUGIN_PROG").
	inline static const char *adc_curl_plugin_prog_default = "/usr/bin/curl";

	/// \brief default ADC server https port.
	/// The server should be highly available.
	/// Overridden with env("ADC_CURL_PLUGIN_PORT").
	inline static const char *adc_curl_plugin_port_default = "443";

	/// \brief ADC data ingest server host.
	/// The server should be highly available; the
	/// default value of https://localhost must be overridden by an
	/// environment variable unless the test environment includes a test server.
	/// Overridden with env("ADC_CURL_PLUGIN_URL").
	inline static const char *adc_curl_plugin_url_default = "https://localhost";

	/// \brief ADC curl plugin enable debug messages; (default "0": none)
	/// "2" provides message send debugging
	/// "1" provides lighter debugging
	/// Overridden with env("ADC_CURL_PLUGIN_DEBUG").
	inline static const char *adc_curl_plugin_debug_default = "0";

private:
	inline static const char *plugin_prefix = "ADC_CURL_PLUGIN_";
	inline static const std::map< const string, const string > plugin_config_defaults =
	{       {"DIRECTORY", adc_curl_plugin_directory_default},
		{"PROG", adc_curl_plugin_prog_default},
		{"URL", adc_curl_plugin_url_default},
		{"PORT", adc_curl_plugin_port_default},
		{"DEBUG", adc_curl_plugin_debug_default}
	};

	const string vers;
	const std::vector<string> tags;
	string fdir;
	string prog;
	string port;
	string url;
	int debug;
	enum state state;
	bool paused;
	enum mode mode;

	int config(const string& dir, string_view surl, string_view sport, string_view sprog, const string& sdebug) {
		if (mode != pi_config)
			return 2;

		uid_t ui = geteuid();
		fdir = dir + string("/") + std::to_string(ui);
		port = sport;
		url = surl;
		prog = sprog;
		mode = pi_init;
		std::stringstream ss(sdebug);
		ss >> debug;
		if (debug < 0) {
			debug = 0;
		}
		if (debug > 0) {
			std::cout<< "curl plugin configured" <<std::endl;
		}
		return 0;
	}

	// find field in m, then with prefix in env, then the default.
	const string get(const std::map< string, string >& m,
			string field, string_view env_prefix) {
		// fields not defined in config_defaults raise an exception.
		auto it = m.find(field);
		if (it != m.end()) {
			return it->second;
		}
		string en = string(env_prefix) += field;
		char *ec = getenv(en.c_str());
		if (!ec) {
			return plugin_config_defaults.at(field);
		} else {
			return string(ec);
		}
	}
	
	string get_temp_file_name() {
		string tplt = fdir + "/json-curl-msg-XXXXXX";
		char * cstr = new char [tplt.length()+1];
		std::strcpy (cstr, tplt.c_str());
		int fd = mkstemp(cstr);
		if (fd >= 0) {
			string f(cstr);
			delete[] cstr;
			close(fd);
			// current user now owns tempfile.
			return f;
		}
		std::cout<< "mkstemp failed" <<std::endl;
		delete[] cstr;
		return "";
	}

	// invoke curl with json in f, then delete f.
	// wrap this in a thread to make it non-blocking eventually.
	int curl_send(string& f)
	{
		if (debug == 2) {
			// do not suppress stderr/stdout from system call
			string cmd = "(" + prog + 
				" -X POST -s -w \"\n%{http_code}\n\" -H \"Content-Type: application/json\" -d @"
				+ f + " " + url + " || /bin/rm -f " + f + ") &";
			int err1 = std::system(cmd.c_str());
			std::cout << "cmd:" << cmd << std::endl;
			std::cout << err1 << std::endl;
			std::cout << f << std::endl;
		} else {
			// suppress stderr/stdout from system call
			string qcmd = "(" + prog + 
				" -X POST -s -w \"\n%{http_code}\n\" -H \"Content-Type: application/json\" -d @"
				+ f + " " + url + 
				" > /dev/null 2>&1 ; /bin/rm -f " + f + ") &";
			int err2 = std::system(qcmd.c_str());
			if (debug) {
				std::cout << err2 << std::endl;
			}
		}
		return 0;
	}

public:
	curl_plugin() : vers("1.0.0") , state(ok), paused(false), mode(pi_config) { }

	/*!
	 */
        int publish(std::shared_ptr<builder_api> b) {
		if (paused)
			return 0;
		if (state != ok)
			return 1;
		if (mode != pi_pub_or_final)
			return 2;
		// write to tmpfile, curl, and then delete
		// this could be made fancy with a work queue in a later reimplementation.
		// try for /dev/shm/adc for performance and fall back to tmpdir
		string fname = get_temp_file_name();
		if (debug) {
			std::cout << "name: " << fname <<std::endl;
		}
		// open dump file
		std::ofstream out(fname);
		if (out.good()) {
			out << b->serialize() << std::endl;
			if (out.good()) {
				if (debug) {
					std::cout << "'curl' wrote" << std::endl;
				}
				out.close();
				curl_send(fname); // send also removes file
				return 0;
			} else {
				std::cout << "failed write" << std::endl;
				return 1;
			}
		}
		std::filesystem::remove(fname);
		std::cout << "failed open " << fname << std::endl;
		return 1;
	}

	/*!
	 */
        int config(const std::map< std::string, std::string >& m) {
		return config(m, plugin_prefix);
	}

	/*!
	 */
        int config(const std::map< std::string, std::string >& m, string_view env_prefix) {
		string d = get(m, "DIRECTORY", env_prefix);
		string url = get(m, "URL", env_prefix);
		string port = get(m, "PORT", env_prefix);
		string prog = get(m, "PROG", env_prefix);
		string sdebug = get(m, "DEBUG", env_prefix);
		return config(d, url, port, prog, sdebug);
	}
        
	const std::map< const std::string, const std::string> & get_option_defaults() {
		return plugin_config_defaults;
	}

	int initialize() {
		std::map <string, string >m;
		// config if never config'd
		if (!fdir.size())
			config(m);
		if (mode != pi_init) {
			return 2;
		}
		if ( state == err ) {
			std::cout << "curl plugin initialize found pre-existing error" << std::endl;
			return 3;
		}
		std::error_code ec;
		std::filesystem::create_directories(fdir, ec);
		if (ec.value() != 0 && ec.value() != EEXIST ) {
			state = err;
			std::cout << "unable to create scratch directory for plugin 'curl'; "
			       	<< fdir << " : " << ec.message() << std::endl;
			return ec.value();
		} else {
			if (debug) {
				std::cout << "created " << fdir <<std::endl;
			}
			mode = pi_pub_or_final;
		}
		return 0;
	}

        void finalize() {
		if (mode == pi_pub_or_final) {
			state = ok;
			paused = false;
			mode = pi_config;
		} else {
			if (debug) {
				std::cout << "curl plugin finalize on non-running plugin" << std::endl;
			}
		}
	}

	void pause() {
		paused = true;
	}

        void resume() {
		paused = false;
	}

	string_view name() const {
		return "curl";
	}

	string_view version() const {
		return vers;
	}

	~curl_plugin() {
		if (debug) {
			std::cout << "Destructing curl_plugin" << std::endl;
		}
	}
};

} // adc
