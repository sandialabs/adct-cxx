#include <adc/builder/builder.hpp>
#include <adc/publisher/publisher.hpp>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <unistd.h>
#include <ldms.h>

namespace adc {

using std::cout;

typedef std::string string;
typedef std::string_view string_view;


/*! \brief libldms_msg_publish publisher_api implementation.
  This plugin calls the ldms libraries for sending messages.

  Multiple independent instances of this plugin may be used simultaneously.
  The asynchronous tranmission spawns (or recycles) a separate thread to
  handle tranmission.
 */
class libldms_msg_publish_plugin : public publisher_api {
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
	/// \brief name of the channel ADC messages go into
	/// LDMS aggregators must be subscribed to this name.
	/// Overridden with env("ADC_LIBLDMS_MSG_PUBLISH_PLUGIN_CHANNEL").
	inline static const char* adc_libldms_msg_publish_plugin_channel_default = "adc_publish_api";

	/// \brief authentication method for channel connections; ldmsd listeners must match.
	/// Overridden with env("ADC_LIBLDMS_MSG_PUBLISH_PLUGIN_AUTH").
	inline static const char* adc_libldms_msg_publish_plugin_auth_default = "munge";

	/// \brief port for channel connections; ldmsd listeners must match.
	/// Overridden with env("ADC_LIBLDMS_MSG_PUBLISH_PLUGIN_PORT").
	inline static const char* adc_libldms_msg_publish_plugin_port_default = "412";

	/// \brief host for channel connections; ldmsd must be listening on the host.
	/// Overridden with env("ADC_LIBLDMS_MSG_PUBLISH_PLUGIN_HOST").
	inline static const char* adc_libldms_msg_publish_plugin_host_default = "localhost";

private:
	const std::map< const string, const string > plugin_libldms_msg_publish_config_defaults =
	{
		{"CHANNEL", adc_libldms_msg_publish_plugin_channel_default},
		{"AUTH", adc_libldms_msg_publish_plugin_auth_default},
		{"HOST", adc_libldms_msg_publish_plugin_host_default},
		{"PORT", adc_libldms_msg_publish_plugin_port_default}
	};

	inline static const char *plugin_prefix = "ADC_LIBLDMS_MSG_PUBLISH_PLUGIN_";

	const string vers;
	const std::vector<string> tags;
	string auth;
	string channel;
	string port;
	string host;
	enum state state;
	bool paused;
	enum mode mode;
	bool configured;

	int config(const string& dir, string_view shost, string_view sport, string_view sauth, string_view schannel) {
		if (mode != pi_config)
			return 2;
		uid_t ui = geteuid();
		port = sport;
		host = shost;
		auth = sauth;
		channel = schannel;
		mode = pi_init;
		configured = true;
		return 0;
	}

	// find field in m, then with prefix in env, then the default.
	const string get(const std::map< string, string >& m,
			string field, const string& env_prefix) {
		// fields not defined in config_defaults raise an exception.
		auto it = m.find(field);
		if (it != m.end()) {
			return it->second;
		}
		string en = env_prefix + field;
		char *ec = getenv(en.c_str());
		if (!ec) {
			return plugin_libldms_msg_publish_config_defaults.at(field);
		} else {
			return string(ec);
		}
	}
	
	// invoke ldms_stream_publish with json in f, then delete f.
	// wrap this in a thread to make it non-blocking eventually.
	int libldms_msg_publish_send(string& f)
	{
// fixme libldms_msg_publish_plugin::libldms_msg_publish_send
#if 0
		string qcmd = "(" + prog + 
			" -t json "
			" -x sock "
			" -h " + host +
			" -p " + port +
			" -a " + auth +
			" -s " + stream +
			" -f " + f +
			"> /dev/null 2>&1 ; /bin/rm " + f + ") &";
		int err2 = std::system(qcmd.c_str());
		std::cout << f << std::endl;
		std::cout << err2 << std::endl;
#endif
		return 0;
	}

public:
	libldms_msg_publish_plugin() : vers("0.0.0") , tags({"unimplemented"}), state(ok), paused(false), mode(pi_config), configured(false) {
		std::cout << "Constructing libldms_msg_publish_plugin" << std::endl;
	}

        int publish(std::shared_ptr<builder_api> b) {
		if (paused)
			return 0;
		if (state != ok)
			return 1;
		if (mode != pi_pub_or_final)
			return 2;
// fixme // send to ldms_msg api; libldms_msg_publish_plugin::publish
		return 1;
	}

        int config(const std::map< std::string, std::string >& m) {
		return config(m, plugin_prefix);
	}

        int config(const std::map< std::string, std::string >& m, const std::string& env_prefix) {
		string host = get(m, "HOST", env_prefix);
		string port = get(m, "PORT", env_prefix);
		string auth = get(m, "AUTH", env_prefix);
		string channel = get(m, "CHANNEL", env_prefix);
		return config(d, host, port, auth, channel);
	}
        
	const std::map< const std::string, const std::string> & get_option_defaults() {
		return plugin_libldms_msg_publish_config_defaults;
	}

	int initialize() {
		std::map <string, string >m;
		if (!configured)
			config(m);
		if (mode != pi_init) {
			return 2;
		}
		if ( state == err ) {
			std::cout << "libldms_msg_publish plugin initialize found pre-existing error" << std::endl;
			return 3;
		}
		// fixme: try connect to ldmsd; libldms_msg_publish_plugin::initialize
		std::error_code ec;
		// attach msg channel
		mode = pi_pub_or_final;
		return 0;
	}

        void finalize() {
		if (mode == pi_pub_or_final) {
			state = ok;
			paused = false;
			mode = pi_config;
		} else {
			std::cout << "libldms_msg_publish plugin finalize on non-running plugin" << std::endl;
		}
	}

	void pause() {
		paused = true;
	}

        void resume() {
		paused = false;
	}

	string_view name() const {
		return "libldms_msg_publish";
	}

	string_view version() const {
		return vers;
	}

	~libldms_msg_publish_plugin() {
		std::cout << "Destructing libldms_msg_publish_plugin" << std::endl;
	}
};

} // adc
