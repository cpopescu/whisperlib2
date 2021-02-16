#ifndef WHISPERLIB_IO_FILESYSTEM_H_
#define WHISPERLIB_IO_FILESYSTEM_H_

#include <sys/stat.h>

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"

namespace whisper {
namespace io {

// Returns true if provided path exists, and is a directory.
bool IsDir(absl::string_view path);
// Returns true if provided path exists, is a file, and is readable.
bool IsReadableFile(absl::string_view path);
// Returns true if provided path exists, and is a symlink.
bool IsSymlink(absl::string_view path);
// Returns true if provided path exists.
bool Exists(absl::string_view path);

// Returns the size of the file at path.
absl::StatusOr<int64_t> GetFileSize(absl::string_view path);
// Returns the modification time for a file.
absl::StatusOr<absl::Time> GetFileModTime(absl::string_view path);

// Removes a normal file.
absl::Status RmFile(absl::string_view path);
// Removes a director - must be empty. Can call RmFilesUnder to clean the
// underlying files before this.
absl::Status RmDir(absl::string_view path);
// Removes all files under the given directory - but not the directory itself.
// (NOTE: just the files if rm_dirs is false)
absl::Status RmFilesUnder(absl::string_view path, bool rm_dirs = false);

// Move src_path file or directory to destination directory.
// If a directory with the same name already exists, the source directory
// will be integrated in the destination directory. See function Rename(..) .
//
// e.g. path = "/tmp/abc.avi"
//      dir = "/home/my_folder"
//      Will move file "abc.avi" to "/home/my_folder/abc.avi"
//      and "/tmp/abc.avi" no longer exists.
absl::Status Mv(absl::string_view src_path, absl::string_view dest_dir,
                bool overwrite = false);
// Renames a file. This is atomic on Linux in all cases. On Windows,
// if new_path exists, and overwrite is true, we need to remove the new_path
// first, so the operation is **NOT** atomic.
absl::Status Rename(absl::string_view old_path,
                    absl::string_view new_path,
                    bool overwrite = false);

// Creates a symbolic link link_path, pointing to target_path.
// Note: the order differs from normal symlink !
absl::Status Symlink(absl::string_view link_path,
                     absl::string_view target_path);

static constexpr mode_t kDefaultMode = (
  S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);

// Creates a directory on disk.
// recursive: if true => creates all directories on path "dir"
//            if false => creates only "dir"; it's parent must exist.
absl::Status MkDir(absl::string_view dir, bool recursive = false,
                   mode_t mode = kDefaultMode);

// List a directory, possibly looking into subdirectories, up to a depth.
// Symlinks are not followed, and completely ignored.
//  * list_attr: a combinations of 1 or more DirListAttributes;
//  * depth: for 0 lists just dir; for 1 lists dir and one level of its
//     sub-directories, etc.
// Returned entries are relative to 'dir' (they do not contain the 'dir').
//
enum DirListAttributes {
  // return regular files & symlinks
  LIST_FILES = 0x01,
  // return directories
  LIST_DIRS = 0x02,
  // return everything (files, dirs, symlinks, sockets, pipes,..)
  LIST_EVERYTHING = 0x0f,
  // look into subdirectories
  LIST_RECURSIVE = 0x80,
};
absl::StatusOr<std::vector<std::string>> DirList(
  absl::string_view dir, uint32_t list_attr, size_t max_depth = 20);

}  // namespace io
}  // namespace whisper

#endif  // WHISPERLIB_IO_FILESYSTEM_H_
