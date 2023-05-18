#include "whisperlib/io/filesystem.h"

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#include <utility>

#include "whisperlib/io/errno.h"
#include "whisperlib/io/path.h"
#include "whisperlib/status/status.h"

namespace whisper {
namespace io {

bool IsDir(absl::string_view path) {
  std::string path_str(path);
  struct stat st;
  if (0 != ::stat(path_str.c_str(), &st)) {
    return false;
  }
  return S_ISDIR(st.st_mode);
}

bool IsReadableFile(absl::string_view path) {
  std::string path_str(path);
  struct stat st;
  if (0 != ::stat(path_str.c_str(), &st)) {
    return false;
  }
  return S_ISREG(st.st_mode);
}

bool IsSymlink(absl::string_view path) {
  std::string path_str(path);
  struct stat st;
  // NOTE: ::stat queries the file referenced by link, not the link itself.
  if (0 != ::lstat(path_str.c_str(), &st)) {
    return false;
  }
  return S_ISLNK(st.st_mode);
}

bool Exists(absl::string_view path) {
  std::string path_str(path);
  struct stat st;
  return (0 == ::lstat(path_str.c_str(), &st));
}

absl::StatusOr<int64_t> GetFileSize(absl::string_view path) {
  std::string path_str(path);
  struct stat st;
  if ( 0 != ::stat(path_str.c_str(), &st) ) {
    return status::FailedPreconditionErrorBuilder()
      << "Error checking size of file named `" << path << "`";
  }
  return st.st_size;
}

absl::StatusOr<absl::Time> GetFileModTime(absl::string_view path) {
  std::string path_str(path);
  struct stat st;
  if ( 0 != ::stat(path_str.c_str(), &st) ) {
    return status::FailedPreconditionErrorBuilder()
      << "Error checking mod time of file named `" << path << "`";
  }
  return absl::FromTimeT(st.st_mtime);
}

namespace {
absl::Status CreateRecursiveDirs(absl::string_view path, mode_t mode) {
  std::string crt_dir(path::Normalize(path));
  if (crt_dir.empty()) {
    return absl::OkStatus();
  }
  if (*crt_dir.rbegin() == path::kDirSeparator) {
    crt_dir.resize(crt_dir.size() - 1);   // cut any trailing '/'
  }
  std::vector<std::string> to_create;
  while (!crt_dir.empty()) {
    if (IsDir(crt_dir)) {
      break;
    }
    if (IsReadableFile(crt_dir)) {
      return status::FailedPreconditionErrorBuilder()
        << "Cannot create directory `" << path << "` as path: `" << crt_dir
        << "` is a file.";
    }
    to_create.push_back(crt_dir);
    crt_dir = std::string(path::Dirname(crt_dir));
  }
  // Create from top to bottom:
  std::reverse(to_create.begin(), to_create.end());
  for (const auto& crt_path : to_create) {
    const int result = ::mkdir(crt_path.c_str(), mode);
    if (result != 0) {
      return error::ErrnoToStatus(errno)
        <<  "Error creating directory: `" << crt_path << "`";
    }
  }
  return absl::OkStatus();
}
}  // namespace

absl::Status MkDir(absl::string_view dir, bool recursive, mode_t mode) {
  if (recursive) {
    return CreateRecursiveDirs(dir, mode);
  }
  const std::string dir_str(path::Normalize(dir));
  const int result = ::mkdir(dir_str.c_str(), mode);
  if (result != 0 && errno != EEXIST) {
    return error::ErrnoToStatus(errno)
      << "Failed to create dir: `" << dir << "`";
  }
  return absl::OkStatus();
}

absl::Status RmFile(absl::string_view path) {
  std::string path_str(path);
  struct stat s;
  // NOTE: if path is a symbolic link:
  //       - lstat() doesn't follow symbolic links. It returns stats for
  //                 the link itself.
  //       - stat() follows symbolic links and returns stats for the linked file
  if (::lstat(path_str.c_str(), &s)) {
    if (errno == ENOENT) {
      return absl::OkStatus();
    }
    return error::ErrnoToStatus(errno)
      << "lstat failed for path to be remove: `" << path << "`";
  }
  if (S_ISREG(s.st_mode) || S_ISLNK(s.st_mode)) {
    if (::unlink(path_str.c_str())) {
      return error::ErrnoToStatus(errno)
        << "unlink failed for path: `" << path << "`";
    }
    return absl::OkStatus();
  }
  if (S_ISDIR(s.st_mode)) {
    return RmDir(path);
  }
  return status::UnimplementedErrorBuilder()
    << "Cannot remove file: `" << path << "` per unsupported mode: "
    << s.st_mode;
}

absl::Status RmDir(absl::string_view path) {
  std::string path_str(path);
  const int result = ::rmdir(path_str.c_str());
  if (result != 0) {
    return error::ErrnoToStatus(errno)
      << "rmdir failed for path: `" << path << "`";
  }
  return absl::OkStatus();
}

absl::Status RmFilesUnder(absl::string_view path, bool rm_dirs) {
  int options = 0;
  if (rm_dirs) {
    options = LIST_DIRS | LIST_FILES | LIST_RECURSIVE;
  } else {
    options = LIST_FILES | LIST_RECURSIVE;
  }
  if (!IsDir(path)) {
    return status::NotFoundErrorBuilder()
      << "RmFilesUnder directory `" << path << "` - cannot be found.";
  }
  ASSIGN_OR_RETURN(
    std::vector<std::string> files, DirList(path, options),
    _ << "While trying to delete files under: `" << path << "`");
  absl::Status rm_status;
  std::vector<std::string> dirs;
  for (const auto& file : files) {
    std::string f(path::Join(path, absl::string_view(file)));
    if (IsDir(f)) {
      if (rm_dirs) {
        dirs.emplace_back(std::move(f));
      }
    } else {
      auto file_rm_status = RmFile(f);
      if (!file_rm_status.ok()) {
        status::UpdateOrAnnotate(rm_status, file_rm_status);
      }
    }
  }
  // Reverse the dirs - so we remove the deep ones first :)
  std::reverse(dirs.begin(), dirs.end());
  for (const auto& dir : dirs) {
    auto dir_rm_status = RmDir(dir);
    if (!dir_rm_status.ok()) {
      status::UpdateOrAnnotate(rm_status, dir_rm_status);
    }
  }
  return rm_status;
}

absl::Status Mv(absl::string_view src_path,
                absl::string_view dest_dir,
                bool overwrite) {
  return Rename(src_path,
                path::Join(dest_dir, path::Basename(src_path)),
                overwrite);
}

absl::Status Rename(absl::string_view old_path,
                    absl::string_view new_path,
                    bool overwrite) {
  const bool old_exists = Exists(old_path);
  const bool old_is_file = IsReadableFile(old_path);
  const bool old_is_dir = IsDir(old_path);
  const bool old_is_symlink = IsSymlink(old_path);
  const bool old_is_single = old_is_file || old_is_symlink;
  const std::string old_type = (old_is_symlink ? "symlink" :
                                old_is_file ? "file" :
                                old_is_dir ? "directory" :
                                "unknown");

  const bool new_exists = Exists(new_path);
  const bool new_is_file = IsReadableFile(new_path);
  const bool new_is_dir = IsDir(new_path);
  const bool new_is_symlink = IsSymlink(new_path);
  const bool new_is_single = new_is_file || new_is_symlink;
  const std::string new_type = (new_is_symlink ? "symlink" :
                                new_is_file ? "file" :
                                new_is_dir ? "directory" :
                                "unknown");


  if (!old_exists) {
    return status::NotFoundErrorBuilder()
      << "Rename old_path: `" << old_path << "` does not exist";
  }
  if ((old_is_single && new_is_dir) ||
      (old_is_dir && new_is_single)) {
    return status::FailedPreconditionErrorBuilder()
      << "Rename old_path: `" << old_path << "`(" << old_type
      << "), new_path: `" << new_path << "`(" << new_type
      << ") incompatible types";
  }
  if (new_exists && new_is_single) {
    if (!overwrite) {
      return status::FailedPreconditionErrorBuilder()
        << "Rename old_path: `" << old_path << "`(" << old_type
        << ") , new_path: `" << new_path << "`(" << new_type
        << ") cannot overwrite";
    }
  }

  // - move file or directory over empty_path
  // - or move file over file
  //
  //  ! Atomically ! (on Linux)
  //
  if (!new_exists || (old_is_single && new_is_single)) {
    std::string old_path_str(old_path);
    std::string new_path_str(new_path);
    if (::rename(old_path_str.c_str(), new_path_str.c_str())) {
      return error::ErrnoToStatus(errno)
        << "::rename failed for old_path: `" << old_path << "`"
        << ", new_path: `" << new_path << "`";
    }
    return absl::OkStatus();
  }
  return status::UnimplementedErrorBuilder()
    << "Rename old_path: `" << old_path << "`(" << old_type
    << ") , new_path: `" << new_path << "`(" << new_type;
}

absl::Status Symlink(absl::string_view link_path,
                     absl::string_view target_path) {
  std::string link_path_str(link_path);
  std::string target_path_str(target_path);
  if (::symlink(target_path_str.c_str(), link_path_str.c_str()) != 0) {
    return error::ErrnoToStatus(errno)
      << "linking `" << link_path << "` to target `" << target_path << "`";
  }
  return absl::OkStatus();
}

absl::StatusOr<std::vector<std::string>> DirList(
  absl::string_view dir, uint32_t list_attr, size_t max_depth) {
  std::string dir_str(dir);
  DIR* dirp = ::opendir(dir_str.c_str());

  if (dirp == nullptr) {
    return error::ErrnoToStatus(errno)
      << "::opendir failed for dir: `" << dir << "`";
  }
  std::vector<std::string> out;
  while (true) {
    struct dirent* entry = ::readdir(dirp);
    if (entry == nullptr) {
      break;
    }
    absl::string_view basename(entry->d_name);
    // Skip dots - self / parent directories:
    if (basename.empty() || basename == "." || basename == "..") {
      continue;
    }

    struct stat st;
    const std::string abs_path = path::Join(dir, basename);
    // ::lstat does not follow symlinks. It returns stats for the link itself.
    if ( 0 != ::lstat(abs_path.c_str(), &st) ) {
      // We just skip this error for now.
      continue;
    }
    // maybe accumulate entry
    if ( (list_attr & LIST_EVERYTHING) == LIST_EVERYTHING ||
         ((list_attr & LIST_FILES) && (S_ISREG(st.st_mode)
                                       || S_ISLNK(st.st_mode))) ||
         ((list_attr & LIST_DIRS) && S_ISDIR(st.st_mode)) ) {
      out.push_back(std::string(basename));
    }
    // recursive listing
    if (max_depth > 0
        && (list_attr & LIST_RECURSIVE) && S_ISDIR(st.st_mode)) {
      ASSIGN_OR_RETURN(std::vector<std::string> subitems,
                       DirList(abs_path, list_attr, max_depth - 1));
      for (absl::string_view subitem : subitems) {
        out.emplace_back(path::Join(basename, subitem));
      }
    }
  }
  ::closedir(dirp);
  return out;
}

}  // namespace io
}  // namespace whisper
