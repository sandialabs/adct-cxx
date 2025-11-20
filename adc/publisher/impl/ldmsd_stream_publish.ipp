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


/*! \brief ldmsd_stream_publish utility publisher_api implementation.
  This plugin generates a scratch file (in-memory) and asynchronously
  sends it to ldmsd by invoking the 'ldmsd_stream_publish' utility
  available in ldms versions up to 4.4. Use the ldms_message_publish_plugin
  for ldms versions 4.5 and above.

  Multiple independent instances of this plugin may be used simultaneously.
  The asynchronous tranmission spawns a separate shell process to
  handle tranmission and file cleanup.
 */
class ldmsd_stream_publish_plugin : public publisher_api {
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
	/// \brief scratch directory for ldmsd_stream_publish utility messages
	/// This directory must be globally writable and should be fast.
	/// Overridden with env("ADC_LDMSD_STREAM_PUBLISH_PLUGIN_DIRECTORY").
	inline static const char* adc_ldmsd_stream_publish_plugin_directory_default = "/dev/shm/adc";

	/// \brief full path to ldmsd_stream_publish utility
	/// This program should be world accessible.
	/// Overridden with env("ADC_LDMSD_STREAM_PUBLISH_PLUGIN_PROG").
	inline static const char* adc_ldmsd_stream_publish_plugin_prog_default = "/usr/sbin/ldmsd_stream_publish";

	/// \brief name of the stream ADC messages go into
	/// LDMS aggregators must be subscribed to this name.
	/// Overridden with env("ADC_LDMSD_STREAM_PUBLISH_PLUGIN_STREAM").
	inline static const char* adc_ldmsd_stream_publish_plugin_stream_default = "adc_publish_api";

	/// \brief authentication method for stream connections; ldmsd listeners must match.
	/// Overridden with env("ADC_LDMSD_STREAM_PUBLISH_PLUGIN_AUTH").
	inline static const char* adc_ldmsd_stream_publish_plugin_auth_default = "munge";

	/// \brief port for stream connections; ldmsd listeners must match.
	/// Overridden with env("ADC_LDMSD_STREAM_PUBLISH_PLUGIN_PORT").
	inline static const char* adc_ldmsd_stream_publish_plugin_port_default = "412";

	/// \brief host for stream connections; ldmsd must be listening on the host.
	/// Overridden with env("ADC_LDMSD_STREAM_PUBLISH_PLUGIN_HOST").
	inline static const char* adc_ldmsd_stream_publish_plugin_host_default = "localhost";

	/// \brief affinity for spawned processes; defaults to all
	/// If not all, it should be a range list of cpu numbers, e,g. "0-1,56-57"
	/// on a 56 core hyperthreaded processor where admin tools should run on 2 lowest cores.
	/// Overridden with env("ADC_LDMSD_STREAM_PUBLISH_PLUGIN_AFFINITY").
	inline static const char* adc_ldmsd_stream_publish_plugin_affinity_default = "all";

	/// \brief ADC ldmsd-stream plugin enable debug messages; (default "0": none)
	/// Overridden with env("ADC_LDMSD_STREAM_PUBLISH_PLUGIN_DEBUG").
	inline static const char *adc_ldmsd_stream_publish_plugin_debug_default = "0";


private:
	const std::map< const string, const string > plugin_ldmsd_stream_publish_config_defaults =
	{	{"DIRECTORY", adc_ldmsd_stream_publish_plugin_directory_default},
		{"PROG", adc_ldmsd_stream_publish_plugin_prog_default},
		{"STREAM", adc_ldmsd_stream_publish_plugin_stream_default},
		{"AUTH", adc_ldmsd_stream_publish_plugin_auth_default},
		{"HOST", adc_ldmsd_stream_publish_plugin_host_default},
		{"PORT", adc_ldmsd_stream_publish_plugin_port_default},
		{"DEBUG", adc_ldmsd_stream_publish_plugin_debug_default},
		{"AFFINITY", adc_ldmsd_stream_publish_plugin_affinity_default}
	};

	inline static const char *plugin_prefix = "ADC_LDMSD_STREAM_PUBLISH_PLUGIN_";

