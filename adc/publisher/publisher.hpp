#ifndef adc_publisher_hpp
#define adc_publisher_hpp
#include <string>
#include <iostream>
#include <sstream>
#include <map>
#include <memory>
#include "adc/types.hpp"
#include "adc/builder/builder.hpp"

namespace adc {

/** \addtogroup API
 *  @{
 */


/**
 * Get the default processor affinity list.
 * This may be useful to publishers that spawn processes.
 */
std::string get_default_affinity();

inline version publisher_api_version("1.0.0", {"none"});

/*! \brief Publisher plugin interface.
 
Life-cycle of a plugin:
- p is created by a call to the factory. (there may be more than one instance per name).
- p->config is called 0 or 1 times (plugins must have reasonable default behavior).
- p->initialize is called exactly once
- p->publish is called 0 or more times
   - optionally, to enable dynamic application control of enabled logging:
       - p->pause is called to suppress emissions from subsequent calls to p->publish.
       - p->resume is called to unsuppress emissions from subsequent calls to p->publish.
- p->finalize is called.

A publisher object should not be assumed thread-safe. In particular,
there may be conflicts among config/initialize/publish/finalize calls
across multiple threads. Publishers which are thread-safe will document
that fact individually.
*/
class ADC_VISIBLE publisher_api
{
public:
	virtual ~publisher_api() {};
	/// \return the name of the plugin
	virtual std::string_view name() const = 0;

	/// \return the version of the plugin (should follow semantic versioning practices)
	virtual std::string_view version() const = 0;

	/// \brief Publish the content of the builder
	/// 
	/// \param b converted to a single json object and published
	virtual int publish(std::shared_ptr<builder_api> b) = 0;

	/// \brief Configure the plugin with the options given.
	/// \param m a map with keys documented in the plugin-specific header.
	///
	/// For plugin QQQ, Environment variables ADC_QQQ_PLUGIN_* will override the source code
	/// default for any key not defined in m. Here QQQ is the uppercase version of the
	/// plugin name.
	virtual int config(const std::map< std::string, std::string >& m) = 0;

	/// \brief Configure the plugin with the options given and the corresponding environment variables.
	/// \param m a map with keys documented in the plugin-specific header.
	/// \param env_prefix is prepended to the expected keys for the plugin
	/// and values found with getenv that match are used, overriding elements of m.
	/// Typically, env_prefix will be PPP_ADC_QQQ_PLUGIN_ if application PPP wants to 
	/// override the defaults of plugin QQQ. Here QQQ is the uppercase version of the
	/// plugin name.
	virtual int config(const std::map< std::string, std::string >& m, std::string_view env_prefix) = 0;

	/// \brief Look up the settable options and their defaults.
	///
	/// Some plugins without options will return an empty map.
	virtual const std::map< const std::string, const std::string> & get_option_defaults() = 0;

	/// \brief Ready the plugin to publish following the configuration options set or defaulted.
	virtual int initialize() = 0; // TODO: add void* mpi_comm_p=NULL arg?

	/// \brief Stop publishing and release any resources held for managing publication.
	virtual void finalize() = 0;

	/// \brief Pause publishing until a call to resume.
	/// Duplicate calls are allowed.
	virtual void pause() = 0;

	/// \brief Resume publishing
	/// Duplicate calls are allowed.
	virtual void resume() = 0;

};

/// \brief name/plugin map.
typedef std::map< const std::string, std::shared_ptr<publisher_api> > plugin_map;

/// \brief list of publishers
typedef std::vector< std::shared_ptr<publisher_api> > publisher_vector;

/// \brief named publisher
typedef std::pair<std::string, std::shared_ptr<publisher_api> > pair_string_publisher_api;

/** @}*/

} // namespace adc
#endif // adc_publisher_hpp
