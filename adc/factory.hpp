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

/** \addtogroup API
 *  @{
 */
inline version adc_factory_version("1.0.0", {"none"});

// /FS/adc-log/$user/$adc_wfid/$app/$host-$pid_$start-$publisherptr.log
// /FS/adc-log/$user/no_wfid/$app/$host-$pid_$start-$publisherptr.log
// /FS/adc-log/$user/$cluster/$jobidraw/$task/$rank/$app/$host-$pid_$start-$publisherptr.log
// get the list of consolidated logs from reduction of a multifile publisher tree.
// FS/adc-log/ is multiuser; $user is owned/writable by $user
std::vector<std::string> consolidate_multifile_logs(std::string_view dir, std::string_view match);

/*!
  \brief provides publishers and builders of application metadata.

This class provides builders of abstracted metadata objects
and publishers. Generally an application will create and configure
one publisher (or a multipublisher) and as many builder objects
as needed. In most cases, a new builder object is used for each published event.
 */
class ADC_VISIBLE factory {
public:
	/// \return a publisher instance
	/// \param name one of the elements found with get_publisher_names().
	std::shared_ptr<publisher_api> get_publisher(const std::string& name);
	
	/** \return a publisher instance, or an empty pointer if the name is unavailable.
	    \param name one of the elements found with get_publisher_names().
	    \param opts a map of option names and their values; plugins will
	    silently ignore unused options.
	  */
	std::shared_ptr<publisher_api> get_publisher(const std::string& name, const std::map<std::string, std::string>& opts);

	/** \return the names of publishers available.
	    The "none", "stdout", "file", "multifile", "syslog", and "script" publishers are always available
	    in Linux environments.
	 */
	const std::set<std::string>& get_publisher_names();

	/** \return a multipublisher instance for populating.
	   
	    multi_publisher provides a convenient way to publish on multiple
	    publishers without duplicate calls to a publish() function.
	 */
	std::shared_ptr<multi_publisher_api> get_multi_publisher();

	/** \return a multipublisher instance pre-populated from environment settings.
	   
	    This function creates a multi_publisher pre-configured with the
	    publisher plugins named in the input vector. If the input vector is empty,
	    publishers named in the environment variable ADC_MULTI_PUBLISHER_NAMES are created.
	    Plugin names in the environment variable are colon-separated.
	    In both cases, the publishers are configured using their defaults and
	    plugin-specific environment variables.
	 */
	std::shared_ptr<multi_publisher_api> get_multi_publisher(std::vector< std::string > & plugin_list);

	/// \return an empty json builder object
	std::shared_ptr<builder_api> get_builder();

private:
	std::set<std::string> names; //!< the list of publisher names
	void init();

};  // class factory

/** @}*/

} // namespace adc
#endif // adc_factory_hpp
