#ifndef WHISPERLIB_NET_SSL_CONNECTION_H_
#define WHISPERLIB_NET_SSL_CONNECTION_H_

#include <atomic>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "openssl/err.h"
#include "openssl/ssl.h"
#include "whisperlib/net/address.h"
#include "whisperlib/net/connection.h"

namespace whisper {
namespace net {

class SslUtils {
 public:
  static absl::string_view SslErrorName(int err);
  static absl::string_view SslWantName(int want);
  // Returns a description of the last SSL error. This function pops error
  // values from SSL stack, so don't call it twice.
  static std::string SslLastError();
  // Globally initialize SSL library.
  static void SslLibraryInit();

  // Returns a new X509 structure, or NULL on failure.
  // You have to call X509_free(..) on the valid result.
  static absl::StatusOr<X509*> SslLoadCertificateFile(
      absl::string_view filename);
  // Returns a new EVP_PKEY structure, or NULL on failure.
  // You have to call EVP_PKEY_free(..) on the valid result.
  static absl::StatusOr<EVP_PKEY*> SslLoadPrivateKeyFile(
      absl::string_view filename);

  // Clone X509. Never fail. Use X509_free(..).
  // static X509* SslDuplicateX509(const X509& src);
  // Clone EVP_PKEY. Never fail. Use EVP_PKEY_free(..).
  // static EVP_PKEY* SslDuplicateEVP_PKEY(const EVP_PKEY& src);

  // returns a description of all data buffered in "bio"
  static std::string SslPrintableBio(BIO* bio);

  // Returns a new SSL_CTX structure, or NULL on failure.
  // It also loads SSL certificate and key if non-empty strings.
  // You have to call SslDeleteContext(..) on the valid result.
  static absl::StatusOr<SSL_CTX*> SslCreateContext(
      absl::string_view certificate_filename = "",
      absl::string_view key_filename = "");
  // Free SSL context. This is the reverse of SslCreateContext(..).
  static void SslDeleteContext(SSL_CTX* ssl_ctx);
};

struct SslConnectionParams {
  // Used for all ssl related operations - just a reference to a context
  // prepared in advance by the application.
  SSL_CTX* ssl_context = nullptr;
  // If we allow unchecked private keys on the ssl context - for testing only.
  bool allow_unchecked_private_key = false;
  // Parameters for the underlying TCP connection.
  TcpConnectionParams tcp_params;
};

struct SslAcceptorParams {
  // Parameters for the underlying TCP-based acceptor
  TcpAcceptorParams tcp_params;
  // Parameters for the accepted SSL-based connections.
  SslConnectionParams ssl_params;
};

class SslConnection;
class SslAcceptor : public Acceptor {
 public:
  SslAcceptor(Selector* selector, SslAcceptorParams params);
  ~SslAcceptor() override;

  // Acceptor interface methods
  absl::Status Listen(const HostPort& local_addr) override;
  void Close() override;
  std::string ToString() const override;

  bool TcpAcceptorFilterHandler(const whisper::net::HostPort& peer_addr);
  void TcpAcceptorAcceptHandler(std::unique_ptr<Connection> connection);

  void SslConnectionConnectHandler(SslConnection* ssl_connection);
  void SslConnectionCloseHandler(SslConnection* ssl_connection,
                                 const absl::Status& status,
                                 Connection::CloseDirective directive);

 protected:
  // Initialize SSL members
  absl::Status SslInitialize();

  // The selector in which we operate our listening / tcp_acceptor_.
  whisper::net::Selector* selector_;
  // Parameters for this acceptor
  SslAcceptorParams params_;
  // the underlying TCP acceptor
  TcpAcceptor tcp_acceptor_;
};

class SslConnection : public Connection {
 public:
  SslConnection(Selector* selector, SslConnectionParams params);
  ~SslConnection() override;

  ////////// Connection interface methods
  absl::Status Connect(const HostPort& remote_addr) override;
  void FlushAndClose() override;
  void ForceClose() override;
  absl::Status SetSendBufferSize(int size) override;
  absl::Status SetRecvBufferSize(int size) override;
  absl::Status RequestReadEvents(bool enable) override;
  absl::Status RequestWriteEvents(bool enable) override;
  HostPort GetLocalAddress() const override;
  HostPort GetRemoteAddress() const override;
  std::string ToString() const override;

  // Used from the depth of the ssl verification callback to set the
  // verification status failed
  void SslSetVerificationFailed() { verification_failed_.store(true); }
  static int SslVerificationIndex() { return verification_index_.load(); }

 private:
  // Use an already established tcp connection. Usually obtained by an acceptor.
  // We take ownership of tcp_connection.
  void Wrap(std::unique_ptr<TcpConnection> tcp_connection);
  friend class SslAcceptor;

  // Handlers for events in the underlying tcp_connection.
  void TcpConnectionConnectHandler();
  absl::Status TcpConnectionReadHandler();
  absl::Status TcpConnectionWriteHandler();
  void TcpConnectionCloseHandler(const absl::Status& status,
                                 Connection::CloseDirective directive);
  // Sets the above handlers to the underlying tcp connection.
  void SetTcpConnectionHandlers();

  // Initializes the internal ssl structures.
  absl::Status SslInitialize(bool is_server);
  // Clears the internal ssl structured.
  void SslClear();

  // Performs the SSL handshake.
  absl::Status SslHandshake();
  // Performs the SSL shutdown.
  absl::Status SslShutdown();

  // Used to initialize the verification index.
  static absl::Status InitializeSslVerificationIndex();

  // Parameters for this ssl connection:
  SslConnectionParams params_;
  // The underlying TCP connection:
  std::unique_ptr<TcpConnection> tcp_connection_;

  // The maximum size of BIO buffers
  static const size_t kSslBufferSize = 10000;

  // The OpenSSL structures
  // Context - also contains the certificate and key.
  SSL_CTX* p_ctx_ = nullptr;
  // Network --> SSL , use BIO_write(p_bio_read_, ...)
  BIO* p_bio_read_ = nullptr;
  // Network <-- SSL , use BIO_read(p_bio_write_, ...)
  BIO* p_bio_write_ = nullptr;
  // Specific SSL structure for this connextion
  SSL* p_ssl_ = nullptr;

  // true = this is a server side connection
  // false = this is a client side connection
  std::atomic_bool is_server_side_ = ATOMIC_VAR_INIT(false);
  // If we finished the ssl handshake for this connection.
  std::atomic_bool handshake_finished_ = ATOMIC_VAR_INIT(false);

  // helpers for sequencing reads/writes of complete SSL packets.
  std::atomic_bool read_blocked_ = ATOMIC_VAR_INIT(false);
  std::atomic_bool read_blocked_on_write_ = ATOMIC_VAR_INIT(false);
  std::atomic_bool write_blocked_on_read_ = ATOMIC_VAR_INIT(false);

  // for debug, count output/input bytes
  std::atomic_uint64_t ssl_out_count_ = ATOMIC_VAR_INIT(0);
  std::atomic_uint64_t ssl_in_count_ = ATOMIC_VAR_INIT(0);

  // If the ssl verification failed:
  std::atomic_bool verification_failed_ = ATOMIC_VAR_INIT(false);

  // Guards the verification index:
  static absl::Mutex verification_mutex_;
  // Ssl index registerd for setting custom data to SSL object.
  static std::atomic_int verification_index_;
};

}  // namespace net
}  // namespace whisper

#endif  // WHISPERLIB_NET_SSL_CONNECTION_H_
