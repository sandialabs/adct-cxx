/* Copyright 2025 NTESS. See the top-level LICENSE.txt file for details.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <adc/builder/builder.hpp>
#include <adc/publisher/publisher.hpp>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <chrono>
// for glob_sendfile_join
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <glob.h>


namespace adc {

using std::string;
using std::string_view;


/*! \brief Parallel file output publisher_api implementation.
  This plugin generates writes each message to the configured directory
  tree with \<adct-json>\</adct-json> delimiters surrounding it.
  The output directory is "." by default, but may be overriden with 
  a full path defined in env("ADC_MULTIFILE_PLUGIN_DIRECTORY").
  The resulting mass of files can be reduced independently later
  by concatenating all files in the tree or more selectively
  with a single call to the adc::consolidate_multifile_logs() function.

  Multiple independent multifile publishers may be created; exact filenames
  are not user controlled, to avoid collisions. Likewise there is no append
  mode.

  DIR/$user/[$adc_wfid.]H_$host.P$pid.T$start.$publisherptr/$application.$rank.XXXXXX

  Files opened will remain opened until the publisher is finalized.
 */
class multifile_plugin : public publisher_api {
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
	inline static const std::map< const string, const string > plugin_multifile_config_defaults =
		{
			{ "DIRECTORY", "."},
			{ "DEBUG", "0"},
			{ "RANK", ""}
		};
	inline static const char *plugin_multifile_prefix = "ADC_MULTIFILE_PLUGIN_";
	const string vers;
	const std::vector<string> tags;
	string fdir;
	string topdir;
	string user;
	string rank;
	std::map< std::string, std::unique_ptr< std::ofstream > > app_out;
	int debug;
	enum state state;
	bool paused;
	enum mode mode;

