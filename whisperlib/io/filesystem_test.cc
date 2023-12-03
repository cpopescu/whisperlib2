#include "whisperlib/io/filesystem.h"

#include <algorithm>

#include "absl/strings/str_replace.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "whisperlib/io/file.h"
#include "whisperlib/io/path.h"
#include "whisperlib/status/status.h"
#include "whisperlib/status/testing.h"

namespace whisper {
namespace io {

class FilesystemTest : public testing::Test {
 protected:
  void SetUp() {
    test_dir_ = path::Join(testing::TempDir(), "whisper_io_filesystem_test");
    CHECK_OK(MkDir(test_dir_, true));
  }
  std::string test_dir_;
};
namespace {
std::string _p(absl::string_view path) {
  return absl::StrReplaceAll(path, {{"/", path::kDirSeparatorStr}});
}

TEST_F(FilesystemTest, FilesystemOps) {
  std::string tmp_dir(path::Join(test_dir_, "FileOps"));
  ASSERT_OK(MkDir(tmp_dir));
  ASSERT_OK(RmFilesUnder(tmp_dir, true));
  std::cerr << "Running test under: " << tmp_dir << std::endl;

  // We basically create a bunch of files and directories, and then
  // start shuffling them by moves, removes, etc. while testing their
  // existence and listing them.

  // Create the directories:
  ASSERT_OK(MkDir(path::Join(tmp_dir, _p("d1/dd1/ddd1")), true));
  ASSERT_OK(MkDir(path::Join(tmp_dir, _p("d1/dd1/ddd1/dddd1"))));
  ASSERT_OK(MkDir(path::Join(tmp_dir, _p("d1/dd1/ddd2"))));
  ASSERT_OK(MkDir(path::Join(tmp_dir, _p("d1/dd2"))));
  EXPECT_RAISES(MkDir(path::Join(tmp_dir, _p("d2/dd1"))), NotFound);
  ASSERT_OK(MkDir(path::Join(tmp_dir, _p("d2"))));
  ASSERT_OK(MkDir(path::Join(tmp_dir, _p("d2/dd1"))));
  ASSERT_OK(MkDir(path::Join(tmp_dir, _p("d3"))));

  absl::Time start_time = absl::Now() - absl::Seconds(1);
  // Creating the files in the directory:
  ASSERT_OK(File::WriteFromString(path::Join(tmp_dir, _p("d1/dd1/ddd1/f1")),
                                  "abcdefg")
                .status());
  ASSERT_OK(
      File::WriteFromString(path::Join(tmp_dir, _p("d1/dd1/ddd1/f2")), "123")
          .status());
  ASSERT_OK(
      File::WriteFromString(path::Join(tmp_dir, _p("d1/dd1/g1")), "").status());
  ASSERT_OK(
      File::WriteFromString(path::Join(tmp_dir, _p("d1/h1")), "").status());
  ASSERT_OK(
      File::WriteFromString(path::Join(tmp_dir, _p("d2/hh1")), "").status());
  ASSERT_OK(
      File::WriteFromString(path::Join(tmp_dir, _p("d2/hh2")), "").status());
  ASSERT_OK(File::WriteFromString(path::Join(tmp_dir, _p("d2/dd1/gg1")), "")
                .status());
  ASSERT_OK(Symlink(path::Join(tmp_dir, _p("d2/hh3")),
                    path::Join(tmp_dir, _p("d2/hh2"))));
  EXPECT_TRUE(IsSymlink(path::Join(tmp_dir, _p("d2/hh3"))));

  // Files created: we start the shuffle:
  EXPECT_TRUE(IsDir(path::Join(tmp_dir, _p("d1/dd1/ddd1"))));
  EXPECT_FALSE(IsDir(path::Join(tmp_dir, _p("d1/dd1/ddd1/f1"))));
  EXPECT_TRUE(IsReadableFile(path::Join(tmp_dir, _p("d1/dd1/ddd1/f1"))));
  EXPECT_FALSE(IsReadableFile(path::Join(tmp_dir, _p("d1/dd1/ddd1"))));
  EXPECT_TRUE(Exists(path::Join(tmp_dir, _p("d1/dd1/ddd1"))));
  EXPECT_TRUE(Exists(path::Join(tmp_dir, _p("d1/dd1/ddd1/f1"))));
  EXPECT_FALSE(Exists(path::Join(tmp_dir, _p("d1/dd1/ddd1/f3"))));
  EXPECT_GE(GetFileModTime(path::Join(tmp_dir, _p("d1/dd1/ddd1/f1"))).value(),
            start_time);
  EXPECT_LE(GetFileModTime(path::Join(tmp_dir, _p("d1/dd1/ddd1/f1"))).value(),
            absl::Now());
  EXPECT_EQ(GetFileSize(path::Join(tmp_dir, _p("d1/dd1/ddd1/f1"))).value(), 7);
  EXPECT_EQ(GetFileSize(path::Join(tmp_dir, _p("d1/dd1/ddd1/f2"))).value(), 3);
  EXPECT_GT(GetFileSize(path::Join(tmp_dir, _p("d1/dd1/ddd1"))).value(), 0);
  EXPECT_RAISES(GetFileSize(path::Join(tmp_dir, _p("d1/dd1/ddd1/f3"))).status(),
                FailedPrecondition);

  {
    ASSERT_OK_AND_ASSIGN(auto dirlist,
                         DirList(tmp_dir, LIST_FILES | LIST_DIRS));
    std::sort(dirlist.begin(), dirlist.end());
    EXPECT_THAT(dirlist, testing::ElementsAre("d1", "d2", "d3"));
  }
  {
    ASSERT_OK_AND_ASSIGN(auto dirlist, DirList(tmp_dir, LIST_FILES | LIST_DIRS |
                                                            LIST_RECURSIVE));
    std::sort(dirlist.begin(), dirlist.end());
    EXPECT_THAT(
        dirlist,
        testing::ElementsAre(
            _p("d1"), _p("d1/dd1"), _p("d1/dd1/ddd1"), _p("d1/dd1/ddd1/dddd1"),
            _p("d1/dd1/ddd1/f1"), _p("d1/dd1/ddd1/f2"), _p("d1/dd1/ddd2"),
            _p("d1/dd1/g1"), _p("d1/dd2"), _p("d1/h1"), _p("d2"), _p("d2/dd1"),
            _p("d2/dd1/gg1"), _p("d2/hh1"), _p("d2/hh2"), _p("d2/hh3"),
            _p("d3")));
  }
  {
    ASSERT_OK_AND_ASSIGN(auto dirlist,
                         DirList(tmp_dir, LIST_FILES | LIST_RECURSIVE));
    std::sort(dirlist.begin(), dirlist.end());
    EXPECT_THAT(dirlist, testing::ElementsAre(
                             _p("d1/dd1/ddd1/f1"), _p("d1/dd1/ddd1/f2"),
                             _p("d1/dd1/g1"), _p("d1/h1"), _p("d2/dd1/gg1"),
                             _p("d2/hh1"), _p("d2/hh2"), _p("d2/hh3")));
  }

  // On Windows the macro expansion does not expand #ifdef s
  EXPECT_RAISES(Mv(path::Join(tmp_dir, _p("d1/dd1/ddd1/f1")),
                   path::Join(tmp_dir, _p("d1/dd1/ddd1/f2"))),
                Internal);
  EXPECT_RAISES(RmFile(path::Join(tmp_dir, _p("d1/dd1"))), Internal);
  EXPECT_RAISES(RmDir(path::Join(tmp_dir, _p("d1/dd1"))), Internal);
  EXPECT_OK(RmFile(path::Join(tmp_dir, _p("d1/dd1/ddd1/f1"))));
  EXPECT_FALSE(Exists(path::Join(tmp_dir, _p("d1/dd1/ddd1/f1"))));
  EXPECT_OK(Rename(path::Join(tmp_dir, _p("d1/dd1/ddd1/f2")),
                   path::Join(tmp_dir, _p("d1/dd1/ddd2/f2"))));
  EXPECT_RAISES(RmDir(path::Join(tmp_dir, _p("d1/dd1/ddd1"))), Internal);
  EXPECT_OK(RmDir(path::Join(tmp_dir, _p("d1/dd1/ddd1/dddd1"))));
  EXPECT_OK(RmDir(path::Join(tmp_dir, _p("d1/dd1/ddd1"))));
  EXPECT_FALSE(Exists(path::Join(tmp_dir, _p("d1/dd1/ddd1"))));
  EXPECT_TRUE(Exists(path::Join(tmp_dir, _p("d1/dd1/ddd2/f2"))));
  EXPECT_RAISES(Rename(path::Join(tmp_dir, _p("d1/dd1/ddd2/f2")),
                       path::Join(tmp_dir, _p("d1/dd1/ddd1/f2"))),
                NotFound);
  EXPECT_OK(Mv(path::Join(tmp_dir, _p("d1/dd1/ddd2/f2")),
               path::Join(tmp_dir, _p("d1/dd1"))));
  EXPECT_TRUE(Exists(path::Join(tmp_dir, _p("d1/dd1/f2"))));
  EXPECT_RAISES(Mv(path::Join(tmp_dir, _p("d1/dd1/ddd2/f2")),
                   path::Join(tmp_dir, _p("d1/dd1/f2"))),
                NotFound);
  EXPECT_OK(Rename(path::Join(tmp_dir, _p("d2/hh1")),
                   path::Join(tmp_dir, _p("d2/hh2")), true));
  EXPECT_FALSE(Exists(path::Join(tmp_dir, _p("d2/hh1"))));
  EXPECT_TRUE(Exists(path::Join(tmp_dir, _p("d2/hh2"))));

  EXPECT_OK(RmFilesUnder(path::Join(tmp_dir, "d1"), true));
  {
    ASSERT_OK_AND_ASSIGN(auto dirlist, DirList(tmp_dir, LIST_FILES | LIST_DIRS |
                                                            LIST_RECURSIVE));
    std::sort(dirlist.begin(), dirlist.end());
    EXPECT_THAT(dirlist,
                testing::ElementsAre("d1", "d2", _p("d2/dd1"), _p("d2/dd1/gg1"),
                                     _p("d2/hh2"), _p("d2/hh3"), "d3"));
  }
}
}  // namespace
}  // namespace io
}  // namespace whisper
