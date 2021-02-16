#include "whisperlib/io/path.h"
#include "absl/strings/str_cat.h"

namespace whisper {
namespace path {

absl::string_view Basename(absl::string_view path) {
  auto pos = path.rfind(kDirSeparator);
  if (pos == absl::string_view::npos) { return path; }
  return path.substr(pos + 1);
}

absl::string_view Dirname(absl::string_view path) {
  auto pos = path.rfind(kDirSeparator);
  if (pos == absl::string_view::npos) { return ""; }
  return path.substr(0, pos);
}

std::string Normalize(absl::string_view path, char sep) {
  std::string s(path);
  const char sep_str[] = { sep, '\0' };
  // Normalize the slashes and add leading slash if necessary
  for ( size_t i = 0; i < s.size(); ++i ) {
    if ( s[i] == '\\' ) {
      s[i] = sep;
    }
  }
  bool slash_added = false;
  if ( s[0] != sep ) {
    s = sep_str + s;
    slash_added = true;
  }

  // Resolve occurrences of "///" in the normalized path
  const char triple_sep[] = { sep, sep, sep, '\0' };
  while ( true ) {
    const size_t index = s.find(triple_sep);
    if ( index == std::string::npos ) break;
    s = s.substr(0, index) + s.substr(index + 2);
  }
  // Resolve occurrences of "//" in the normalized path (but not beginning !)
  const char* double_sep = triple_sep + 1;
  while ( true ) {
    const size_t index = s.find(double_sep, 1);
    if ( index == std::string::npos ) break;
    s = s.substr(0, index) + s.substr(index + 1);
  }
  // Resolve occurrences of "/./" in the normalized path
  const char sep_dot_sep[] = { sep, '.', sep, '\0' };
  while ( true ) {
    const size_t index = s.find(sep_dot_sep);
    if ( index == std::string::npos ) break;
    s = s.substr(0, index) + s.substr(index + 2);
  }
  // Resolve occurrences of "/../" in the normalized path
  const char sep_dotdot_sep[] = { sep, '.', '.', sep, '\0' };
  while  ( true ) {
    const size_t index = s.find(sep_dotdot_sep);
    if ( index == std::string::npos ) break;
    if ( index == 0 )
      return slash_added ? "" : sep_str;
       // The only left path is the root.
    const size_t index2 = s.find_last_of(sep, index - 1);
    if ( index2 == std::string::npos )
      return slash_added ? "": sep_str;
    s = s.substr(0, index2) + s.substr(index + 3);
  }
  // Resolve ending "/.." and "/."
  {
    const char sep_dot[] = { sep, '.' , '\0' };
    const size_t index = s.rfind(sep_dot);
    if ( index != std::string::npos && index  == s.length() - 2 ) {
      s = s.substr(0, index);
    }
  }
  {
    const char sep_dotdot[] = { sep, '.', '.',  '\0' };
    size_t index = s.rfind(sep_dotdot);
    if ( index != std::string::npos && index == s.length() - 3 ) {
      if ( index == 0 )
        return slash_added ? "": sep_str;
      const size_t index2 = s.find_last_of(sep, index - 1);
      if ( index2 == std::string::npos )
        return slash_added ? "": sep_str;
      s = s.substr(0, index2);
    }
    if ( !slash_added && s.empty() ) s = sep_str;
  }
  if ( !slash_added || s.empty() ) return s;
  return s.substr(1);
}

std::string Join(absl::string_view path1,
                 absl::string_view path2,
                 char path_separator) {
  if (path1.empty()) {
    return std::string(path2);
  }
  if (path2.empty()) {
    return std::string(path1);
  }
  if ((path1.size() == 1 && *path1.begin() == path_separator)
      || *path2.begin() == path_separator
      || *path1.rbegin() == path_separator) {
    return absl::StrCat(path1, path2);
  }
  return absl::StrCat(path1, absl::string_view(&path_separator, 1), path2);
}

std::string Join(absl::Span<std::string> paths) {
  std::string result_path;
  for (absl::string_view path : paths) {
    result_path = Join(result_path, path);
  }
  return result_path;
}
std::string Join(std::initializer_list<absl::string_view> paths) {
  std::string result_path;
  for (absl::string_view path : paths) {
    result_path = Join(result_path, path);
  }
  return result_path;
}

}  // namespace path
}  // namespace whisper
