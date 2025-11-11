#include <adc/builder/builder.hpp>
#include <adc/publisher/publisher.hpp>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <filesystem>

namespace adc {

using std::cout;

typedef std::string string;
typedef std::string_view string_view;


/*! \brief File output publisher_api implementation.
  This plugin generates writes each message to the configured file, with
  \<json>\</json> delimiters surrounding it.
  The output directory is "." by default, but may be overriden with 
  a full path defined in env("ADC_FILE_PLUGIN_DIRECTORY").
  The file name is adc.file_plugin.log by default, and may be overridden
  with a path name in env("ADC_FILE_PLUGIN_FILE").
  The file is overwritten when the file_plugin is created, unless
  env("ADC_FILE_PLUGIN_APPEND") is "true".
  Debugging output is enabled if 
  env("ADC_FILE_PLUGIN_DEBUG") is a number greater than 0.

  Multiple independent file publishers may be created; if the same file name 
  and directory are used for distinct instances, output file content
  is undefined.
 */
class file_plugin : public publisher_api {
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

private:
	inline static const std::map< const string, const string > plugin_file_config_defaults =
		{{ "DIRECTORY", "."},
		 { "FILE", "adc.file_plugin.log" },
		 { "DEBUG", "0" },
		 { "APPEND", "false" }
		};
	inline static const char *plugin_file_prefix = "ADC_FILE_PLUGIN_";
	const string vers;
	const std::vector<string> tags;
	string fname;
	string fdir;
	bool fappend;
	std::ofstream out;
	int debug;
	enum state state;
	bool paused;
	enum mode mode;

	int config(const string dir, const string file, bool append, const string& sdebug) {
		if (mode != pi_config)
			return 2;
		fname = file;
		fappend = append;
		fdir = dir;
		mode = pi_init;
		std::stringstream ss(sdebug);
		ss >> debug;
		if (debug < 0) {
			debug = 0;
		}
		if (debug > 0) {
			std::cout<< "file plugin configured" <<std::endl;
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
			return plugin_file_config_defaults.at(field);
		} else {
			return string(ec);
		}
	}


public:
	file_plugin() : vers("1.0.0") , tags({"none"}), fappend(false), debug(0),
		state(ok), paused(false), mode(pi_config) { }

	int publish(std::shared_ptr<builder_api> b) {
		if (paused)
			return 0;
		if (state != ok)
			return 1;
		if (mode != pi_pub_or_final)
			return 2;
		// write to stream
		if (out.good()) {
			out << "<json>"  << b->serialize() << "</json>" << std::endl;
			if (debug) {
				std::cout << "'file' wrote" << std::endl;
			}
			return 0;
		}
		if (debug) {
			std::cout << "plugin 'file' failed out.good" << std::endl;
		}
		return 1;
	}

	int config(const std::map< std::string, std::string >& m) {
		return config(m, plugin_file_prefix);
	}

	int config(const std::map< std::string, std::string >& m, string_view env_prefix) {
		bool app = ( get(m, "APPEND", env_prefix) == "true");
		string d = get(m, "DIRECTORY", env_prefix);
		string f = get(m, "FILE", env_prefix);
		string sdebug = get(m, "DEBUG", env_prefix);
		return config(d, std::move(f), app, sdebug);
	}

	const std::map< const std::string, const std::string> & get_option_defaults() {
		return plugin_file_config_defaults;
	}

	int initialize() {
		std::map <string, string >m;
		// config if never config'd
		if (!fname.size())
			config(m);
		if (mode != pi_init) {
			return 2;
		}
		if ( state == err ) {
			std::cout << "file plugin initialize found pre-existing error" << std::endl;
			return 3;
		}
		std::error_code ec;
		std::filesystem::create_directories(fdir, ec);
		if (ec.value() != 0 && ec.value() != EEXIST ) {
			state = err;
			std::cout << "unable to create output directory for plugin 'file'; "
			       	<< fdir << " : " << ec.message() << std::endl;
			return ec.value();
		}
		string fpath = fdir + "/" + fname;
		// open dump file; messages are null separated, not endl separated, as content may include \n.
		out.open(fpath, std::ofstream::out |
       			(fappend ? std::ofstream::app : std::ofstream::trunc));
		if (out.good()) {
			mode = pi_pub_or_final;
			return 0;
		}
		state = err;
		return EBADF;
	}

	void finalize() {
		if (mode == pi_pub_or_final) {
			state = ok;
			paused = false;
			mode = pi_config;
			out.close();
		} else {
			if (debug) {
				std::cout << "file plugin finalize on non-running plugin" << std::endl;
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
		return "file";
	}

	string_view version() const {
		return vers;
	}

	~file_plugin() {
		if (debug) {
			std::cout << "Destructing file_plugin" << std::endl;
		}
	}
};

} // adc