	const string vers;
	const std::vector<string> tags;
	string fdir;
	string prog;
	string auth;
	string stream;
	string port;
	string host;
	int debug;
	enum state state;
	bool paused;
	enum mode mode;

	int config(const string& dir, string_view shost, string_view sport, string_view sprog, string_view sauth, string_view sstream, const string& sdebug) {
		if (mode != pi_config)
			return 2;
		uid_t ui = geteuid();
		fdir = dir + string("/") + std::to_string(ui);
		port = sport;
		host = shost;
		prog = sprog;
		auth = sauth;
		stream = sstream;
		mode = pi_init;
		std::stringstream ss(sdebug);
		ss >> debug;
		if (debug < 0) {
			debug = 0;
		}
		if (debug > 0) {
			std::cout<< "ldmsd_stream_publish plugin configured" <<std::endl;
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
			return plugin_ldmsd_stream_publish_config_defaults.at(field);
		} else {
			return string(ec);
		}
	}
	
	string get_temp_file_name() {
		string tplt = fdir + "/json-lsp-msg-XXXXXX";
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

	// invoke ldms_stream_publish with json in f, then delete f.
	// wrap this in a thread to make it non-blocking eventually.
	int ldmsd_stream_publish_send(string& f)
	{
		string qcmd = "(" + prog + 
			" -t json "
			" -x sock "
			" -h " + host +
			" -p " + port +
			" -a " + auth +
			" -s " + stream +
			" -f " + f +
			"> /dev/null 2>&1 ; /bin/rm -f " + f + ") &";
		int err2 = std::system(qcmd.c_str());
		if (debug) {
			std::cout << f << std::endl;
			std::cout << err2 << std::endl;
		}
		return 0;
	}

public:
	ldmsd_stream_publish_plugin() : vers("1.0.0") , tags({"none"}), debug(0),
		state(ok), paused(false), mode(pi_config) {}

        int publish(std::shared_ptr<builder_api> b) {
		if (paused)
			return 0;
		if (state != ok)
			return 1;
		if (mode != pi_pub_or_final)
			return 2;
		// write to tmpfile, ldms_stream_publish, and then delete
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
					std::cout << "'ldmsd_stream_publish' wrote" << std::endl;
				}
				out.close();
				ldmsd_stream_publish_send(fname); // send also removes file
				return 0;
			} else {
				std::cout << "failed write" << std::endl;
				return 1;
			}
		}
		std::filesystem::remove(fname);
		std::cout << "failed open " <<fname<< std::endl;
		return 1;
	}

        int config(const std::map< std::string, std::string >& m) {
		return config(m, plugin_prefix);
	}

        int config(const std::map< std::string, std::string >& m, string_view env_prefix) {
		string d = get(m, "DIRECTORY", env_prefix);
		string host = get(m, "HOST", env_prefix);
		string port = get(m, "PORT", env_prefix);
		string prog = get(m, "PROG", env_prefix);
		string auth = get(m, "AUTH", env_prefix);
		string stream = get(m, "STREAM", env_prefix);
		string sdebug = get(m, "DEBUG", env_prefix);
		return config(d, host, port, prog, auth, stream, sdebug);
	}
        
	const std::map< const std::string, const std::string> & get_option_defaults() {
		return plugin_ldmsd_stream_publish_config_defaults;
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
			std::cout << "ldmsd_stream_publish plugin initialize found pre-existing error" << std::endl;
			return 3;
		}
		std::error_code ec;
		std::filesystem::create_directories(fdir, ec);
		if (ec.value() != 0 && ec.value() != EEXIST ) {
			state = err;
			std::cout << "unable to create scratch directory for plugin 'ldmsd_stream_publish'; "
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
				std::cout << "ldmsd_stream_publish plugin finalize on non-running plugin" << std::endl;
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
		return "ldmsd_stream_publish";
	}

	string_view version() const {
		return vers;
	}

	~ldmsd_stream_publish_plugin() {
		if (debug) {
			std::cout << "Destructing ldmsd_stream_publish_plugin" << std::endl;
		}
	}
};

} // adc
