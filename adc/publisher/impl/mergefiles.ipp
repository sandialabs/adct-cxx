#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <glob.h>
#include <cerrno>
#include <vector>
#include <string>

/*! \brief use sendfile to join all files matching pattern in new dest file.
 * \param dest file to write merge result.
 * \param pattern glob pattern to match possible input files.
 * \param merged list of files added
 * \param perm permission bits for opening the file; modified by umask.
 * \return 0 and updated merged list or errno and merged content undefined.
 */
int glob_sendfile_join(const char *dest, const char *pattern, int perm, std::vector<std::string>& merged)
{
	glob_t glob_results;
	merged.clear();
	int err = glob(pattern, GLOB_NOSORT|GLOB_TILDE, NULL, &glob_results);
	switch(err) {
	case 0:
		break;
	case GLOB_NOSPACE:
		globfree(&glob_results);
		return ENOMEM;
	case GLOB_ABORTED:
		globfree(&glob_results);
		return EPERM;
	case GLOB_NOMATCH:
		globfree(&glob_results);
		return 0;
	}

	int dest_fd = open(dest, O_WRONLY | O_CREAT |O_CLOEXEC | O_TRUNC , perm);
	if (dest_fd == -1) {
		globfree(&glob_results);
		return errno;
	}

	for (size_t i = 0; i < glob_results.gl_pathc; i++) {
		int src_fd = open(glob_results.gl_pathv[i], O_RDONLY);
		if (src_fd == -1) continue;

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
					break;
				}
				remaining -= sent;
			}
		}
		close(src_fd);
		merged.push_back(glob_results.gl_pathv[i]);
	}

	close(dest_fd);
	globfree(&glob_results);
	return 0;
}

