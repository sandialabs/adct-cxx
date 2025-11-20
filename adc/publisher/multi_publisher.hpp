/* Copyright 2025 NTESS. See the top-level LICENSE.txt file for details.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef adc_multi_publisher_hpp
#define adc_multi_publisher_hpp
#include <string>
#include <iostream>
#include <sstream>
#include <map>
#include <memory>
#include "adc/types.hpp"
#include "adc/builder/builder.hpp"
#include "adc/publisher/publisher.hpp"
#define MULTI_PUBLISHER_VERSION "0.0.0"
#define MULTI_PUBLISHER_TAGS {"none"}

namespace adc {


/** \addtogroup API
 *  @{
 */

inline version multi_publisher_version(MULTI_PUBLISHER_VERSION, MULTI_PUBLISHER_TAGS);

/*! \brief Interface for a group of publishers all being fed the same message(s).
 
  */
class ADC_VISIBLE multi_publisher_api
{
public:
	/// \brief Get the version.
        virtual std::string_view version() const = 0;

	/// \brief Add a configured and initialized publisher
	virtual void add(std::shared_ptr<publisher_api> pub) = 0;

	/// \brief Finalize all added publishers
	virtual void terminate() = 0;

	/// \brief Publish the same message to all added publishers.
        virtual int publish(std::shared_ptr<builder_api> b) = 0;

	/// \brief Pause all publishers
        virtual void pause() = 0;

	/// \brief Resume all publishers
        virtual void resume() = 0;


	virtual ~multi_publisher_api() {}
};

/** @}*/

} // namespace adc
#endif // adc_publisher_hpp
