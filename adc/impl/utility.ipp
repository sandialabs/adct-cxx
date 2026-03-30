/* Copyright 2025 NTESS. See the top-level LICENSE.txt file for details.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef adc_utility_ipp
#define adc_utility_ipp
#include <string>
#include <string_view>
#include <map>
#include <set>

namespace adc {

/*! Utility to merge parallel outputs from multifile publisher.
 * Consolidation should only happen after the multifile publisher is finalized;
 * If a multi_publisher is in use, consolidation should only happen after
 * terminate has been called on the multi_publisher (which calls finalize).
 * On slow nfs-based file systems, some delay may be needed to ensure all
 * files are seen on the aggregating client.
 *
 * \param match glob expression from get_multifile_log_path, or user edit thereof.
 * \param old_paths (output) names of files merged that caller may remove.
 * \param debug make more diagnostic noise if true.
 *
 * \return the list of consolidated logs from reduction of a multifile publisher tree.
 *
 * /FS/adc/$user/$adc_wfid/$app/$host-$pid_$start-$publisherptr.log
 * /FS/adc/$user/no_wfid/$app/$host-$pid_$start-$publisherptr.log
 * /FS/adc/$user/$cluster/$jobidraw/$task/$rank/$app/$host-$pid_$start-$publisherptr.log
 * /FS/adc/ is multiuser; $user is owned/writable by $user
 */
	
ADC_VISIBLE std::vector<std::string> consolidate_multifile_logs(
		const std::string& match, std::vector< std::string>& old_paths, bool debug=false)
{
	return adc::multifile_plugin::consolidate_multifile_logs(match, old_paths, debug);
}

/*! Utility to get output directory from the multifile_publisher.
 */
ADC_VISIBLE std::string get_multifile_log_path(string_view dir, string_view wfid)
{
	return adc::multifile_plugin::get_multifile_log_path(dir, wfid);
}

ADC_VISIBLE std::vector<size_t> validate_multifile_log(string_view filename, bool check_json, size_t & record_count)
{
	return adc::multifile_plugin::validate_multifile_log(filename, check_json, record_count);
}

} // namespace adc
#endif // adc_utility_ipp
