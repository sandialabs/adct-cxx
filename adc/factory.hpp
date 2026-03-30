/* Copyright 2025 NTESS. See the top-level LICENSE.txt file for details.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef adc_factory_hpp
#define adc_factory_hpp
#include <string>
#include <string_view>
#include <map>
#include <set>
#include "adc/types.hpp"
#ifdef ADC_HAVE_MPI
#include <mpi.h>
#endif
#include "adc/builder/builder.hpp"
#include "adc/publisher/publisher.hpp"
#include "adc/publisher/multi_publisher.hpp"

namespace adc {

/** @addtogroup API
 *  @{
 */
inline version adc_factory_version("1.0.0", {"none"});

/*!
  @brief provides publishers and builders of application metadata.

This class provides builders of abstracted metadata objects
and publishers. Generally an application will create and configure
one publisher (or a multipublisher) and as many builder objects
as needed. In most cases, a new builder object is used for each published event.
 */
class ADC_VISIBLE factory {
public:
	/** @brief Get an instance of the named publisher type.

	@return A publisher instance
	@param name one of the elements found with get_publisher_names().
	*/
	std::shared_ptr<publisher_api> get_publisher(const std::string& name);
	
	/** @brief Get an instance of the named publisher type, configured per opts given.

	@return A publisher instance, or an empty pointer if the name is unavailable.
	@param name one of the elements found with get_publisher_names().
	@param opts a map of option names and their values; plugins will
	silently ignore unused options.
	  */
	std::shared_ptr<publisher_api> get_publisher(const std::string& name, const std::map<std::string, std::string>& opts);

	/** @brief Get the names of publishers available.

	@return The names of publishers available.
	The "none", "stdout", "file", "multifile", "syslog", and "script" publishers are always available
	in Linux environments.
	 */
	const std::set<std::string>& get_publisher_names();

	/** @brief Get an empty multipublisher for populating.
	
	@return An empty multipublisher instance for populating.
	   
	Multi_publisher provides a convenient way to publish on multiple
	publishers without duplicate calls to a publish() function.
	Before any publisher is added to the returned multipublisher,the
	publisher must have a config method and initialize called.
	*/
	std::shared_ptr<multi_publisher_api> get_multi_publisher();

	/** @brief Configure a custom multipublisher from a map of plugin configurations.

	@param plugins_map A map of plugin name : option map, as documented in the get_publisher method.
	@return a multipublisher instance pre-populated from maps.
	   
	This function creates a multi_publisher configured with the
	publisher plugins named in the input map of maps.
	 */
	std::shared_ptr<multi_publisher_api> get_multi_publisher(std::map< const std::string, const std::map<std::string, std::string> > plugins_map);

	/** @brief Configure a multipublisher from an environment list of plugins.

	@return a multipublisher instance pre-populated using elements of env_name.
	   
	This function creates a multi_publisher pre-configured with the
	publisher plugins named in the given environment variable.
	If the environment variable given is the empty string, then
	publishers named in the environment variable ADC_MULTI_PUBLISHER_NAMES are created.
	Plugin names in the environment variable are colon-separated.
	In either case, the publishers are configured using their defaults and
	plugin-specific environment variables documented by each plugin implementation.
	 */
	std::shared_ptr<multi_publisher_api> get_multi_publisher_from_env(const std::string& env_name);

	/** @brief Configure a multipublisher from a vector of plugin names.

	@return a multipublisher instance pre-populated from environment settings for the listed plugins.
	   
	This function creates a multi_publisher pre-configured with the
	publisher plugins named in the input vector. If the input vector is empty,
	this is equivalent to get_multi_publisher().
	 */
	std::shared_ptr<multi_publisher_api> get_multi_publisher_from_env(std::vector< std::string > & plugin_list);

	/** @brief Get an empty message or subsection object.

	@return an empty json builder object
	*/
	std::shared_ptr<builder_api> get_builder();

private:
	std::set<std::string> names; //!< the list of publisher names
	int debug;
	void init();

};  // class factory

/** @}*/

} // namespace adc
#endif // adc_factory_hpp
