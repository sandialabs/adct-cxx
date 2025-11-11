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

#define ADC_LIBCURL_PLUGIN_PORT_DEFAULT "443"
#define ADC_LIBCURL_PLUGIN_URL_DEFAULT "https://localhost"

const std::map< string, string > plugin_libcurl_config_defaults =
{	{"URL", ADC_LIBCURL_PLUGIN_URL_DEFAULT},
	{"PORT", ADC_LIBCURL_PLUGIN_PORT_DEFAULT}
};
const string plugin_libcurl_prefix("ADC_LIBCURL_PLUGIN_");

/* this behaves as a singleton under the plugin dynamic loader */

/*! \brief NOT yet implemented publisher plugin that will eventually use libcurl.
 */
class libcurl_plugin : public publisher_api {
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
	const string vers;
	const std::vector<string> tags;
	string port;
	string url;
	enum state state;
	bool paused;
	enum mode mode;

	int config(string_view surl, string_view sport) {
		if (mode != pi_config)
			return 2;
		port = sport;
		url = surl;
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
			return plugin_libcurl_config_defaults.at(field);
		} else {
			return string(ec);
		}
	}
	
	string get_temp_file_name() {
		string tplt = fdir + "/json-msg-XXXXXX";
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
		return "";
	}

	// invoke https network POST with json 
	// wrap this in a thread to make it non-blocking eventually?
	int libcurl_send(std::string_view json)
	{
		// fixme: implement POST with boost beast/asio or libcurl in libcurl_send; see curl --libcurl ; libcurl_plugin::libcurl_send
		// probably prefer libcurl:
		// https://curl.se/libcurl/c/postinmemory.html
		// https://curl.se/libcurl/c/simplessl.html
		return 1;
	}

public:
	libcurl_plugin() : vers("0.0.0") , tags({"unimplemented"}), state(ok), paused(false), mode(pi_config) {
		std::cout << "Constructing libcurl_plugin" << std::endl;
	}

	/*! \brief NOT IMPLEMENTED .libcurl_plugin::libcurl_send pending.  */
        int publish(std::shared_ptr< builder_api > b) {
		if (paused)
			return 0;
		if (state != ok)
			return 1;
		if (mode != pi_pub_or_final)
			return 2;
		string json = b->serialize();
		libcurl_send(json);
		return 0;
	}

        int config(const std::map< std::string, std::string >& m) {
		return config(m, plugin_libcurl_prefix);
	}

        int config(const std::map< std::string, std::string >& m, std::string_view env_prefix) {
		string url = get(m, "URL", env_prefix);
		string port = get(m, "PORT", env_prefix);
		return config(url, port);
	}
        
	const std::map< const std::string, const std::string> & get_option_defaults() {
		return plugin_libcurl_config_defaults;
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
			std::cout << "libcurl plugin initialize found pre-existing error" << std::endl;
			return 3;
		}
		std::error_code ec;
		std::filesystem::create_directories(fdir, ec);
		if (ec.value() != 0 && ec.value() != EEXIST ) {
			state = err;
			std::cout << "unable to create scratch directory for plugin 'libcurl'; "
			       	<< fdir << " : " << ec.message() << std::endl;
			return ec.value();
		} else {
			std::cout << "created " << fdir <<std::endl;
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
			std::cout << "libcurl plugin finalize on non-running plugin" << std::endl;
		}
	}

	void pause() {
		paused = true;
	}

        void resume() {
		paused = false;
	}

	string_view name() const {
		return "libcurl";
	}

	string_view version() const {
		return vers;
	}

	~libcurl_plugin() {
		std::cout << "Destructing libcurl_plugin" << std::endl;
	}
};

} // adc
