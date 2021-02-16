#ifndef WHISPERLIB_IO_PATH_H_
#define WHISPERLIB_IO_PATH_H_

#include <string>
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace whisper {
namespace path {

#if defined _WIN32 && !defined(DIR_SEPARATOR)
#  define DIR_SEPARATOR               '\\'
#endif

#ifndef DIR_SEPARATOR
#  define DIR_SEPARATOR               '/'
#endif

static constexpr char kDirSeparator = DIR_SEPARATOR;
static constexpr char kDirSeparatorStr[] = { DIR_SEPARATOR, '\0' };

absl::string_view Basename(absl::string_view path);
absl::string_view Dirname(absl::string_view path);

std::string Join(absl::string_view path1,
                 absl::string_view path2,
                 char path_sepparator);

inline std::string Join(absl::string_view path1,
                        absl::string_view path2) {
  return Join(path1, path2, kDirSeparator);
}

inline std::string Join(absl::string_view path1,
                        absl::string_view path2,
                        absl::string_view path3) {
  return Join(Join(path1, path2), path3);
}

std::string Join(absl::Span<std::string> paths);
std::string Join(std::initializer_list<absl::string_view> paths);

//  Normalizes a file path (collapses ../, ./ // etc)  but leaves
// all the prefix '/'  ( with a custom path separator character ).
std::string Normalize(absl::string_view path, char sep = kDirSeparator);

}  // namespace path
}  // namespace whisper

#endif  // WHISPERLIB_IO_PATH_H_