	int config(const string dir, const string user_rank, const string sdebug) {
		if (mode != pi_config)
			return 2;
		char hname[HOST_NAME_MAX+1];
		char uname[L_cuserid+1];
		if (gethostname(hname, HOST_NAME_MAX+1)) {
			return 2;
		}
		if (!cuserid(uname)) {
			return 2;
		}
		user = uname;
		std::stringstream dss(sdebug);
		dss >> debug;
		if (debug < 0) {
			debug = 0;
		}
		std::stringstream ss;
		// output goes to
		// dir/user/[wfid.].H_host.Ppid.Tstarttime.pptr/application.Rrank.XXXXXX files
		// where application[.rank].XXXXXX is opened per process
		// and application comes in header and rank via config.
		// Here we set up dir/user/[wfid.].H_host.Ppid.Tstarttime.pptr/ in fdir
		// and later we add application.rank.XXXXXX 
		string wname;
		char *wfid = getenv("ADC_WFID");
		if (wfid) {
			wname = string("wfid_") + string(wfid) + ".";
		} else {
			wname = "";
		}
		pid_t pid = getpid();
		struct timespec ts;
		clock_gettime(CLOCK_BOOTTIME, &ts);
		ss << dir << "/" << uname << "/" << wname <<
			"H_"<< hname << ".P" << pid << ".T" << 
			ts.tv_sec << "." << ts.tv_nsec << ".p" << this;
		ss >> fdir;
		topdir = dir;
		rank = user_rank;
		mode = pi_init;

		if (debug > 0) {
			std::cerr << "multifile plugin configured" <<std::endl;
		}
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
			return plugin_multifile_config_defaults.at(field);
		} else {
			return string(ec);
		}
	}
	
	/** Create fdir path following perm/group of dir or dir/user
	 * or dirname(fdir), depending on which already exists.
	 * The aim is to preserve group policy set at dir/user as other items
	 * below get created instead of defaulting to user:user;600.
	 */
	void create_directories(const string& dir, const string& user, const string& fdir, std::error_code& ec) {
		namespace fs = std::filesystem;
		// return if exists already
		auto status_fdir = fs::status(fdir, ec);
		if (fs::exists(status_fdir)) {
			if (fs::is_directory(status_fdir) ) {
				return;
			} else {
				ec = make_error_code(std::errc::not_a_directory);
				return;
			}
		}
		// if dirname(fdir), create last subdir
		auto fpath = fs::path(fdir);
		auto bdir = fpath.parent_path();
		auto status_bdir = fs::status(bdir);
		if (fs::exists(status_bdir)) {
			if (fs::is_directory(status_bdir) ) {
				fs::create_directory(fdir, bdir, ec);
				return;
			} else {
				ec = make_error_code(std::errc::not_a_directory);
				return;
			}
		}

		// if dir/user exists, create rest w/perm, sticky from user/
		fs::path du_path = dir;
		du_path /= user;
		auto status_udir = fs::status(du_path);
		if (fs::exists(status_udir)) {
			if (fs::is_directory(status_bdir) ) {
				fs::create_directory(fdir, du_path, ec);
				return;
			} else {
				ec = make_error_code(std::errc::not_a_directory);
				return;
			}
		}

		// if dir/ exists, create user and rest w/perm, sticky from dir/
		fs::path d_path = dir;
		auto status_dir = fs::status(d_path);
		if (fs::exists(status_dir)) {
			if (fs::is_directory(status_dir) ) {
				fs::create_directory(du_path, d_path, ec);
				if (ec) {
					return;
				}
				fs::create_directory(fdir, d_path, ec);
				if (ec) {
					return;
				}
				auto is_sticky = status_dir.permissions() & fs::perms::sticky_bit;
				fs::permissions(
				    fdir,
				    fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec | is_sticky,
				    fs::perm_options::replace,
				    ec
				);
				return;
			} else {
				ec = make_error_code(std::errc::not_a_directory);
				return;
			}
		}
		
		// create tree w/o1750
		fs::create_directory(dir, ec);
		if (ec) {
			return;
		}
		fs::permissions(
		    dir,
		    fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec | fs::perms::sticky_bit,
		    fs::perm_options::replace,
		    ec
		);
		fs::create_directory(du_path, dir, ec);
		if (ec) {
			return;
		}
		fs::create_directory(fdir, dir, ec);
		if (ec) {
			return;
		}
	}

	/** If not yet created, add an element to the app_out file mapping.
	 * The element added should always be checked before use.
	 * File is created with perm 640. Group will be inherited from the 
	 * file system parent if gid (+s) is set there.
	 */
	void create_stream(const string& application) {
		if (app_out.count(application)) {
			return;
		}
		std::stringstream ss;
		ss << fdir << "/" << application << ".R" << rank << ".XXXXXX";
		string fpath = ss.str();
		char *ftemplate = strdup(fpath.c_str());
		if (!ftemplate) {
			if (debug) {
				std::cerr << __FILE__ << ": out of memory" << std::endl;
			}
			app_out[application] = std::make_unique< std::ofstream >();
			return;
		}
		int fd = mkstemp(ftemplate);
		if (fd < 0) {
			if (debug) {
				std::cerr << __FILE__ << ": mkstemp failed for " << ftemplate << std::endl;
			}
			free(ftemplate);
			app_out[application] = std::make_unique< std::ofstream >();
			return;
		}
		close(fd);
		app_out[application] = std::make_unique< std::ofstream >(ftemplate,
				(std::ofstream::out | std::ofstream::trunc));
		std::filesystem::permissions(ftemplate,
					(
					std::filesystem::perms::owner_read |
					std::filesystem::perms::owner_write |
					std::filesystem::perms::group_write |
					std::filesystem::perms::group_read),
					std::filesystem::perm_options::add);
		free(ftemplate);
	}

