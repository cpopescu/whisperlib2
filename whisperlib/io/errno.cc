#include "whisperlib/io/errno.h"

#include "absl/strings/string_view.h"
#include "absl/strings/str_cat.h"

namespace whisper {
namespace error {

namespace {
// For different is triggered by different versions of strerror_r.
template<class T>
const char* ErrorBuffer(const char* buffer, T result);

template<>
inline const char* ErrorBuffer<int>(const char* buffer, int result) {
  if (!result) { return buffer; }
  return "{No errno description found.}";
}

template<>
inline const char* ErrorBuffer<char*>(const char* buffer, char* result) {
  if (result == nullptr) { return buffer; }
  return "{No errno description found.}";
}

#define CASE_STR(name) case name: return #name
absl::string_view ErrnoName(int error) {
  switch (error) {
    CASE_STR(E2BIG);
    CASE_STR(EACCES);
    CASE_STR(EADDRINUSE);
    CASE_STR(EADDRNOTAVAIL);
    CASE_STR(EAFNOSUPPORT);
    CASE_STR(EAGAIN);
    CASE_STR(EALREADY);
    CASE_STR(EBADE);
    CASE_STR(EBADF);
    CASE_STR(EBADFD);
    CASE_STR(EBADMSG);
    CASE_STR(EBADR);
    CASE_STR(EBADRQC);
    CASE_STR(EBADSLT);
    CASE_STR(EBUSY);
    CASE_STR(ECANCELED);
    CASE_STR(ECHILD);
    CASE_STR(ECHRNG);
    CASE_STR(ECOMM);
    CASE_STR(ECONNABORTED);
    CASE_STR(ECONNREFUSED);
    CASE_STR(ECONNRESET);
    CASE_STR(EDEADLK);
    CASE_STR(EDESTADDRREQ);
    CASE_STR(EDOM);
    CASE_STR(EDQUOT);
    CASE_STR(EEXIST);
    CASE_STR(EFAULT);
    CASE_STR(EFBIG);
    CASE_STR(EHOSTDOWN);
    CASE_STR(EHOSTUNREACH);
    CASE_STR(EHWPOISON);
    CASE_STR(EIDRM);
    CASE_STR(EILSEQ);
    CASE_STR(EINPROGRESS);
    CASE_STR(EINTR);
    CASE_STR(EINVAL);
    CASE_STR(EIO);
    CASE_STR(EISCONN);
    CASE_STR(EISDIR);
    CASE_STR(EISNAM);
    CASE_STR(EKEYEXPIRED);
    CASE_STR(EKEYREJECTED);
    CASE_STR(EKEYREVOKED);
    CASE_STR(EL2HLT);
    CASE_STR(EL2NSYNC);
    CASE_STR(EL3HLT);
    CASE_STR(EL3RST);
    CASE_STR(ELIBACC);
    CASE_STR(ELIBBAD);
    CASE_STR(ELIBEXEC);
    CASE_STR(ELIBMAX);
    CASE_STR(ELIBSCN);
    CASE_STR(ELOOP);
    CASE_STR(EMEDIUMTYPE);
    CASE_STR(EMFILE);
    CASE_STR(EMLINK);
    CASE_STR(EMSGSIZE);
    CASE_STR(EMULTIHOP);
    CASE_STR(ENAMETOOLONG);
    CASE_STR(ENETDOWN);
    CASE_STR(ENETRESET);
    CASE_STR(ENETUNREACH);
    CASE_STR(ENFILE);
    CASE_STR(ENOANO);
    CASE_STR(ENOBUFS);
    CASE_STR(ENODATA);
    CASE_STR(ENODEV);
    CASE_STR(ENOENT);
    CASE_STR(ENOEXEC);
    CASE_STR(ENOKEY);
    CASE_STR(ENOLCK);
    CASE_STR(ENOLINK);
    CASE_STR(ENOMEDIUM);
    CASE_STR(ENOMEM);
    CASE_STR(ENOMSG);
    CASE_STR(ENONET);
    CASE_STR(ENOPKG);
    CASE_STR(ENOPROTOOPT);
    CASE_STR(ENOSPC);
    CASE_STR(ENOSR);
    CASE_STR(ENOSTR);
    CASE_STR(ENOSYS);
    CASE_STR(ENOTBLK);
    CASE_STR(ENOTCONN);
    CASE_STR(ENOTDIR);
    CASE_STR(ENOTEMPTY);
    CASE_STR(ENOTRECOVERABLE);
    CASE_STR(ENOTSOCK);
    CASE_STR(ENOTSUP);
    CASE_STR(ENOTTY);
    CASE_STR(ENOTUNIQ);
    CASE_STR(ENXIO);
    CASE_STR(EOVERFLOW);
    CASE_STR(EOWNERDEAD);
    CASE_STR(EPERM);
    CASE_STR(EPFNOSUPPORT);
    CASE_STR(EPIPE);
    CASE_STR(EPROTO);
    CASE_STR(EPROTONOSUPPORT);
    CASE_STR(EPROTOTYPE);
    CASE_STR(ERANGE);
    CASE_STR(EREMCHG);
    CASE_STR(EREMOTE);
    CASE_STR(EREMOTEIO);
    CASE_STR(ERESTART);
    CASE_STR(ERFKILL);
    CASE_STR(EROFS);
    CASE_STR(ESHUTDOWN);
    CASE_STR(ESOCKTNOSUPPORT);
    CASE_STR(ESPIPE);
    CASE_STR(ESRCH);
    CASE_STR(ESTALE);
    CASE_STR(ESTRPIPE);
    CASE_STR(ETIME);
    CASE_STR(ETIMEDOUT);
    CASE_STR(ETOOMANYREFS);
    CASE_STR(ETXTBSY);
    CASE_STR(EUCLEAN);
    CASE_STR(EUNATCH);
    CASE_STR(EUSERS);
    CASE_STR(EXDEV);
    CASE_STR(EXFULL);
  }
  // Possible Duplicates for above
  switch (error) {
    CASE_STR(EDEADLOCK);
    CASE_STR(EOPNOTSUPP);
    CASE_STR(EWOULDBLOCK);
#ifdef ELNRANGE
    CASE_STR(ELNRANGE);
#endif
  }
  return "";
}
}  // namespace

std::string ErrnoToString(int error) {
  char errmsg[512] = { '\0', };
  return absl::StrCat(
      "Errno: ", error, " [", ErrnoName(error), "] ",
      ErrorBuffer(errmsg, strerror_r(error, errmsg, sizeof(errmsg))), " ");
}

bool IsUnavailableAndShouldRetry(int error) {
  return error == EWOULDBLOCK || error == EAGAIN;
}

status::StatusWriter ErrnoToStatus(int error) {
  const std::string error_str(ErrnoToString(error));
  // Cover same values for this error in the switch.
  if (error == EWOULDBLOCK) { error = EAGAIN; }

  switch (errno) {
  case EAGAIN:           // Resource temporarily unavailable.
  case EADDRNOTAVAIL:    // Address not available.
    return status::UnavailableErrorBuilder() << error_str;
  case ECANCELED:        // Operation canceled
    return status::CancelledErrorBuilder() << error_str;
  case EACCES:           // Permission denied.
  case EPERM:            // Operation not permitted.
    return status::PermissionDeniedErrorBuilder() << error_str;
  case ECHRNG:           // Channel number out of range.
  case ERANGE:           // Result too large.
#ifdef ELNRANGE
  case ELNRANGE:         // Link number out of range.
#endif
    return status::OutOfRangeErrorBuilder() << error_str;
  case EBADE:            // Invalid exchange.
  case EBADF:            // Bad file descriptor.
  case EBADFD:           // File descriptor in bad state.
  case EBADMSG:          // Bad message.
  case EBADR:            // Invalid request descriptor.
  case EBADRQC:          // Invalid request code.
  case EBADSLT:          // Invalid slot.
  case EDESTADDRREQ:     // Destination address required.
  case EDOM:             // Mathematics argument out of domain of function.
  case EMSGSIZE:         // Message too long
  case ENAMETOOLONG:     // Filename too long.
  case EISDIR:           // Is a directory.
  case EINVAL:           // Invalid argument.
  case EISNAM:           // Is a named type file.
  case E2BIG:            // Argument list too long.
  case EFBIG:            // File too large.
  case ENOTSOCK:         // Not a socket.
  case ENXIO:            // No such device or address.
    return status::InvalidArgumentErrorBuilder() << error_str;
  case ECONNABORTED:     // Connection aborted.
    return status::AbortedErrorBuilder() << error_str;
  case EADDRINUSE:       // Address already in use.
  case EEXIST:           // File exists.
    return status::AlreadyExistsErrorBuilder() << error_str;
  case ENOENT:           // No such file or directory.
  case ESRCH:            // No such process.
    return status::NotFoundErrorBuilder() << error_str;
  case ENFILE:           // Too many open files in system.
  case EDQUOT:           // Disk quota exceeded.
  case EMLINK:           // Too many links.
  case EMFILE:           // Too many open files.
  case ENOSPC:           // No space left on device.
  case EUSERS:           // Too many users.
  case EXFULL:           // Exchange full.
  case ENOLCK:           // No locks available.
  case ENOMEM:           // Not enough space/cannot allocate memory.
    return status::ResourceExhaustedErrorBuilder() << error_str;
  case ESOCKTNOSUPPORT:  // Socket type not supported.
  case EAFNOSUPPORT:     // Address family not supported.
  case ENOPROTOOPT:      // Protocol not available.
  case ENOSYS:           // Function not implemented.
  case ENOTSUP:          // Operation not supported.
  case EPFNOSUPPORT:     // Protocol family not supported.
  case EPROTONOSUPPORT:  // Protocol not supported.
    return status::UnimplementedErrorBuilder() << error_str;
  default:
    return status::InternalErrorBuilder() << error_str;
  }
}
int Errno() {
  return errno;
}

}  // namespace error
}  // namespace whisper
