#define SYSLOG_NAMES
#include <syslog.h>
#include <adc/builder/builder.hpp>
#include <adc/publisher/publisher.hpp>
#include <cstdlib>

namespace adc {

using std::cout;

typedef std::string string;
typedef std::string_view string_view;


#define PRIORITY_UNSET_ADC_SYSLOG -1

int get_priority_from_string(string s)
{
	int i = 0;
	while (prioritynames[i].c_name &&
		strcmp(prioritynames[i].c_name,s.c_str()))
		i++;
	if (!prioritynames[i].c_name)
		return LOG_INFO;
	else
		return prioritynames[i].c_val;
}


/*! \brief syslog publisher_api implementation.
  This plugin sends messages to syslog synchronously.
  Multiple independent instances of this plugin may be used simultaneously,
  but the underlying syslog setting (priority) will be that
  of the most recently created instance.

  If not configured, the default priority is LOG_INFO.
  env("ADC_SYSLOG_PLUGIN_PRIORITY") overrides the default
  if defined with a priority string from
  /usr/include/sys/syslog.h.
 */
class syslog_plugin : public publisher_api {

private:
	inline static const std::map< const string, const string > plugin_syslog_config_defaults =
		{{ "PRIORITY", "info"} };
	inline static const char *plugin_prefix = "ADC_SYSLOG_PLUGIN_";
	const string vers;
	const std::vector<string> tags;
	int priority;
	bool paused;

	int config(string p) {
		priority = get_priority_from_string(std::move(p));
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
		string en = std::string(env_prefix) += field;
		char *ec = getenv(en.c_str());
		if (!ec) {
			return plugin_syslog_config_defaults.at(field);
		} else {
			return string(ec);
		}
	}

public:
	syslog_plugin() : vers("1.0.0") , tags({"none"}), priority(PRIORITY_UNSET_ADC_SYSLOG), paused(false) {
		openlog("ADC", LOG_PID, LOG_USER);

	}

        int publish(std::shared_ptr<builder_api> b) {
		if (paused)
			return 0;
		if (priority == PRIORITY_UNSET_ADC_SYSLOG)
			priority = LOG_INFO;
		auto str = b->serialize();
		syslog(priority, "%s", str.c_str());
		return 0;
	}

        int config(const std::map< std::string, std::string >& m) {
		return config(m, plugin_prefix);
	}

        int config(const std::map< std::string, std::string >& m, std::string_view env_prefix) {
		string p = get(m, "PRIORITY", env_prefix);
		return config(std::move(p));
	}
        
	const std::map< const std::string, const std::string> & get_option_defaults() {
		return plugin_syslog_config_defaults;
	}
        
	int initialize() {
		std::map <string, string >m;
		if (priority == PRIORITY_UNSET_ADC_SYSLOG)
			config(m);
		return 0;
	}

        void finalize() {
	}

	void pause() {
		paused = true;
	}

        void resume() {
		paused = false;
	}

	string_view name() const {
		return "syslog";
	}

	string_view version() const {
		return vers;
	}

	~syslog_plugin() {
		closelog();
	}
};

} // adc