public:
	multifile_plugin() : vers("1.0.0") , tags({"none"}), state(ok), paused(false), mode(pi_config) { }

	int publish(std::shared_ptr<builder_api> b) {
		if (paused)
			return 0;
		if (state != ok)
			return 1;
		if (mode != pi_pub_or_final)
			return 2;
		auto app = b->get_value_string("/header/application");
		if (!app) {
			auto fields = b->get_field_names();
			if (debug) {
				std::cerr << "fields: ";
				for (const auto& f : fields) {
					std::cerr << f << " ";
				}
				std::cerr << std::endl;
			}
			if (debug) {
				std::cerr << __FILE__ <<
					": cannot publish without /header/application"
				       	<< std::endl;
				std::cerr << b->serialize() << std::endl;
				std::cerr << "sections:" ;
				for (auto s : b->get_section_names()) {
					std::cerr << s << " ";
				}
				std::cerr << std::endl;
			}
			return 1;
		}
		
		create_stream(app);
		if (app_out[app]->is_open() && app_out[app]->good()) {
			*(app_out[app]) << string("<adct-json>")  << b->serialize() << "</adct-json>" << std::endl;
			if (debug) {
				std::cerr << "'multifile' wrote" << std::endl;
			}
			return 0;
		}
		if (debug) {
			std::cerr << __FILE__ << " failed out.good" << std::endl;
		}
		return 1;
	}

        int config(const std::map< std::string, std::string >& m) {
		return config(m, plugin_multifile_prefix);
	}

        int config(const std::map< std::string, std::string >& m, std::string_view env_prefix) {
		string d = get(m, "DIRECTORY", env_prefix);
		string r = get(m, "RANK", env_prefix);
		string l = get(m, "DEBUG", env_prefix);
		return config(d, r, l);
	}
        
	const std::map< const std::string, const std::string> & get_option_defaults() {
		return plugin_multifile_config_defaults;
	}
        
	int initialize() {
		std::map <string, string >m;
		if (!fdir.size())
			config(m);
		if (mode != pi_init) {
			return 2;
		}
		if ( state == err ) {
			if (debug) {
				std::cerr <<
					"multifile plugin initialize found pre-existing error"
					<< std::endl;
			}
			return 3;
		}
		std::error_code ec;
		create_directories(topdir, user, fdir, ec);
		if (ec.value() != 0 && ec.value() != EEXIST ) {
			state = err;
			if (debug) {
				std::cerr << "unable to create output directory for plugin 'multifile'; "
					<< fdir << " : " << ec.message() << std::endl;
			}
			return ec.value();
		}
		string testfile = fdir + "/.XXXXXX";
		auto ftemplate = std::make_unique<char[]>(testfile.length()+1);
		::std::strcpy(ftemplate.get(), testfile.c_str());
		int fd = mkstemp(ftemplate.get());
		int rc = errno;
		if (fd == -1) {
			state = err;
			if (debug) {
				std::cerr << "unable to open file in output directory "
					<< fdir << " for plugin 'multifile': " << 
					std::strerror(rc) << std::endl;
			}
			return 4;
		} else {
			close(fd);
			unlink(ftemplate.get());
		}
		mode = pi_pub_or_final;
		return 0;
	}

	void finalize() {
		if (mode == pi_pub_or_final) {
			state = ok;
			paused = false;
			mode = pi_config;
			app_out.clear();
		} else {
			if (debug) {
				std::cerr << "multifile plugin finalize on non-running plugin" << std::endl;
			}
		}
	}

	void pause() {
		paused = true;
	}

        void resume() {
		paused = false;
	}

	string_view name() const {
		return "multifile";
	}

	string_view version() const {
		return vers;
	}

	~multifile_plugin() {
		if (debug) {
			std::cerr << "Destructing multifile_plugin" << std::endl;
		}
	}

	inline static std::vector<std::string> consolidate_multifile_logs(
		const string& pattern, std::vector< std::string>& old_paths,
		int debug=0)
	{
		namespace fs = std::filesystem;
		std::vector<std::string> new_files;
		std::vector<std::string> unmerged;

		std::vector<std::string> parts;
		for (const auto& part : fs::path(pattern)) {
			parts.push_back(part.string());
		}
		// strip last 2: [wfid.]H_host.Pval.Tval.paddr/app.Rrank.unique
		auto appdir = fs::path(pattern).parent_path();
		auto appleafdir = parts.end()[-2];
		auto userdir = appdir.parent_path();
		auto pos = appleafdir.find(".H_");
		std::string prefix;
		if (pos != std::string::npos) {
			prefix = appleafdir.substr(0,pos);
		} else {
			const auto now = std::chrono::system_clock::now();
			const auto esec = std::chrono::duration_cast<
				std::chrono::seconds >( now.time_since_epoch());
			const long long isec = esec.count();
			prefix = std::to_string(isec);
		}
		// dir/user/[wfid.]H_host.Pval.Tval.paddr/app.Rrank.unique
		userdir /= (string("consolidated.") + prefix + ".adct-json.multi.xml");
		const char *dest = userdir.c_str(); 
		std::error_code ec;
		fs::perms perms = fs::status(appdir, ec).permissions();
		int perm = static_cast<int>(perms);
		if (ec) {
			perm = 0640;
		}
		int merge_err = glob_sendfile_join(dest, pattern.c_str(), perm, old_paths, unmerged);
		if (merge_err) {
			if (debug) {
				for (auto f : unmerged) {
					std::cerr << __FILE__ <<
						": unmergeable file: " <<
					       	f << std::endl;
				}
				for (auto f : old_paths) {
					std::cerr << __FILE__ << 
						": ignoring mergable file: "
					       	<< f << std::endl;
				}
			}
			old_paths.clear();
			return new_files;
		}
		new_files.push_back(dest);
		return new_files;
	}

	/*! Utility to get output directory glob pattern from the multifile_publisher.
	 */
	inline static std::string get_multifile_log_path(string_view dir, string_view wfid)
	{
		std::stringstream ss;
		char uname[L_cuserid+1];
		if (!cuserid(uname)) {
			return "";
		}
		string wname;
		if (wfid.size()) {
			wname = string("wfid_") + string(wfid) + ".";
		} else {
			wname = "";
		}
		// dir/user/[wfid.].H_host.Pval.Tval.paddr/app.Rrank.unique
		ss << dir << "/" << uname << "/" << wname << "H_*.P*.T*.p*/*.R*.*";
		string s;
		ss >> s;
		return s;
	}

	/*! \brief use sendfile to join all files matching pattern in new dest file.
	 * No checking of input format correctness is performed.
	 * Ignores directory names.
	 * \param dest file to write merge result.
	 * \param pattern glob pattern to match possible input files.
	 * \param perm permissions on dest file, subject to umask.
	 * \param merged list of files added
	 * \param unmerged list of files found but not added due to some error. 
	 *        If this list is not empty, the output file should be abandoned,
	 *        as it may contain a partially copied file.
	 * \return 0 and updated merged list or errno and file and merged vector content undefined.
	 */
	inline static int glob_sendfile_join(const char *dest, const char *pattern, 
			int perm, std::vector<std::string>& merged, std::vector<std::string>& unmerged)
	{
		glob_t files;
		merged.clear();
		int err = glob(pattern, GLOB_NOSORT|GLOB_TILDE, NULL, &files);
		switch(err) {
		case 0:
			break;
		case GLOB_NOSPACE:
			globfree(&files);
			return ENOMEM;
		case GLOB_ABORTED:
			globfree(&files);
			return EPERM;
		case GLOB_NOMATCH:
			globfree(&files);
			return 0;
		}

		int dest_fd = open(dest, O_WRONLY | O_CREAT |O_CLOEXEC | O_TRUNC , perm);
		if (dest_fd == -1) {
			globfree(&files);
			return errno;
		}

		for (size_t i = 0; i < files.gl_pathc; i++) {
			int src_fd = open(files.gl_pathv[i], O_RDONLY);
			if (src_fd == -1) continue;

			err = 0;
			struct stat stat_buf;
			if (fstat(src_fd, &stat_buf) == 0 && S_ISREG(stat_buf.st_mode)) {
				off_t offset = 0;
				size_t remaining = stat_buf.st_size;

				// Loop until the entire file is transferred
				while (remaining > 0) {
					ssize_t sent = sendfile(dest_fd, src_fd, &offset, remaining);

					if (sent <= 0) {
						// Handle errors or unexpected EOF
						if (sent == -1 && errno == EINTR) continue; // Interrupted by signal, retry
						err = errno;
						break;
					}
					remaining -= sent;
				}
				if (!err) {
					merged.push_back(files.gl_pathv[i]);
				} else {
					unmerged.push_back(files.gl_pathv[i]);
				}
			}
			close(src_fd);
		}

		close(dest_fd);
		globfree(&files);
		if (unmerged.size() != 0) {
			return EINVAL;
		}

		return 0;
	}

	inline static std::vector<size_t> validate_multifile_log(string_view /*filename*/, bool /*check_json*/, size_t & record_count)
	{
		std::vector<size_t> v;
		record_count = 0;
		//fixme validate_multifile_log
		// open file
		// state=closed
		// curpos = 0
		// begin = 0
		// while( next = find_next_regex <[/]adct-json>, curpos ) != end
			// if next[1] == 'a': # found start tag
			// 	if state == closed
			// 		state = opened
			// 		begin = curpos
			// 		curpos = after(next)
			// 	else # broken missing tail
			// 		state = closed
			// 		v.push_back(begin)
			// 		curpos = next
			// else # found end tag
			//      if state == opened
			//      	state = closed
			//      	record_count++
			//      	if check_json
			//      		parse check (after(begin), curpos-1)
			//      	curpos=after(next)
			//     	else # broken missing head
			//     		v.push_back(begin)
			//      	curpos=after(next)
		// if state != closed
		// 	extra at end of file.
		// 	v.push_back(begin)
		// else
		//	 if !eof
		//	 	trailing junk error
		return v;
	}
};

} // adc
