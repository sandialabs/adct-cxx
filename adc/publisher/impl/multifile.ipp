#include <adc/builder/builder.hpp>
#include <adc/publisher/publisher.hpp>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <filesystem>

namespace adc {

using std::cout;

typedef std::string string;
typedef std::string_view string_view;


/*! \brief Parallel file output publisher_api implementation.
  This plugin generates writes each message to the configured directory
  tree with \<json>\</json> delimiters surrounding it.
  The output directory is "." by default, but may be overriden with 
  a full path defined in env("ADC_MULTIFILE_PLUGIN_DIRECTORY").
  The resulting mass of files can be reduced independently later
  by concatenating all files in the tree or more selectively
  with a single call to the adc::multfile_assemble() function.

  Multiple independent multifile publishers may be created; exact filenames
  are not user controlled, to avoid collisions. Likewise there is no append
  mode.

  DIR/$user/[$adc_wfid.]$host.$pid.$start.$publisherptr/$application.$rank.XXXXXX
 */
class multifile_plugin : public publisher_api {
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
	inline static const std::map< const string, const string > plugin_multifile_config_defaults =
		{
			{ "DIRECTORY", "."},
			{ "RANK", ""}
		};
	inline static const char *plugin_multifile_prefix = "ADC_MULTIFILE_PLUGIN_";
	const string vers;
	const std::vector<string> tags;
	string fdir;
	string rank;
	std::ofstream out;
	enum state state;
	bool paused;
	enum mode mode;

	int config(const string dir, const string user_rank) {
		if (mode != pi_config)
			return 2;
		char hname[HOST_NAME_MAX+1];
		char uname[L_cuserid+1];
		if (gethostname(hname, HOST_NAME_MAX+1)) {
			return 2;
		}
		if (!cuserid(uname)) {
			return 2;
		}
		std::stringstream ss;
		// output goes to
		// dir/user/[wfid.].host.pid.starttime.ptr/application.rank.XXXXXX files
		// where application[.rank].XXXXXX is opened per process
		// and application comes in header and rank via config.
		char *wfid = getenv("ADC_WFID");
		pid_t pid = getpid();
		struct timespec ts;
		clock_gettime(CLOCK_BOOTTIME, &ts);
		ss << dir << "/" << uname << "/" << ( wfid ? wfid : "") <<
			(wfid ? "." : "") <<
			hname << "." << pid << "." << 
			ts.tv_sec << "." << ts.tv_nsec << "." << this;
		ss >> fdir;
		rank = user_rank;
		mode = pi_init;
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
			return plugin_multifile_config_defaults.at(field);
		} else {
			return string(ec);
		}
	}
	

public:
	multifile_plugin() : vers("1.0.0") , tags({"none"}), state(ok), paused(false), mode(pi_config) {
		std::cout << "Constructing multifile_plugin" << std::endl;
	}

	int publish(std::shared_ptr<builder_api> b) {
		if (paused)
			return 0;
		if (state != ok)
			return 1;
		if (mode != pi_pub_or_final)
			return 2;
		auto header = b->get_section("header");
		auto app = b->get_value("application");
		
#if 0
		// fixme; multifile_plugin::publish
		// open dump file; messages are null separated, not endl separated, as content may include \n.
		// need a map of stream to header.application
		string fpath = fdir + "/" + "multifile.XXXXXX"; // 
		out.open(fpath, std::ofstream::out | (std::ofstream::trunc));
		if (out.good()) {
			mode = pi_pub_or_final;
			return 0;
		}
		state = err;
		return EBADF;
#endif
		// write to stream
		if (out.good()) {
			out << "<json>"  << b->serialize() << "</json>" << std::endl;
			std::cout << "'multifile' wrote" << std::endl;
			return 0;
		}
		std::cout << "failed out.good" << std::endl;
		return 1;
	}

        int config(const std::map< std::string, std::string >& m) {
		return config(m, plugin_multifile_prefix);
	}

        int config(const std::map< std::string, std::string >& m, std::string_view env_prefix) {
		string d = get(m, "DIRECTORY", env_prefix);
		string r = get(m, "RANK", env_prefix);
		return config(d, r);
	}
        
	const std::map< const std::string, const std::string> & get_option_defaults() {
		return plugin_multifile_config_defaults;
	}
        
	int initialize() {
		std::map <string, string >m;
		if (!fdir.size())
			config(m);
		if (mode != pi_init) {
			return 2;
		}
		if ( state == err ) {
			std::cout <<
				"multifile plugin initialize found pre-existing error"
				<< std::endl;
			return 3;
		}
		std::error_code ec;
		std::filesystem::create_directories(fdir, ec);
		if (ec.value() != 0 && ec.value() != EEXIST ) {
			state = err;
			std::cout << "unable to create output directory for plugin 'multifile'; "
				<< fdir << " : " << ec.message() << std::endl;
			return ec.value();
		}
		string testfile = fdir + "/.XXXXXX";
		auto ftemplate = std::make_unique<char[]>(testfile.length()+1);
		::std::strcpy(ftemplate.get(), testfile.c_str());
		int fd = mkstemp(ftemplate.get());
		int rc = errno;
		if (fd == -1) {
			state = err;
			std::cout << "unable to open file in output directory "
				<< fdir << " for plugin 'multifile': " << 
				std::strerror(rc) << std::endl;
			return 4;
		} else {
			close(fd);
			unlink(ftemplate.get());
		}
		return 0;
	}

	void finalize() {
		if (mode == pi_pub_or_final) {
			state = ok;
			paused = false;
			mode = pi_config;
			out.close();
		} else {
			std::cout << "multifile plugin finalize on non-running plugin" << std::endl;
		}
	}

	void pause() {
		paused = true;
	}

        void resume() {
		paused = false;
	}

	string_view name() const {
		return "multifile";
	}

	string_view version() const {
		return vers;
	}

	~multifile_plugin() {
		std::cout << "Destructing multifile_plugin" << std::endl;
	}
};

} // adc
