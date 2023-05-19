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

#define CASE_STR(name)                          \
case name: return #name

absl::string_view ErrnoName(int error) {
  switch (error) {
    CASE_STR(E2BIG);
    CASE_STR(EACCES);
    CASE_STR(EADDRINUSE);
    CASE_STR(EADDRNOTAVAIL);
    CASE_STR(EAFNOSUPPORT);
    CASE_STR(EAGAIN);
    CASE_STR(EALREADY);
#ifdef EBADE
    CASE_STR(EBADE);
#endif  // EBADE
    CASE_STR(EBADF);
#ifdef EBADFD
    CASE_STR(EBADFD);
#endif  // EBADFD
    CASE_STR(EBADMSG);
#ifdef EBADR
    CASE_STR(EBADR);
#endif  // EBADR
#ifdef EBADRQC
    CASE_STR(EBADRQC);
#endif  // EBADRQC
#ifdef EBADSLT
    CASE_STR(EBADSLT);
#endif  // EBADSLT
    CASE_STR(EBUSY);
    CASE_STR(ECANCELED);
    CASE_STR(ECHILD);
#ifdef ECHRNG
    CASE_STR(ECHRNG);
#endif  // ECHRNG
#ifdef ECOMM
    CASE_STR(ECOMM);
#endif  // ECOMM
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
#ifdef EHWPOISON
    CASE_STR(EHWPOISON);
#endif  // EHWPOISON
    CASE_STR(EIDRM);
    CASE_STR(EILSEQ);
    CASE_STR(EINPROGRESS);
    CASE_STR(EINTR);
    CASE_STR(EINVAL);
    CASE_STR(EIO);
    CASE_STR(EISCONN);
    CASE_STR(EISDIR);
#ifdef EISNAM
    CASE_STR(EISNAM);
#endif  // EISNAM
#ifdef EKEYEXPIRED
    CASE_STR(EKEYEXPIRED);
#endif  // EKEYEXPIRED
#ifdef EKEYREJECTED
    CASE_STR(EKEYREJECTED);
#endif  // EKEYREJECTED
#ifdef EKEYREVOKED
    CASE_STR(EKEYREVOKED);
#endif  // EKEYREVOKED
#ifdef EL2HLT
    CASE_STR(EL2HLT);
#endif  // EL2HLT
#ifdef EL2NSYNC
    CASE_STR(EL2NSYNC);
#endif  // EL2NSYNC
#ifdef EL3HLT
    CASE_STR(EL3HLT);
#endif  // EL3HLT
#ifdef EL3RST
    CASE_STR(EL3RST);
#endif  // EL3RST
#ifdef ELIBACC
    CASE_STR(ELIBACC);
#endif  // ELIBACC
#ifdef ELIBBAD
    CASE_STR(ELIBBAD);
#endif  // ELIBBAD
#ifdef ELIBEXEC
    CASE_STR(ELIBEXEC);
#endif  // ELIBEXEC
#ifdef ELIBMAX
    CASE_STR(ELIBMAX);
#endif  // ELIBMAX
#ifdef ELIBSCN
    CASE_STR(ELIBSCN);
#endif  // ELIBSCN
    CASE_STR(ELOOP);
#ifdef EMEDIUMTYPE
    CASE_STR(EMEDIUMTYPE);
#endif  // EMEDIUMTYPE
    CASE_STR(EMFILE);
    CASE_STR(EMLINK);
    CASE_STR(EMSGSIZE);
    CASE_STR(EMULTIHOP);
    CASE_STR(ENAMETOOLONG);
    CASE_STR(ENETDOWN);
    CASE_STR(ENETRESET);
    CASE_STR(ENETUNREACH);
    CASE_STR(ENFILE);
#ifdef ENOANO
    CASE_STR(ENOANO);
#endif  // ENOANO
    CASE_STR(ENOBUFS);
    CASE_STR(ENODATA);
    CASE_STR(ENODEV);
    CASE_STR(ENOENT);
    CASE_STR(ENOEXEC);
#ifdef ENOKEY
    CASE_STR(ENOKEY);
#endif  // ENOKEY
    CASE_STR(ENOLCK);
    CASE_STR(ENOLINK);
#ifdef ENOMEDIUM
    CASE_STR(ENOMEDIUM);
#endif  // ENOMEDIUM
    CASE_STR(ENOMEM);
    CASE_STR(ENOMSG);
#ifdef ENONET
    CASE_STR(ENONET);
#endif  // ENONET
#ifdef ENOPKG
    CASE_STR(ENOPKG);
#endif  // ENOPKG
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
#ifdef ENOTUNIQ
    CASE_STR(ENOTUNIQ);
#endif  // ENOTUNIQ
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
#ifdef EREMCHG
    CASE_STR(EREMCHG);
#endif  // EREMCHG
    CASE_STR(EREMOTE);
#ifdef EREMOTEIO
    CASE_STR(EREMOTEIO);
#endif  // EREMOTEIO
#ifdef ERESTART
    CASE_STR(ERESTART);
#endif  // ERESTART
#ifdef ERFKILL
    CASE_STR(ERFKILL);
#endif  // ERFKILL
    CASE_STR(EROFS);
    CASE_STR(ESHUTDOWN);
    CASE_STR(ESOCKTNOSUPPORT);
    CASE_STR(ESPIPE);
    CASE_STR(ESRCH);
    CASE_STR(ESTALE);
#ifdef ESTRPIPE
    CASE_STR(ESTRPIPE);
#endif  // ESTRPIPE
    CASE_STR(ETIME);
    CASE_STR(ETIMEDOUT);
    CASE_STR(ETOOMANYREFS);
    CASE_STR(ETXTBSY);
#ifdef EUCLEAN
    CASE_STR(EUCLEAN);
#endif  // EUCLEAN
#ifdef EUNATCH
    CASE_STR(EUNATCH);
#endif  // EUNATCH
    CASE_STR(EUSERS);
    CASE_STR(EXDEV);
#ifdef EXFULL
    CASE_STR(EXFULL);
#endif  // EXFULL
  }
  // Possible Duplicates for above
  switch (error) {
#ifdef EDEADLOCK
    CASE_STR(EDEADLOCK);
#endif  // EDEADLOCK
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
#ifdef ECHRNG
  case ECHRNG:           // Channel number out of range.
#endif  // ECHRNG
  case ERANGE:           // Result too large.
#ifdef ELNRANGE
  case ELNRANGE:         // Link number out of range.
#endif
    return status::OutOfRangeErrorBuilder() << error_str;
#ifdef EBADE
  case EBADE:            // Invalid exchange.
#endif  // EBADE
  case EBADF:            // Bad file descriptor.
#ifdef EBADFD
  case EBADFD:           // File descriptor in bad state.
#endif
  case EBADMSG:          // Bad message.
#ifdef EBADR
  case EBADR:            // Invalid request descriptor.
#endif
#ifdef EBADRQC
  case EBADRQC:          // Invalid request code.
#endif
#ifdef EBADSLT
  case EBADSLT:          // Invalid slot.
#endif
  case EDESTADDRREQ:     // Destination address required.
  case EDOM:             // Mathematics argument out of domain of function.
  case EMSGSIZE:         // Message too long
  case ENAMETOOLONG:     // Filename too long.
  case EISDIR:           // Is a directory.
  case EINVAL:           // Invalid argument.
#ifdef EISNAM
  case EISNAM:           // Is a named type file.
#endif
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
#ifdef EXFULL
  case EXFULL:           // Exchange full.
#endif
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
