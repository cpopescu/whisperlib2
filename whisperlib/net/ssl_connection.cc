#include "whisperlib/net/ssl_connection.h"

#include <cstdio>

#include "absl/functional/bind_front.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "whisperlib/io/errno.h"
#include "whisperlib/status/status.h"

namespace whisper {
namespace net {

absl::string_view SslUtils::SslErrorName(int err) {
  switch(err) {
  case SSL_ERROR_NONE: return "SSL_ERROR_NONE";
  case SSL_ERROR_SSL: return "SSL_ERROR_SSL";
  case SSL_ERROR_WANT_READ: return "SSL_ERROR_WANT_READ";
  case SSL_ERROR_WANT_WRITE: return "SSL_ERROR_WANT_WRITE";
  case SSL_ERROR_WANT_X509_LOOKUP: return "SSL_ERROR_WANT_X509_LOOKUP";
  case SSL_ERROR_SYSCALL: return "SSL_ERROR_SYSCALL";
  case SSL_ERROR_ZERO_RETURN: return "SSL_ERROR_ZERO_RETURN";
  case SSL_ERROR_WANT_CONNECT: return "SSL_ERROR_WANT_CONNECT";
  case SSL_ERROR_WANT_ACCEPT: return "SSL_ERROR_WANT_ACCEPT";
  default: return "UNKNOWN";
  }
}
absl::string_view SslUtils::SslWantName(int want) {
  switch(want) {
  case SSL_NOTHING: return "SSL_NOTHING";
  case SSL_WRITING: return "SSL_WRITING";
  case SSL_READING: return "SSL_READING";
  case SSL_X509_LOOKUP: return "SSL_X509_LOOKUP";
  default: return "UNKNWOWN";
  }
}
std::string SslUtils::SslLastError() {
  std::vector<std::string> errors;
  errors.emplace_back("SSL error stack:");
  while (true) {
    int line = 0;
    const char* file = nullptr;
    const int e = ERR_get_error_line(&file, &line);
    if (e == 0) { break; }
    char text[512] = {0,};
    ERR_error_string_n(e, text, sizeof(text));
    errors.emplace_back(absl::StrCat("  ", text, ":", file, ":", line));
  }
  errors.emplace_back(absl::StrCat(
      "General error: ", error::ErrnoToString(error::Errno())));
  return absl::StrJoin(errors, "\n");
}

void SslUtils::SslLibraryInit() {
  SSL_library_init();                      // initialize library
  SSL_load_error_strings();                // readable error messages
  ERR_load_SSL_strings();
  ERR_load_CRYPTO_strings();
  ERR_load_crypto_strings();
  // actions_to_seed_PRNG();
}
absl::StatusOr<X509*>
SslUtils::SslLoadCertificateFile(absl::string_view filename) {
  std::string filename_str(filename);
  FILE* f = ::fopen(filename_str.c_str(), "r");
  if (f == nullptr) {
    return error::ErrnoToStatus(errno)
      << "Opening certificate file: `" << filename << "`";
  }
  base::CallOnReturn close_f([f]() { fclose(f); });
  X509* certificate = nullptr;
  if (nullptr == PEM_read_X509(f, &certificate, nullptr, nullptr)
      || nullptr == certificate) {
    return status::InternalErrorBuilder()
      << "PEM_read_X509 failed to load certificate from file: `"
      << filename << "`";
  }
  return certificate;
}
absl::StatusOr<EVP_PKEY*>
SslUtils::SslLoadPrivateKeyFile(absl::string_view filename) {
  std::string filename_str(filename);
  FILE* f = ::fopen(filename_str.c_str(), "r");
  if (f == nullptr) {
    return error::ErrnoToStatus(errno)
      << "Opening private key file: `" << filename << "`";
  }
  base::CallOnReturn close_f([f]() { fclose(f); });
  EVP_PKEY* key = nullptr;
  if (nullptr == PEM_read_PrivateKey(f, &key, nullptr, nullptr)
      || nullptr == key) {
    return status::InternalErrorBuilder()
      << "PEM_read_PrivateKey failed to load key from file: `"
      << filename << "`";
  }
  return key;
}

// X509* SslUtils::SslDuplicateX509(const X509& src) {
//  return X509_dup(const_cast<X509*>(&src));
// }
// EVP_PKEY* SslUtils::SslDuplicateEVP_PKEY(const EVP_PKEY& src) {
//   // There are several versions here:
//   //   http://www.mail-archive.com/openssl-users@openssl.org/msg17614.html
//   //   http://www.mail-archive.com/openssl-users@openssl.org/msg17680.html
//   // Choosing the easiest one:
//
//   EVP_PKEY* k = const_cast<EVP_PKEY*>(&src);
//   ++k->references;
//   return k;
// /*
//   EVP_PKEY* pKey = const_cast<EVP_PKEY*>(&src);
//   EVP_PKEY* pDupKey = EVP_PKEY_new();
//   RSA* pRSA = EVP_PKEY_get1_RSA(pKey);
//   RSA* pRSADupKey = nullptr;
//   if(eKeyType == eKEY_PUBLIC) {
//     pRSADupKey = RSAPublicKey_dup(pRSA);
//   } else {
//     pRSADupKey = RSAPrivateKey_dup(pRSA);
//   }
//   RSA_free(pRSA);
//   EVP_PKEY_set1_RSA(pDupKey, pRSADupKey);
//   RSA_free(pRSADupKey);
//   return pDupKey;
// */
// }

std::string SslUtils::SslPrintableBio(BIO* bio) {
  char* bio_data = nullptr;
  long bio_data_size = BIO_get_mem_data(bio, &bio_data);
  if (bio_data == nullptr) {
    return "";
  }
  return absl::CEscape(absl::string_view(bio_data, bio_data_size));
}

absl::StatusOr<SSL_CTX*> SslUtils::SslCreateContext(
    absl::string_view certificate_filename,
    absl::string_view key_filename) {
  SslLibraryInit();
  X509* ssl_certificate = nullptr;
  if (!certificate_filename.empty()) {
    ASSIGN_OR_RETURN(ssl_certificate,
                     SslLoadCertificateFile(certificate_filename));
  }
  base::CallOnReturn clear_cert([ssl_certificate]() {
    if (ssl_certificate != nullptr) { X509_free(ssl_certificate); } });
  EVP_PKEY* ssl_key = nullptr;
  if (!key_filename.empty()) {
    ASSIGN_OR_RETURN(ssl_key, SslLoadPrivateKeyFile(key_filename));
  }
  base::CallOnReturn clear_key([ssl_key]() {
    if (ssl_key != nullptr) { EVP_PKEY_free(ssl_key); } });
  SSL_CTX* ssl_ctx = SSL_CTX_new(SSLv23_method());
  if (ssl_ctx == nullptr) {
    return status::InternalErrorBuilder()
      << "SSL_CTX_new failed: " << SslLastError();
  }
  base::CallOnReturn clear_ctx([ssl_ctx]() { SSL_CTX_free(ssl_ctx); });
  const long ssl_ctx_mode = SSL_CTX_get_mode(ssl_ctx);
  const long ssl_new_ctx_mode =
    ssl_ctx_mode | SSL_MODE_ENABLE_PARTIAL_WRITE |
    SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER;
  const long result = SSL_CTX_set_mode(ssl_ctx, ssl_new_ctx_mode);
  if (result != ssl_new_ctx_mode) {
    return status::InternalErrorBuilder()
      << "SSL_CTX_set_mode failed: " << SslLastError();
  }
  // The server needs certificate and key.
  // The client may optionally use certificate and key.
  if (ssl_certificate != nullptr &&
      SSL_CTX_use_certificate(ssl_ctx, ssl_certificate) <= 0) {
    return status::InternalErrorBuilder()
      << "SSL_CTX_use_certificate failed: " << SslLastError();
  }
  // Now the 'ssl_certificate' is part of 'ssl_ctx' - freed w/ 'ssl_ctx'
  clear_cert.reset();
  if (ssl_key != nullptr && SSL_CTX_use_PrivateKey(ssl_ctx, ssl_key) <= 0) {
    return status::InternalErrorBuilder()
      << "SSL_CTX_use_PrivateKey failed: " << SslLastError();
  }
  // Now the 'ssl_key' is part of 'ssl_ctx'  - freed w/ 'ssl_ctx'
  clear_key.reset();
  clear_ctx.reset();
  return ssl_ctx;
}

void SslUtils::SslDeleteContext(SSL_CTX* ssl_ctx) {
  if (ssl_ctx != nullptr) {
    SSL_CTX_free(ssl_ctx);
  }
}

SslAcceptor::SslAcceptor(Selector* selector, SslAcceptorParams params)
  : Acceptor(),
    selector_(ABSL_DIE_IF_NULL(selector)),
    params_(std::move(params)),
    tcp_acceptor_(selector, params_.tcp_params) {
  tcp_acceptor_
    .set_filter_handler(absl::bind_front(
        &SslAcceptor::TcpAcceptorFilterHandler, this))
    .set_accept_handler(absl::bind_front(
        &SslAcceptor::TcpAcceptorAcceptHandler, this));
}

SslAcceptor::~SslAcceptor() {}

absl::Status SslAcceptor::Listen(const HostPort& local_addr) {
  RETURN_IF_ERROR(SslInitialize());
  return tcp_acceptor_.Listen(local_addr);
}
void SslAcceptor::Close() {
  tcp_acceptor_.Close();
}
std::string SslAcceptor::ToString() const {
  return absl::StrCat("[SSL] ", tcp_acceptor_.ToString());
}
bool SslAcceptor::TcpAcceptorFilterHandler(const HostPort& peer_addr) {
  return CallFilterHandler(peer_addr);
}

void SslAcceptor::TcpAcceptorAcceptHandler(
    std::unique_ptr<Connection> connection) {
  auto ssl_connection = absl::make_unique<SslConnection>(
      connection->net_selector(), params_.ssl_params);
  // Set temporary handlers in the new ssl_connection. We'll be notified
  // when the ssl connect is completed. Only after the ssl connection
  // is fully established we pass it to application.
  ssl_connection->Wrap(absl::WrapUnique(
      static_cast<TcpConnection*>(connection.release())));
  ssl_connection
    ->set_connect_handler(absl::bind_front(
        &SslAcceptor::SslConnectionConnectHandler, this, ssl_connection.get()))
    .set_close_handler(absl::bind_front(
        &SslAcceptor::SslConnectionCloseHandler, this, ssl_connection.get()));
  ssl_connection.release();   // the pointer is owned by the handlers.
}


void SslAcceptor::SslConnectionConnectHandler(SslConnection* ssl_connection) {
  // Ssl connection ready, we detach our temporary handlers to let the
  // application attach and use it.
  ssl_connection->clear_all_handlers();
  // pass ssl connection to application
  CallAcceptHandler(absl::WrapUnique(ssl_connection));
}

void SslAcceptor::SslConnectionCloseHandler(
    SslConnection* ssl_connection,
    const absl::Status& status, Connection::CloseDirective directive) {
  if (directive != Connection::CLOSE_READ_WRITE ) {
    // ignore partial close
    return;
  }
  LOG(WARNING)
    << "SSL connection closed in SSL acceptor, before connect completed: "
    << status;
  // ssl connection broken, we have to delete it
  // NOTE: we are called from SslConnection !! don't use "delete" here
  ssl_connection->clear_all_handlers();
  ssl_connection->net_selector()->DeleteInSelectLoop(ssl_connection);
}

absl::Status SslAcceptor::SslInitialize() {
  if (params_.ssl_params.ssl_context == nullptr) {
    return status::FailedPreconditionErrorBuilder()
      << "SslAcceptor created without proper ssl context.";
  }
  if (SSL_CTX_check_private_key(
          params_.ssl_params.ssl_context) != 1) {
    if (!params_.ssl_params.allow_unchecked_private_key) {
      return status::FailedPreconditionErrorBuilder()
        << "No SSL certificate set for ssl context.";
    }
    LOG(WARNING) << "No SSL certificate set for ssl context.";
  }
  return absl::OkStatus();
}

SslConnection::SslConnection(Selector* selector, SslConnectionParams params)
  : Connection(selector), params_(std::move(params)) {}

SslConnection::~SslConnection() {
}

void SslConnection::SetTcpConnectionHandlers() {
  tcp_connection_
    ->set_connect_handler(absl::bind_front(
        &SslConnection::TcpConnectionConnectHandler, this))
    .set_close_handler(absl::bind_front(
        &SslConnection::TcpConnectionCloseHandler, this))
    .set_read_handler(absl::bind_front(
        &SslConnection::TcpConnectionReadHandler, this))
    .set_write_handler(absl::bind_front(
        &SslConnection::TcpConnectionWriteHandler, this));
}

void SslConnection::Wrap(std::unique_ptr<TcpConnection> tcp_connection) {
  CHECK(tcp_connection_ == nullptr);
  tcp_connection_ = std::move(tcp_connection);
  SetTcpConnectionHandlers();
  set_state(CONNECTING);
  is_server_side_.store(true);
  // resume from the point where the TCP is connected and SSL handshake starts.
  TcpConnectionConnectHandler();
}

absl::Status SslConnection::Connect(const HostPort& remote_addr) {
  CHECK(tcp_connection_ == nullptr);
  tcp_connection_ = absl::make_unique<TcpConnection>(
      net_selector(), params_.tcp_params);
  SetTcpConnectionHandlers();
  set_state(CONNECTING);
  is_server_side_.store(false);
  auto connect_status = tcp_connection_->Connect(remote_addr);
  if (!connect_status.ok()) {
    tcp_connection_.reset();
    set_state(DISCONNECTED);
    return status::Annotate(
        connect_status, "For underlying TCP connection of SSL connection");
  }
  // The next thing will be:
  //  - TcpConnectionConnectHandler:
  //       => TCP is connected and SSL handshake should start
  //  - TcpConnectionCloseHandler:
  //       => TCP broken, clear everything.
  return absl::OkStatus();
}

void SslConnection::FlushAndClose() {
  set_state(FLUSHING);
  LOG_IF_ERROR(WARNING, RequestWriteEvents(true));
}

void SslConnection::ForceClose() {
  SslClear();
  if (ABSL_PREDICT_TRUE(tcp_connection_ != nullptr)) {
    tcp_connection_->ForceClose();
  }
}
absl::Status SslConnection::SetSendBufferSize(int size) {
  if (ABSL_PREDICT_TRUE(tcp_connection_ != nullptr)) {
    return tcp_connection_->SetSendBufferSize(size);
  }
  return absl::OkStatus();
}
absl::Status SslConnection::SetRecvBufferSize(int size) {
  if (ABSL_PREDICT_TRUE(tcp_connection_ != nullptr)) {
    return tcp_connection_->SetRecvBufferSize(size);
  }
  return absl::OkStatus();
}
absl::Status SslConnection::RequestReadEvents(bool enable) {
  if (ABSL_PREDICT_TRUE(tcp_connection_ != nullptr)) {
    return tcp_connection_->RequestReadEvents(enable);
  }
  return absl::OkStatus();
}
absl::Status SslConnection::RequestWriteEvents(bool enable) {
  if (ABSL_PREDICT_TRUE(tcp_connection_ != nullptr)) {
    return tcp_connection_->RequestWriteEvents(enable);
  }
  return absl::OkStatus();
}
HostPort SslConnection::GetLocalAddress() const {
  if (ABSL_PREDICT_TRUE(tcp_connection_ != nullptr)) {
    return tcp_connection_->GetLocalAddress();
  }
  return HostPort();
}
HostPort SslConnection::GetRemoteAddress() const {
  if (ABSL_PREDICT_TRUE(tcp_connection_ != nullptr)) {
    return tcp_connection_->GetRemoteAddress();
  }
  return HostPort();
}
std::string SslConnection::ToString() const {
  std::string prefix(absl::StrCat(
      "[ SSL connection: ", state_name(),
      " server_side: ", is_server_side_.load(),
      " handshaked: ", handshake_finished_.load(),
      " read blocked: ", read_blocked_.load(),
      " read blocked on write: ", read_blocked_on_write_.load(),
      " write blocked on read: ", write_blocked_on_read_.load()));
  if (tcp_connection_ != nullptr) {
    return absl::StrCat(prefix, " - ", tcp_connection_->ToString(), " ]");
  } else {
    return absl::StrCat(prefix, " - No tcp connection]");
  }
}

void SslConnection::TcpConnectionConnectHandler() {
  auto ssl_status = SslInitialize(is_server_side_.load());
  if (!ssl_status.ok()) {
    set_last_error(ssl_status);
    ForceClose();
    return;
  }
  // Our state is still CONNECTING, the next thing is:
  //  - TCP invokes TcpConnectionWriteHandler, and the SSL handshake will begin
}

absl::Status SslConnection::TcpConnectionReadHandler() {
  // Read from TCP --> write to SSL
  for (absl::string_view chunk : tcp_connection_->inbuf()->Chunks()) {
    const int write_size = BIO_write(p_bio_read_, chunk.data(), chunk.size());
    if (write_size < int(chunk.size()) ) {
      return status::InternalErrorBuilder()
        << "BIO_write failed, closing connection: " << SslUtils::SslLastError();
    }
    ssl_in_count_.fetch_add(write_size);
  }
  tcp_connection_->inbuf()->Clear();
  if (state() == CONNECTING) {
    // still in handshake
    return SslHandshake();
  }
  if (write_blocked_on_read_.load()) {
    return RequestWriteEvents(true);
  }

  // NOTE: SSL_pending looks only inside SSL layer, and not into BIO buffer.
  //       So even if you have tons of data in BIO, SSL_pending still returns 0.

  // Read from SSL --> write back to inbuf()
  int pending = 0;
  // Note - is essential to take the max of pending bytes in the BIO and SSL -
  // the bytes are moved from the BIO to SSL at SSL_read. At the last SSL_read
  // we may end up with byte in internal SSL buffer, but BIO may be empty (not
  // yet read by SSL_read) so we need to read as long as we have any kind of
  // data in both of them.
  while ((pending = std::max(BIO_pending(p_bio_read_),
                             SSL_pending(p_ssl_))) > 0) {
    // If there is no data in p_bio_read_ then avoid calling SSL_read because
    // it would return WANT_READ and we'll get read_blocked.
    size_t scratch_size = std::min(
        params_.tcp_params.block_size, size_t(pending));
    char* buffer = new char[scratch_size];
    base::CallOnReturn clear_buffer([buffer]() { delete [] buffer; });
    const int cb = SSL_read(p_ssl_, buffer, scratch_size);

    read_blocked_.store(false);
    read_blocked_on_write_.store(false);
    if (cb < 0) {
      const int error = SSL_get_error(p_ssl_, cb);
      switch(error) {
      case SSL_ERROR_NONE:
        break;
      case SSL_ERROR_WANT_READ:
        read_blocked_.store(true);
        break;
      case SSL_ERROR_WANT_WRITE: {
        read_blocked_on_write_.store(true);
        RETURN_IF_ERROR(RequestWriteEvents(true))
          << "During SSL_ERROR_WANT_WRITE for SSL read handler.";
        break;
      }
      case SSL_ERROR_ZERO_RETURN:
        // End of data. We need to SSL_shutdown.
        FlushAndClose();
        return absl::OkStatus();
      default:
        return status::InternalErrorBuilder()
          << "SSL_read fatal, SSL_get_error: " << error
          << " " << SslUtils::SslErrorName(error)
          << " , " << SslUtils::SslLastError();
      };
      break;
    }
    // SSL_read was successful
    inbuf()->Append(absl::MakeCordFromExternal(
        absl::string_view(buffer, cb), clear_buffer.reset()));
  }
  if (read_blocked_.load() && !outbuf()->empty() ) {
    // the write has been stopped due to read_blocked_
    RETURN_IF_ERROR(RequestWriteEvents(true))
      << "For read blocked in SSL read handler.";
  }
  // in FLUSHING state discard all input. We're waiting for the outbuf() to
  // become empty so we can gracefully shutdown SSL.
  if (state() == FLUSHING) {
    inbuf()->Clear();
  }
  // ask application to read data from our inbuf()
  if (!inbuf()->empty()) {
    RETURN_IF_ERROR(CallReadHandler())
      << "While calling the read handler for SSL connection data.";
  }
  return absl::OkStatus();
}

absl::Status SslConnection::TcpConnectionWriteHandler() {
  if (state() == CONNECTING) {
    RETURN_IF_ERROR(SslHandshake())
      << "During SslHandshake in TcpConnectionWriteHandler.";
  } else if (!read_blocked_.load() && !read_blocked_on_write_.load()) {
    // ask application to write something in our outbuf()
    // [don't ask if we're FLUSHING]
    if (state() == CONNECTED) {
      RETURN_IF_ERROR(CallWriteHandler());
    }
    size_t bytes_written = 0;
    // Read from outbuf() --> write to SSL
    for (absl::string_view chunk : outbuf()->Chunks()) {
      const int cb = SSL_write(p_ssl_, chunk.data(), chunk.size());
      bytes_written += cb;
      // write - the number of encrypted bytes written in BIO, always > read
      write_blocked_on_read_.store(false);
      if (write <= 0) {
        const int error = SSL_get_error(p_ssl_, cb);
        switch(error) {
        case SSL_ERROR_WANT_READ:
          write_blocked_on_read_.store(true);
          // we need more data in p_bio_read_ so we're just gonna wait for
          // ReadHandler to happen
          outbuf()->RemovePrefix(bytes_written);
          return absl::OkStatus();
        case SSL_ERROR_WANT_WRITE:
          // p_bio_write_ is probably full.
          // But we use memory BIO, so it should never happen.
          break;
        default:
          outbuf()->RemovePrefix(bytes_written);
          return status::InternalErrorBuilder()
            << "SSL_write fatal, SSL_get_error: " << error
            << " " << SslUtils::SslErrorName(error)
            << " , " << SslUtils::SslLastError();
        }
        break;
      }
    }
    outbuf()->Clear();
  }
  // Else: a partial SSL_read is in progress. DON'T use SSL_write! or it will
  // corrupt internal ssl structures. If we don't write anything to TCP, the
  // write event will be stopped. The ReadHandler will test outbuf non empty
  // and re-enable write.

  // Read from SSL --> write to TCP
  int pending = 0;
  while ((pending = BIO_pending(p_bio_write_)) > 0) {
    size_t scratch_size = std::min(
        params_.tcp_params.block_size, size_t(pending));
    char* buffer = new char[scratch_size];
    base::CallOnReturn clear_buffer([buffer]() { delete [] buffer; });
    const int cb = BIO_read(p_bio_write_, buffer, sizeof(scratch_size));
    if (cb < 0) {
      return status::InternalErrorBuilder()
        << "BIO_read failed, closing connection: " << SslUtils::SslLastError();
    }
    ssl_out_count_.fetch_add(cb);
    tcp_connection_->outbuf()->Append(absl::MakeCordFromExternal(
        absl::string_view(buffer, cb), clear_buffer.reset()));
  }

  // If we sent every piece of data, and we are shutdown SSL
  if (state() == FLUSHING && outbuf()->empty()) {
    RETURN_IF_ERROR(SslShutdown())
      << "During SslShutdown on connection flushing.";
    net_selector_->RunInSelectLoop([this]() {
      tcp_connection_->FlushAndClose();
    });
  }
  return absl::OkStatus();
}

void SslConnection::TcpConnectionCloseHandler(
    const absl::Status& status, Connection::CloseDirective directive) {
  set_last_error(status);
  if (directive != CLOSE_READ_WRITE) {
    auto shutdown_status = SslShutdown();
    if (!shutdown_status.ok()) {
      set_last_error(shutdown_status);
    }
  } else {
    set_state(DISCONNECTED);
    CallCloseHandler(status, directive);
  }
}

int SslConnectionVerifyCallback(int preverify, X509_STORE_CTX* x509_ctx) {
  auto ssl = reinterpret_cast<SSL*>(X509_STORE_CTX_get_ex_data(
      x509_ctx, SSL_get_ex_data_X509_STORE_CTX_idx()));
  if (!preverify) {
    auto conn = reinterpret_cast<SslConnection*>(
        SSL_get_ex_data(ssl, SslConnection::SslVerificationIndex()));
    conn->SslSetVerificationFailed();
  }
  SSL_set_verify_result(ssl, preverify);
  return preverify;
}

absl::Mutex SslConnection::verification_mutex_;
std::atomic_int SslConnection::verification_index_ = ATOMIC_VAR_INIT(-1);
absl::Status SslConnection::InitializeSslVerificationIndex() {
  if (verification_index_.load() >= 0) {
    return absl::OkStatus();
  }
  absl::MutexLock l(&verification_mutex_);
  if (verification_index_.load() < 0) {
    verification_index_ = SSL_get_ex_new_index(
        0, (void*)"SSLConnection::verification_index",
        nullptr, nullptr, nullptr);
    RET_CHECK(verification_index_ > 0)
      << "Invalid SSL verification index obtained: "
      << SslUtils::SslLastError();
  }
  return absl::OkStatus();
}

absl::Status SslConnection::SslInitialize(bool is_server) {
  RET_CHECK(p_ctx_ == nullptr);
  RET_CHECK(params_.ssl_context != nullptr);
  RET_CHECK(p_ssl_ == nullptr);
  p_ctx_ = params_.ssl_context;
  verification_failed_.store(false);
  RETURN_IF_ERROR(InitializeSslVerificationIndex());

  p_ssl_ = SSL_new(p_ctx_);
  RET_CHECK(p_ssl_ != nullptr)
    << "Cannot obtain a new SSL structure: " << SslUtils::SslLastError();
  SSL_set_ex_data(p_ssl_, verification_index_, this);
  const int verify_mode = SSL_CTX_get_verify_mode(p_ctx_);
  if (verify_mode != SSL_VERIFY_NONE) {
    SSL_set_verify(p_ssl_, verify_mode, SslConnectionVerifyCallback);
  }
  p_bio_read_ = BIO_new(BIO_s_mem());
  RET_CHECK(p_bio_read_ != nullptr)
    << "Cannot allocate a new bio_read buffer: " << SslUtils::SslLastError();
  p_bio_write_ = BIO_new(BIO_s_mem());
  RET_CHECK(p_bio_write_ != nullptr)
    << "Cannot allocate a new bio_write buffer: " << SslUtils::SslLastError();
  SSL_set_bio(p_ssl_, p_bio_read_, p_bio_write_);
  if (is_server) {
    SSL_set_accept_state(p_ssl_);
  } else {
    SSL_set_connect_state(p_ssl_);
  }
  return absl::OkStatus();
}

void SslConnection::SslClear() {
  if (p_ssl_ != nullptr) {
    // SSL_free also deletes the associated BIOs
    SSL_free(p_ssl_);
    p_ssl_ = nullptr;
    // BIO_free_all(p_bio_read_), BIO_free_all(p_bio_write_) => No need.
    p_bio_read_ = nullptr;
    p_bio_write_ = nullptr;
  }
  if (p_bio_read_ != nullptr) {
    BIO_free_all(p_bio_read_);
    p_bio_read_ = nullptr;
  }
  if (p_bio_write_ != nullptr) {
    BIO_free_all(p_bio_write_);
    p_bio_write_ = nullptr;
  }
  // We do not own the SSL_CTX.
  // We only received this pointer by SslConnectionParams.
  p_ctx_ = nullptr;

  // TODO(cosmin): the SslConnection is not reusable, for the time being.
  // handshake_finished_ = false;
}

absl::Status SslConnection::SslHandshake() {
  if (handshake_finished_.load()) {
    return absl::OkStatus();
  }
  if (SSL_is_init_finished(p_ssl_)) {
    // SSL completed the handshake but did we empty the ssl buffers?
    if ( BIO_pending(p_bio_write_) > 0 ) {
      RETURN_IF_ERROR(RequestWriteEvents(true))
        << "After SslHandshake finished - delaying connect handler per pending "
        "writed: " << BIO_pending(p_bio_write_);
      return absl::OkStatus();
    }
    handshake_finished_.store(true);
    set_state(CONNECTED);
    net_selector_->RunInSelectLoop(absl::bind_front(
        &SslConnection::CallConnectHandler, this));
    return absl::OkStatus();
  }
  const int result = SSL_do_handshake(p_ssl_);
  // For some reason this seems to never get set.
  // const int verify_result = SSL_get_verify_result(p_ssl_);
  if (result < 1 || verification_failed_.load()) {
    const int error = SSL_get_error(p_ssl_, result);
    if (verification_failed_.load() ||
        (error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE)) {
      return status::InternalErrorBuilder()
        << "SSL_do_handshake failed: " << SslUtils::SslErrorName(error)
        << " ssl last error: " << SslUtils::SslLastError();
    }
    // Handshake still in progress..
    RETURN_IF_ERROR(RequestWriteEvents(true))
      << "During want read / write fulfillment in Ssl handshake.";
    // Next thing: TcpConnectionWriteHandler will read from SSl
    //   --> write to TCP and will call SslHandshake again.
    return absl::OkStatus();
  }
  // The handshake is completed for this endpoint(SSL_do_handshake returned 1).
  // But maybe we need to send some data to the other endpoint.
  int ssl_want = SSL_want(p_ssl_);
  VLOG(1) << "ssl_want: " << ssl_want << " " << SslUtils::SslWantName(ssl_want)
          << ", BIO_pending(p_bio_write_): " << BIO_pending(p_bio_write_)
          << ", BIO_pending(p_bio_read_): " << BIO_pending(p_bio_read_);
  RETURN_IF_ERROR(RequestWriteEvents(true));
  // Next thing TcpConnectionWriteHandler will read from SSl
  // --> write to TCP and will call SslHandshake again.
  return absl::OkStatus();
}

absl::Status SslConnection::SslShutdown() {
  if (p_ssl_ == nullptr) {
    return absl::OkStatus();
  }
  const int result = SSL_shutdown(p_ssl_);
  if (result < 0 ) {
    const int error = SSL_get_error(p_ssl_, result);
    LOG(WARNING) << "SSL_shutdown error: " << SslUtils::SslErrorName(error)
                 << " detail: " << SslUtils::SslLastError();
  }
  // Read from SSL --> write to TCP (the last SSL close signal)
  int pending = 0;
  while ((pending = BIO_pending(p_bio_write_)) > 0 ) {
    size_t scratch_size = std::min(
        params_.tcp_params.block_size, size_t(pending));
    char* buffer = new char[scratch_size];
    base::CallOnReturn clear_buffer([buffer]() { delete [] buffer; });
    const int cb = BIO_read(p_bio_write_, buffer, scratch_size);
    if (cb < 0) {
      return status::InternalErrorBuilder()
        << "BIO_read failed on SslShutdown: " << SslUtils::SslLastError();
    }
    ssl_out_count_.fetch_add(cb);
    tcp_connection_->outbuf()->Append(absl::MakeCordFromExternal(
        absl::string_view(buffer, cb), clear_buffer.reset()));
  }
  return absl::OkStatus();
}


}  // namespace net
}  // namespace whisper
