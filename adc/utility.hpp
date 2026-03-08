/* Copyright 2025 NTESS. See the top-level LICENSE.txt file for details.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef adc_utility_hpp
#define adc_utility_hpp
#include <string>
#include <string_view>
#include <map>
#include <set>

namespace adc {

/** \addtogroup API
 *  @{
 */
inline version adc_utility_version("1.0.0", {"none"});

/*! Utility to get output directory glob pattern from the multifile_publisher, given the
 * ADC_MULTIFILE_DIRECTORY and ADC_WFID values in use or their library-prefixed-versions.
 */
ADC_VISIBLE std::string get_multifile_log_path(std::string_view dir, std::string_view wfid);

/*! Utility to merge parallel outputs from multifile publisher.
 * Consolidation should only happen after the multifile publisher is finalized;
 * If a multi_publisher is in use, consolidation should only happen after
 * terminate has been called on the multi_publisher (which calls finalize).
 * On slow nfs-based file systems, some delay may be needed to ensure all
 * files are seen on the aggregating client.
 * \param match the result of an appropriate call to get_multifile_log_path.
 * \param old_paths the names of files that were successfully merged. May be empty;
 *        if not empty, deleting the files in the list is recommended.
 *
 * \return the list of new consolidated logs from reduction of a multifile publisher tree.
 *
 * Example:
 * /FS/adc/ may be multiuser
 * /FS/adc/$user is owned/writable by $user
 * /FS/adc/$user/$adc_wfid.* are created to prevent output conflicts, and then consolidated
 * to:
 * /FS/adc/$user/consolidated.$adc_wfid.adct-json.multi.xml
 * Where ADC_WFID is not defined, a timestamp is used in its place.
 */
ADC_VISIBLE std::vector< std::string > consolidate_multifile_logs(const std::string& match, std::vector< std::string>& old_paths, bool debug=false);

/*! Utility to validate that multi-record files were correctly written.
 * Each record is a json object delimited by <adct-json></adct-json> xml tags.
 * \return vector of the start positions of invalid records.
 * \param filename input to check.
 * \param check_json validates that individual record contents are json formatted (but does not
 *        check adct schema compliance).
 * \param count output of number of valid records found.
 *
 * <adc-json> tags cannot be nested. Unclosed tags are assumed closed by the start of the next record.
 * 
 * BUGS:
 * not yet implemented. returns emtpy vector and 0 record_count until implemented.
 */
ADC_VISIBLE std::vector<size_t> validate_multifile_log(std::string_view filename, bool check_json, size_t & record_count);
/** @}*/
} // namespace adc
#endif // adc_utility_hpp
