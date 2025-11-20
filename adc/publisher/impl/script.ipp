/* Copyright 2025 NTESS. See the top-level LICENSE.txt file for details.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
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


/*! \brief User script publisher plugin. The program specified
  by environment variable is used to asynchronously deliver
  published messages. It is invoked in a shell as:
     ($prog $messagefile > /dev/null 2>&1 || /bin/rm $messagefile) &
 */
class script_plugin : public publisher_api {
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
	/// \brief The default scratch directory for user script messages.
	/// This directory must be globally writable and should be fast.
	/// Overridden with env("ADC_SCRIPT_PLUGIN_DIRECTORY").
	static const inline char *adc_script_plugin_dir_default = "/dev/shm/adc";

	/// \brief The default path to a user script or program.
	/// This program should be user accessible.
	/// Overridden with env("ADC_SCRIPT_PLUGIN_PROG").
	/// The default : is a non-operation.
	static const inline char *adc_script_plugin_prog_default = ":";

	/// \brief ADC script plugin enable debug messages; (default "0": none)
	/// "2" provides message send debugging
	/// "1" provides lighter debugging
	/// Overridden with env("ADC_SCRIPT_PLUGIN_DEBUG").
	inline static const char *adc_script_plugin_debug_default = "0";

private:
	const std::map< const string, const string > plugin_script_config_defaults =
	{	{"DIRECTORY", adc_script_plugin_dir_default},
		{"DEBUG", adc_script_plugin_debug_default},
		{"PROG", adc_script_plugin_prog_default}
	};
	inline static const char *plugin_prefix = "ADC_SCRIPT_PLUGIN_";
	const string vers;
	const std::vector<string> tags;
	string fdir;
	string prog;
	int debug;
	enum state state;
	bool paused;
	enum mode mode;

	int config(const string& dir, string_view sprog, const string &sdebug) {
		if (mode != pi_config)
			return 2;
		uid_t ui = geteuid();
		fdir = dir + string("/") + std::to_string(ui);
		prog = sprog;
		mode = pi_init;
		std::stringstream ss(sdebug);
		ss >> debug;
		if (debug < 0) {
			debug = 0;
		}
		if (debug > 0) {
			std::cout<< "script plugin configured" <<std::endl;
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
			return plugin_script_config_defaults.at(field);
		} else {
			return string(ec);
		}
	}
	
	string get_temp_file_name() {
		string tplt = fdir + "/json-sh-msg-XXXXXX";
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
		std::cout<< "'script' plugin: mkstemp failed" <<std::endl;
		delete[] cstr;
		return "";
	}

	// invoke script with json in f as arg 1, then delete f.
	// wrap this in a thread to make it non-blocking eventually.
	int script_send(string& f)
	{
		string qcmd = "(" + prog + " " + f +
			"> /dev/null 2>&1 || /bin/rm " + f + ") &";
		int err2 = std::system(qcmd.c_str());
		if (debug > 1) {
			std::cout << "script plugin file: " << f << std::endl;
			std::cout << "script plugin err: " << err2 << std::endl;
		}
		return 0;
	}

public:
	script_plugin() : vers("1.0.0") , tags({"none"}), debug(0), state(ok), paused(false), mode(pi_config) { }

        int publish(std::shared_ptr<builder_api> b) {
		if (paused)
			return 0;
		if (state != ok)
			return 1;
		if (mode != pi_pub_or_final)
			return 2;
		// write to tmpfile, run script and then delete tmpfile
		// this could be made fancy with a work queue in a later reimplementation.
		// try for /dev/shm/adc for performance and fall back to tmpdir
		string fname = get_temp_file_name();
		if (debug) {
			std::cout << "script plugin temp name: " << fname <<std::endl;
		}
		// open dump file
		std::ofstream out(fname);
		if (out.good()) {
			out << b->serialize() << std::endl;
			if (out.good()) {
				if (debug) {
					std::cout << "plugin 'script' wrote" << std::endl;
				}
				out.close();
				script_send(fname); // send also removes file
				return 0;
			} else {
				std::cout << "script plugin failed write" << std::endl;
				return 1;
			}
		}
		std::filesystem::remove(fname);
		std::cout << "script plugin failed open " <<fname<< std::endl;
		return 1;
	}

        int config(const std::map< std::string, std::string >& m) {
		return config(m, plugin_prefix );
	}

        int config(const std::map< std::string, std::string >& m, std::string_view env_prefix) {
		string d = get(m, "DIRECTORY", env_prefix);
		string prog = get(m, "PROG", env_prefix);
		string sdebug = get(m, "DEBUG", env_prefix);
		return config(d, prog, sdebug);
	}

	const std::map< const std::string, const std::string> & get_option_defaults() {
		return plugin_script_config_defaults;
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
			std::cout << "script plugin initialize found pre-existing error" << std::endl;
			return 3;
		}
		std::error_code ec;
		std::filesystem::create_directories(fdir, ec);
		if (ec.value() != 0 && ec.value() != EEXIST ) {
			state = err;
			std::cout << "unable to create scratch directory for plugin 'script'; "
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
				std::cout << "script plugin finalize on non-running plugin" << std::endl;
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
		return "script";
	}

	string_view version() const {
		return vers;
	}

	~script_plugin() {
		if (debug) {
			std::cout << "Destructing script_plugin" << std::endl;
		}
	}
};

} // adc
