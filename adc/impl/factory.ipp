/* Copyright 2025 NTESS. See the top-level LICENSE.txt file for details.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef adc_factory_ipp
#define adc_factory_ipp
#include <string>
#include <map>
#include "boost/json/src.hpp"
#include "adc/factory.hpp"

// add a base class for common impl details like pause/resume
// #include <adc/builder/impl/base_publisher.ipp>

#define ADC_PUBLISHER_NONE_NAME "none"
#include <adc/publisher/impl/none.ipp>

#define ADC_PUBLISHER_STDOUT_NAME "stdout"
#include <adc/publisher/impl/stdout.ipp>

#define ADC_PUBLISHER_SYSLOG_NAME "syslog"
#include <adc/publisher/impl/syslog.ipp>

#define ADC_PUBLISHER_FILE_NAME "file"
#include <adc/publisher/impl/file.ipp>

#define ADC_PUBLISHER_MULTIFILE_NAME "multifile"
#include <adc/publisher/impl/multifile.ipp>

#define ADC_PUBLISHER_CURL_NAME "curl"
#include <adc/publisher/impl/curl.ipp>

#define ADC_PUBLISHER_LDMS_SUBPROCESS_NAME "ldmsd_stream_publish"
#include <adc/publisher/impl/ldmsd_stream_publish.ipp>

#define ADC_PUBLISHER_LDMS_SUBPROCESS_MESSAGE_NAME "ldms_message_publish"
#include <adc/publisher/impl/ldms_message_publish.ipp>

#define ADC_PUBLISHER_SCRIPT_NAME "script"
#include <adc/publisher/impl/script.ipp>

#ifdef ADC_HAVE_LDMS

#ifdef ENABLE_ADC_PUBLISHER_LIBLDMS_MSG
#define ADC_PUBLISHER_LIBLDMS_MSG_NAME "libldms_msg"
#include <adc/publisher/impl/libldms_msg.ipp>
#endif

#endif

#ifdef ENABLE_ADC_PUBLISHER_LIBCURL
#define ADC_PUBLISHER_LIBCURL_NAME "libcurl"
#include <adc/publisher/impl/libcurl.ipp>
#endif

#if ADC_HAVE_ADIAK

#define ADC_PUBLISHER_LIBADIAK_NAME "libadiak_many"
#include <adc/publisher/impl/libadiak_many.ipp>

#if defined(ADIAK_HAVE_JSONSTRING) && (ADIAK_HAVE_JSONSTRING==1)
#define ADC_PUBLISHER_LIBADIAK_JSON_NAME "libadiak_json"
#include <adc/publisher/impl/libadiak_json.ipp>
#endif

#endif

#include <adc/publisher/impl/multi_publisher.ipp>

#include <adc/builder/impl/builder.ipp>

namespace adc {

void factory::init()
{
	if (names.size() == 0 ) {
		// init list of uninstantiated plugin 
		names.insert( ADC_PUBLISHER_NONE_NAME );
		names.insert( ADC_PUBLISHER_FILE_NAME );
		names.insert( ADC_PUBLISHER_MULTIFILE_NAME );
		names.insert( ADC_PUBLISHER_STDOUT_NAME );
		names.insert( ADC_PUBLISHER_SYSLOG_NAME );
		names.insert( ADC_PUBLISHER_CURL_NAME );
		names.insert( ADC_PUBLISHER_LDMS_SUBPROCESS_NAME );
		names.insert( ADC_PUBLISHER_LDMS_SUBPROCESS_MESSAGE_NAME );
		names.insert( ADC_PUBLISHER_SCRIPT_NAME );

#ifdef ENABLE_ADC_PUBLISHER_LIBCURL
		names.insert( ADC_PUBLISHER_LIBCURL_NAME );
#endif

#ifdef ENABLE_ADC_PUBLISHER_LIBLDMS_MSG
#define ADC_PUBLISHER_LIBLDMS_MSG_NAME "libldms_msg"
		names.insert( ADC_PUBLISHER_LIBLDMS_MSG_NAME );
#endif

#ifdef ENABLE_ADC_PUBLISHER_LIBADIAK
		names.insert( ADC_PUBLISHER_LIBADIAK_NAME );
#endif

#ifdef ADC_PUBLISHER_LIBADIAK_JSON_NAME
		names.insert( ADC_PUBLISHER_LIBADIAK_JSON_NAME );
#endif

	}
	const char *env = getenv("ADC_MULTI_PUBLISHER_DEBUG");
	if (env && !strcmp(env,"1") ) {
		debug = 1;
	} else {
		debug = 0;
	}

}

std::shared_ptr<multi_publisher_api> factory::get_multi_publisher()
{
	std::shared_ptr<multi_publisher_api> p(new multi_publisher);
	return p;
}

std::shared_ptr<multi_publisher_api> factory::get_multi_publisher(std::map< const std::string, const std::map<std::string, std::string> > plugin_map)
{
	std::shared_ptr<multi_publisher_api> mp(new multi_publisher);
	if (!plugin_map.size()) {
		return mp;
	}
	const std::map< std::string, std::string > m;
	for (const auto& pair : plugin_map) {
		std::shared_ptr<publisher_api> p = get_publisher(pair.first, pair.second);
		if (p) {
			if (debug) {
				std::cout << "multi_publisher: publisher"
					" found named: " << pair.first << std::endl;
			}
			int ei = p->initialize();
			if (ei) {
				if (debug) {
					std::cout << "multi_publisher: initialize"
						" failed for: " << pair.first << std::endl;
				}
				continue;
			}
			mp->add(p);
		} else {
			if (debug) {
				std::cout << "multi_publisher: no publisher"
					" found named: " << pair.first << std::endl;
			}
		}
	}
}

std::shared_ptr<multi_publisher_api> factory::get_multi_publisher_from_env(const std::string& namelist)
{
	std::shared_ptr<multi_publisher_api> mp(new multi_publisher);
	const char *env;
	if (!namelist.size()) {
		env = getenv("ADC_MULTI_PUBLISHER_NAMES");
	} else {
		env = getenv(namelist.c_str());
	}
	if (env) {
		auto enames = split_string(std::string(env), ':');
		return get_multi_publisher_from_env(enames);
	}
	return mp;
}

std::shared_ptr<multi_publisher_api> factory::get_multi_publisher_from_env(std::vector<std::string>& namelist)
{

	std::shared_ptr<multi_publisher_api> mp(new multi_publisher);
	if (namelist.size() != 0) {
		const std::map< std::string, std::string > m;
		for (auto n : namelist) {
			std::shared_ptr<publisher_api> p = get_publisher(n);
			if (p) {
				if (debug) {
					std::cout << "multi_publisher: publisher"
						" found named: " << n << std::endl;
				}
				int ec = p->config(m);
				if (ec) {
					if (debug) {
						std::cout << "multi_publisher: config"
							" failed for: " << n << std::endl;
					}
					continue;
				}
				int ei = p->initialize();
				if (ei) {
					if (debug) {
						std::cout << "multi_publisher: initialize"
							" failed for: " << n << std::endl;
					}
					continue;
				}
				mp->add(p);
			} else {
				if (debug) {
					std::cout << "multi_publisher: no publisher"
						" found named: " << n << std::endl;
				}
			}
		}
	}
	return mp;
}

std::shared_ptr<publisher_api> factory::get_publisher( const std::string& name)
{
	init();
	auto it = names.find(name);
	if (it != names.end()) {
		if (name == ADC_PUBLISHER_NONE_NAME ) {
			std::shared_ptr<publisher_api> p(new none_plugin);
			return p;
		}
		if (name == ADC_PUBLISHER_STDOUT_NAME ) {
			std::shared_ptr<publisher_api> p(new stdout_plugin);
			return p;
		}
		if (name == ADC_PUBLISHER_SYSLOG_NAME ) {
			std::shared_ptr<publisher_api> p(new syslog_plugin);
			return p;
		}
		if (name == ADC_PUBLISHER_FILE_NAME ) {
			std::shared_ptr<publisher_api> p(new file_plugin);
			return p;
		}
		if (name == ADC_PUBLISHER_MULTIFILE_NAME ) {
			std::shared_ptr<publisher_api> p(new multifile_plugin);
			return p;
		}
		if (name == ADC_PUBLISHER_CURL_NAME ) {
			std::shared_ptr<publisher_api> p(new curl_plugin);
			return p;
		}
		if (name == ADC_PUBLISHER_LDMS_SUBPROCESS_NAME) {
			std::shared_ptr<publisher_api> p(new ldmsd_stream_publish_plugin);
			return p;
		}
		if (name == ADC_PUBLISHER_LDMS_SUBPROCESS_MESSAGE_NAME) {
			std::shared_ptr<publisher_api> p(new ldms_message_publish_plugin);
			return p;
		}
		if (name == ADC_PUBLISHER_SCRIPT_NAME) {
			std::shared_ptr<publisher_api> p(new script_plugin);
			return p;
		}
#ifdef ENABLE_ADC_PUBLISHER_LDMS_STREAM
		// todo
#endif
#ifdef ENABLE_ADC_PUBLISHER_LIBCURL
		// todo
#endif
#ifdef ENABLE_ADC_PUBLISHER_LIBADIAK
		if (name == ADC_PUBLISHER_LIBADIAK_NAME ) {
			std::shared_ptr<publisher_api> p(new libadiak_plugin);
			return p;
		}
#endif
	}
	return std::shared_ptr<publisher_api>();
}

std::shared_ptr<publisher_api> factory::get_publisher(const std::string& name, const std::map<std::string, std::string>& opts)
{
	init();
	auto it = names.find(name);
	if (it != names.end()) {
		if (name == ADC_PUBLISHER_NONE_NAME ) {
			std::shared_ptr<publisher_api> p(new none_plugin());
			p->config(opts);
			return p;
		}
		if (name == ADC_PUBLISHER_STDOUT_NAME ) {
			std::shared_ptr<publisher_api> p(new stdout_plugin());
			p->config(opts);
			return p;
		}
		if (name == ADC_PUBLISHER_SYSLOG_NAME ) {
			std::shared_ptr<publisher_api> p(new syslog_plugin());
			p->config(opts);
			return p;
		}
		if (name == ADC_PUBLISHER_FILE_NAME ) {
			std::shared_ptr<publisher_api> p(new file_plugin());
			p->config(opts);
			return p;
		}
		if (name == ADC_PUBLISHER_MULTIFILE_NAME ) {
			std::shared_ptr<publisher_api> p(new multifile_plugin());
			p->config(opts);
			return p;
		}
		if (name == ADC_PUBLISHER_CURL_NAME ) {
			std::shared_ptr<publisher_api> p(new curl_plugin());
			p->config(opts);
			return p;
		}
		if (name == ADC_PUBLISHER_LDMS_SUBPROCESS_NAME) {
			std::shared_ptr<publisher_api> p(new ldmsd_stream_publish_plugin());
			p->config(opts);
			return p;
		}
		if (name == ADC_PUBLISHER_LDMS_SUBPROCESS_MESSAGE_NAME) {
			std::shared_ptr<publisher_api> p(new ldms_message_publish_plugin());
			p->config(opts);
			return p;
		}
		if (name == ADC_PUBLISHER_SCRIPT_NAME) {
			std::shared_ptr<publisher_api> p(new script_plugin());
			p->config(opts);
			return p;
		}
		// TODO: ldmsd lib publisher
		// TODO: ldms lib publisher
		// TODO: curl lib publisher
	}
	return std::shared_ptr<publisher_api>();
}

// return the names of publishers available
// there's a cleaner view solution to this in c++20, but we're assuming 17.
const std::set<std::string>& factory::get_publisher_names()
{
	init();
	return names;
}

std::shared_ptr<builder_api> factory::get_builder()
{
	std::shared_ptr<builder_api> b(new builder);
	return b;
}


} // end adc
#endif // adc_factory_ipp
