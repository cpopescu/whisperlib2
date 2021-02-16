#include "whisperlib/io/path.h"

#include <vector>
#include "absl/strings/str_replace.h"
#include "gtest/gtest.h"

namespace whisper {
namespace path {

namespace {
std::string _p(absl::string_view path) {
  return absl::StrReplaceAll(path, { {"/", whisper::path::kDirSeparatorStr} });
}
}  // namespace

TEST(WhisperPath, Normalize) {
  EXPECT_EQ(Normalize(""), "");
  EXPECT_EQ(Normalize(_p("/")), _p("/"));
  EXPECT_EQ(Normalize(_p("//")), _p("//"));
  EXPECT_EQ(Normalize(_p("///")), _p("/"));
  EXPECT_EQ(Normalize("f"), "f");
  EXPECT_EQ(Normalize("foo"), "foo");
  EXPECT_EQ(Normalize(_p("foo/")), _p("foo/"));
  EXPECT_EQ(Normalize(_p("f/")), _p("f/"));
  EXPECT_EQ(Normalize(_p("/foo")), _p("/foo"));
  EXPECT_EQ(Normalize(_p("foo/bar")), _p("foo/bar"));
  EXPECT_EQ(Normalize(_p("..")), _p(""));
  EXPECT_EQ(Normalize(_p("../..")), _p(""));
  EXPECT_EQ(Normalize(_p("/..")), _p("/"));
  EXPECT_EQ(Normalize(_p("/../..")), _p("/"));
  EXPECT_EQ(Normalize(_p("../foo")), _p(""));
  EXPECT_EQ(Normalize(_p("foo/..")), _p(""));
  EXPECT_EQ(Normalize(_p("foo/../")), _p(""));
  EXPECT_EQ(Normalize(_p("foo/...")), _p("foo/..."));
  EXPECT_EQ(Normalize(_p("foo/.../")), _p("foo/.../"));
  EXPECT_EQ(Normalize(_p("foo/..bar")), _p("foo/..bar"));
  EXPECT_EQ(Normalize(_p("../f")), _p(""));
  EXPECT_EQ(Normalize(_p("/../f")), _p("/"));
  EXPECT_EQ(Normalize(_p("f/..")), _p(""));
  EXPECT_EQ(Normalize(_p("foo/../..")), _p(""));
  EXPECT_EQ(Normalize(_p("foo/../../")), _p(""));
  EXPECT_EQ(Normalize(_p("foo/../../..")), _p(""));
  EXPECT_EQ(Normalize(_p("foo/../../../")), _p(""));
  EXPECT_EQ(Normalize(_p("foo/../bar")), _p("bar"));
  EXPECT_EQ(Normalize(_p("foo/../bar/")), _p("bar/"));
  EXPECT_EQ(Normalize(_p("foo/bar/..")), _p("foo"));
  EXPECT_EQ(Normalize(_p("foo/bar/../")), _p("foo/"));
  EXPECT_EQ(Normalize(_p("foo/bar/../..")), _p(""));
  EXPECT_EQ(Normalize(_p("foo/bar/../../")), _p(""));
  EXPECT_EQ(Normalize(_p("foo/bar/../blah")), _p("foo/blah"));
  EXPECT_EQ(Normalize(_p("f/../b")), _p("b"));
  EXPECT_EQ(Normalize(_p("f/b/..")), _p("f"));
  EXPECT_EQ(Normalize(_p("f/b/../")), _p("f/"));
  EXPECT_EQ(Normalize(_p("f/b/../a")), _p("f/a"));
  EXPECT_EQ(Normalize(_p("foo/bar/blah/../..")), _p("foo"));
  EXPECT_EQ(Normalize(_p("foo/bar/blah/../../bletch")),
            _p("foo/bletch"));
  EXPECT_EQ(Normalize(_p("//net")), _p("//net"));
  EXPECT_EQ(Normalize(_p("//net/")), _p("//net/"));
  EXPECT_EQ(Normalize(_p("//..net")), _p("//..net"));
  EXPECT_EQ(Normalize(_p("//net/..")), _p("/"));
  EXPECT_EQ(Normalize(_p("//net/foo")), _p("//net/foo"));
  EXPECT_EQ(Normalize(_p("//net/foo/")), _p("//net/foo/"));
  EXPECT_EQ(Normalize(_p("//net/foo/..")), _p("//net"));
  EXPECT_EQ(Normalize(_p("//net/foo/../")), _p("//net/"));

  EXPECT_EQ(Normalize(_p("/net/foo/bar")),
            _p("/net/foo/bar"));
  EXPECT_EQ(Normalize(_p("/net/foo/bar/")),
            _p("/net/foo/bar/"));
  EXPECT_EQ(Normalize(_p("/net/foo/..")),
            _p("/net"));
  EXPECT_EQ(Normalize(_p("/net/foo/../")),
            _p("/net/"));

  EXPECT_EQ(Normalize(_p("//net//foo//bar")),
            _p("//net/foo/bar"));
  EXPECT_EQ(Normalize(_p("//net//foo//bar//")),
            _p("//net/foo/bar/"));
  EXPECT_EQ(Normalize(_p("//net//foo//..")),
            _p("//net"));
  EXPECT_EQ(Normalize(_p("//net//foo//..//")),
            _p("//net/"));

  EXPECT_EQ(Normalize(_p("///net///foo///bar")),
            _p("/net/foo/bar"));
  EXPECT_EQ(Normalize(_p("///net///foo///bar///")),
            _p("/net/foo/bar/"));
  EXPECT_EQ(Normalize(_p("///net///foo///..")),
            _p("/net"));
  EXPECT_EQ(Normalize(_p("///net///foo///..///")),
            _p("/net/"));

  ///////////////////////////////////////////////////////////////////////

  EXPECT_EQ(Normalize("", '#'), "");
  EXPECT_EQ(Normalize("#", '#'), "#");
  EXPECT_EQ(Normalize("##", '#'), "##");
  EXPECT_EQ(Normalize("###", '#'), "#");
  EXPECT_EQ(Normalize("f", '#'), "f");
  EXPECT_EQ(Normalize("foo", '#'), "foo");
  EXPECT_EQ(Normalize("foo#", '#'), "foo#");
  EXPECT_EQ(Normalize("f#", '#'), "f#");
  EXPECT_EQ(Normalize("#foo", '#'), "#foo");
  EXPECT_EQ(Normalize("foo#bar", '#'), "foo#bar");
  EXPECT_EQ(Normalize("..", '#'), "");
  EXPECT_EQ(Normalize("..#..", '#'), "");
  EXPECT_EQ(Normalize("#..", '#'), "#");
  EXPECT_EQ(Normalize("#..#..", '#'), "#");
  EXPECT_EQ(Normalize("..#foo", '#'), "");
  EXPECT_EQ(Normalize("foo#..", '#'), "");
  EXPECT_EQ(Normalize("foo#..#", '#'), "");
  EXPECT_EQ(Normalize("foo#...", '#'), "foo#...");
  EXPECT_EQ(Normalize("foo#...#", '#'), "foo#...#");
  EXPECT_EQ(Normalize("foo#..bar", '#'), "foo#..bar");
  EXPECT_EQ(Normalize("..#f", '#'), "");
  EXPECT_EQ(Normalize("#..#f", '#'), "#");
  EXPECT_EQ(Normalize("f#..", '#'), "");
  EXPECT_EQ(Normalize("foo#..#..", '#'), "");
  EXPECT_EQ(Normalize("foo#..#..#", '#'), "");
  EXPECT_EQ(Normalize("foo#..#..#..", '#'), "");
  EXPECT_EQ(Normalize("foo#..#..#..#", '#'), "");
  EXPECT_EQ(Normalize("foo#..#bar", '#'), "bar");
  EXPECT_EQ(Normalize("foo#..#bar#", '#'), "bar#");
  EXPECT_EQ(Normalize("foo#bar#..", '#'), "foo");
  EXPECT_EQ(Normalize("foo#bar#..#", '#'), "foo#");
  EXPECT_EQ(Normalize("foo#bar#..#..", '#'), "");
  EXPECT_EQ(Normalize("foo#bar#..#..#", '#'), "");
  EXPECT_EQ(Normalize("foo#bar#..#blah", '#'), "foo#blah");
  EXPECT_EQ(Normalize("f#..#b", '#'), "b");
  EXPECT_EQ(Normalize("f#b#..", '#'), "f");
  EXPECT_EQ(Normalize("f#b#..#", '#'), "f#");
  EXPECT_EQ(Normalize("f#b#..#a", '#'), "f#a");
  EXPECT_EQ(Normalize("foo#bar#blah#..#..", '#'), "foo");
  EXPECT_EQ(Normalize("foo#bar#blah#..#..#bletch", '#'),
            "foo#bletch");
  EXPECT_EQ(Normalize("##net", '#'), "##net");
  EXPECT_EQ(Normalize("##net#", '#'), "##net#");
  EXPECT_EQ(Normalize("##..net", '#'), "##..net");
  EXPECT_EQ(Normalize("##net#..", '#'), "#");
  EXPECT_EQ(Normalize("##net#foo", '#'), "##net#foo");
  EXPECT_EQ(Normalize("##net#foo#", '#'), "##net#foo#");
  EXPECT_EQ(Normalize("##net#foo#..", '#'), "##net");
  EXPECT_EQ(Normalize("##net#foo#..#", '#'), "##net#");

  EXPECT_EQ(Normalize("#net#foo#bar", '#'),
            "#net#foo#bar");
  EXPECT_EQ(Normalize("#net#foo#bar#", '#'),
            "#net#foo#bar#");
  EXPECT_EQ(Normalize("#net#foo#..", '#'),
            "#net");
  EXPECT_EQ(Normalize("#net#foo#..#", '#'),
            "#net#");

  EXPECT_EQ(Normalize("##net##foo##bar", '#'),
            "##net#foo#bar");
  EXPECT_EQ(Normalize("##net##foo##bar##", '#'),
            "##net#foo#bar#");
  EXPECT_EQ(Normalize("##net##foo##..", '#'),
            "##net");
  EXPECT_EQ(Normalize("##net##foo##..##", '#'),
            "##net#");

  EXPECT_EQ(Normalize("###net###foo###bar", '#'),
            "#net#foo#bar");
  EXPECT_EQ(Normalize("###net###foo###bar###", '#'),
            "#net#foo#bar#");
  EXPECT_EQ(Normalize("###net###foo###..", '#'),
            "#net");
  EXPECT_EQ(Normalize("###net###foo###..###", '#'),
            "#net#");
}

TEST(WhisperPath, Basename) {
  EXPECT_EQ(Basename(""), "");
  EXPECT_EQ(Basename("foo"), "foo");
  EXPECT_EQ(Basename(_p("foo/bar")), "bar");
  EXPECT_EQ(Basename(_p("foo//bar")), "bar");
  EXPECT_EQ(Basename(_p("/baz/foo//bar")), "bar");
  EXPECT_EQ(Basename(_p("foo/bar/")), "");
}

TEST(WhisperPath, Dirname) {
  EXPECT_EQ(Dirname(""), "");
  EXPECT_EQ(Dirname("foo"), "");
  EXPECT_EQ(Dirname(_p("foo/bar")), "foo");
  EXPECT_EQ(Dirname(_p("foo//bar")), _p("foo/"));
  EXPECT_EQ(Dirname(_p("/baz/foo/bar")), _p("/baz/foo"));
  EXPECT_EQ(Dirname(_p("/baz/foo//bar")), _p("/baz/foo/"));
  EXPECT_EQ(Dirname(_p("foo/bar/")), _p("foo/bar"));
}

TEST(WhisperPath, Join) {
  EXPECT_EQ(Join(_p(""), _p("")), _p(""));
  EXPECT_EQ(Join(_p(""), _p("b")), _p("b"));
  EXPECT_EQ(Join(_p(""), _p("/b")), _p("/b"));
  EXPECT_EQ(Join(_p("/"), _p("")), _p("/"));
  EXPECT_EQ(Join(_p("/"), _p("b")), _p("/b"));
  EXPECT_EQ(Join(_p("/"), _p("/b")), _p("//b"));
  EXPECT_EQ(Join(_p("/a"), _p("b")), _p("/a/b"));
  EXPECT_EQ(Join(_p("/a"), _p("/b")), _p("/a/b"));
  EXPECT_EQ(Normalize(
              Join(_p("/a/b"), _p("//c//d//"))), _p("/a/b/c/d/"));
  EXPECT_EQ(Join(_p("a"), _p("b")), _p("a/b"));
  EXPECT_EQ(Join(_p("a"), _p("/b")), _p("a/b"));
  EXPECT_EQ(Join(_p("a/"), _p("b/")), _p("a/b/"));
  EXPECT_EQ(Join(_p("a/"), _p("b/"), _p("c/")), _p("a/b/c/"));
  EXPECT_EQ(Join({_p("a/"), _p("b/"), _p("c/")}), _p("a/b/c/"));
  std::vector<std::string> paths({_p("a/"), _p("b/"), _p("c/")});
  EXPECT_EQ(Join(absl::Span<std::string>(paths)), _p("a/b/c/"));
}

}  // namespace path
}  // namespace whisper
