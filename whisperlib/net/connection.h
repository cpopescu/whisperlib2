#ifndef WHISPERLIB_NET_CONNECTION_H_
#define WHISPERLIB_NET_CONNECTION_H_

#include <memory>
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "whisperlib/net/address.h"
#include "whisperlib/net/dns_resolve.h"
#include "whisperlib/net/selectable.h"
#include "whisperlib/net/selector.h"
#include "whisperlib/net/timeouter.h"

namespace whisper {
namespace net {

// Uses ::getsockopt to extract the last socket error from provided socket fd.
int ExtractSocketErrno(int fd);
class Connection;

class Acceptor {
 public:
  explicit Acceptor() = default;
  virtual ~Acceptor();

  // Starts acceptor in the listen mode.
  virtual absl::Status Listen(const HostPort& local_addr) = 0;
  // Closes the acceptor.
  virtual void Close() = 0;
  // Returns a human description of this acceptor, for logging / human
  // consumption.
  virtual std::string ToString() const = 0;

  enum State { DISCONNECTED, LISTENING, };
  static absl::string_view StateName(State s);

  // The current state:
  State state() const;
  // The name of the current state:
  absl::string_view state_name() const;
  // The local address we are listening on:
  HostPort local_address() const;
  // The last error code encountered by this acceptor:
  absl::Status last_error() const;

  // We call this handler to notify the application that a new client wants
  // to connect. If the handler returns true, we proceed with connection setup,
  // else we reject this guy.
  using FilterHandler = std::function<bool(const HostPort&)>;
  Acceptor& set_filter_handler(FilterHandler handler);
  Acceptor& clear_filter_handler();

  // We call this handler to deliver a fully connected client to application.
  using AcceptHandler = std::function<void(std::unique_ptr<Connection>)>;
  Acceptor& set_accept_handler(AcceptHandler handler);
  Acceptor& clear_accept_handler();

  // We call this handler when the accepting socket is closed, due to
  // an error encountered.
  using CloseHandler = std::function<void(const absl::Status&)>;
  Acceptor& set_close_handler(CloseHandler handler);
  Acceptor& clear_close_handler();

 protected:
  // Sets the internal state:
  void set_state(State value);
  // Sets the address on which we are listening:
  void set_local_address(HostPort value);
  // Sets the last error status encountered:
  void set_last_error(const absl::Status& value);

  // Filters an incoming client peer address through filter_handler_:
  bool CallFilterHandler(const HostPort& peer_address);
  // Called when a remote network connection was accepted, invoking
  // accept_handler_:
  void CallAcceptHandler(std::unique_ptr<Connection> new_connection);
  // Called when the acceptor is closed due some internal error or whatever
  // bad happened after Listen.
  void CallCloseHandler(const absl::Status& status);

  // The state we are in
  std::atomic<State> state_ = ATOMIC_VAR_INIT(DISCONNECTED);
  // Protects the setting of the internal information members.
  mutable absl::Mutex mutex_;
  // Locks access to internal thread sensitive data.
  // socket local address
  HostPort local_address_ ABSL_GUARDED_BY(mutex_);
  // the last error recorded for - for log & debug only.
  absl::Status last_error_ ABSL_GUARDED_BY(mutex_);
  // Filters the incoming connections.
  FilterHandler filter_handler_ = nullptr;
  // Passes incoming connections to the user layer.
  AcceptHandler accept_handler_ = nullptr;
  // Informs any third party that the acceptor encountered an error.
  CloseHandler close_handler_ = nullptr;
  // Log in detail about this acceptor.
  bool detail_log_ = false;
};

class Connection {
public:
  explicit Connection(Selector* net_selector);
  virtual ~Connection() = default;

  // Starts connection to a remote address. If successful, the connection
  // is actually pending, when completed ok, the connect handler will be called,
  // else the close handler is called.
  virtual absl::Status Connect(const HostPort& addr) = 0;
  // Starts the normal close process, by flushing the buffers and closing
  // of the connection.
  virtual void FlushAndClose() = 0;
  // Starts the abrupt close process, supposedly in case of an error.
  virtual void ForceClose() = 0;

  // Connection tuning of the send and receive buffer sizes:
  virtual absl::Status SetSendBufferSize(int size) = 0;
  virtual absl::Status SetRecvBufferSize(int size) = 0;

  // Marks the connection desire to read from the associated remote source.
  virtual absl::Status RequestReadEvents(bool enable) = 0;
  // Marks the connection desire to write to the associated remote destination.
  virtual absl::Status RequestWriteEvents(bool enable) = 0;

  // Return local connection coordinates.
  virtual HostPort GetLocalAddress() const = 0;
  // Return remote connection coordinates.
  virtual HostPort GetRemoteAddress() const = 0;

  // Returns a human description of this connection, for logging / human
  // consumption.
  virtual std::string ToString() const = 0;

  //////////////////// Accessors

  enum State { DISCONNECTED, RESOLVING, CONNECTING, CONNECTED, FLUSHING, };
  static absl::string_view StateName(State value);

  // Returns the currently associated selector.
  const Selector* net_selector() const;
  Selector* net_selector();
  // Returns the state this connection is in.
  State state() const;
  // Returns the stated for this connection as a string.
  absl::string_view state_name() const;
  // Returns the last encountered error code.
  absl::Status last_error() const;

  // Returns the number of bytes written by this connection.
  int64_t count_bytes_written() const;
  // Returns the number of bytes read by this connection.
  int64_t count_bytes_read() const;

  // Then input data from the remote peer.
  absl::Cord* inbuf();
  // Then output data for the remote peer.
  absl::Cord* outbuf();

  //////////////////// Connect / Close / Read / Write handlers

  // Handling function for connection "connect" completion:
  using ConnectHandler = std::function<void()>;
  Connection& set_connect_handler(ConnectHandler handler);
  Connection& clear_connect_handler();

  // Handling function for connection data reading:
  using ReadHandler = std::function<absl::Status()>;
  Connection& set_read_handler(ReadHandler handler);
  Connection& clear_read_handler();

  // Handling function for connection data writing:
  using WriteHandler = std::function<absl::Status()>;
  Connection& set_write_handler(WriteHandler handler);
  Connection& clear_write_handler();

  // What to close in a connection: read half / write half / both.
  enum CloseDirective { CLOSE_READ, CLOSE_WRITE, CLOSE_READ_WRITE, };
  static absl::string_view CloseDirectiveName(CloseDirective value);

  // Handling of a close directive request:
  using CloseHandler = std::function<void(const absl::Status&, CloseDirective)>;
  Connection& set_close_handler(CloseHandler handler);
  Connection& clear_close_handler();

  // Clear all connection handlers:
  Connection& clear_all_handlers();

  //////////////////// Writing to the remote destination

  // Appends the content of the buffer to the outbuf_ and registers
  // the desire for write I/O operation.
  void Write(const absl::Cord& buffer);
  void Write(absl::Cord&& buffer);
  void Write(absl::string_view buffer);
  void Write(std::string&& buffer);

 protected:
  // Sets the internal selector.
  // Note: can be called when the current selector is null.
  void set_net_selector(Selector* value);
  // Sets the current connection state.
  void set_state(State value);
  // Sets the code of the last encountered error.
  void set_last_error(const absl::Status& value);
  // Increments the count of bytes read, by this value.
  void inc_bytes_read(int64_t value);
  // Increments the count of bytes written, by this value.
  void inc_bytes_written(int64_t value);

  // Calls the registered connect handler.
  void CallConnectHandler();
  // Calls the registered read handler and returns the result.
  absl::Status CallReadHandler();
  // Calls the registered write handler and returns the result.
  absl::Status CallWriteHandler();
  // Calls the registered close handler with provided error code and directive.
  void CallCloseHandler(const absl::Status& status, CloseDirective directive);

  Selector* net_selector_ = nullptr;
  std::atomic<State> state_ = ATOMIC_VAR_INIT(DISCONNECTED);

  // Protects the setting of the internal information members.
  mutable absl::Mutex mutex_;
  // The last registered error.
  absl::Status last_error_ ABSL_GUARDED_BY(mutex_);

  ConnectHandler connect_handler_ = nullptr;
  ReadHandler read_handler_ = nullptr;
  WriteHandler write_handler_ = nullptr;
  CloseHandler close_handler_ = nullptr;

  std::atomic_int64_t count_bytes_written_ = ATOMIC_VAR_INIT(0);
  std::atomic_int64_t count_bytes_read_ = ATOMIC_VAR_INIT(0);

  // Buffer with data from remote address to us.
  // - should be accessed only from selector thread.
  absl::Cord inbuf_;
  // Buffer with data from us to remote address.
  // - should be accessed only from selector thread.
  absl::Cord outbuf_;
  // Log in detail about this connection.
  bool detail_log_ = false;
};

struct TcpConnectionParams {
  // Buffer size for send operation for the underlying socket.
  absl::optional<size_t> send_buffer_size;
  // Buffer size for receive operation for the underlying socket.
  absl::optional<size_t> recv_buffer_size;
  // Buffered read operations are limited to this size.
  absl::optional<size_t> read_limit;
  // Buffered write operations are limited to this size.
  absl::optional<size_t> write_limit;
  // Block size for buffered reads and writes.
  size_t block_size = 16384UL;
  // During unconfirmed shutdown, linger this long before closing.
  absl::Duration shutdown_linger_timeout = absl::Seconds(5);
  // If detail description should be logged about this connection.
  bool detail_log = false;

  TcpConnectionParams& set_send_buffer_size(size_t value);
  TcpConnectionParams& set_recv_buffer_size(size_t value);
  TcpConnectionParams& set_read_limit(size_t value);
  TcpConnectionParams& set_write_limit(size_t value);
  TcpConnectionParams& set_block_size(size_t value);
  TcpConnectionParams& set_shutdown_linger_timeout(absl::Duration value);
  TcpConnectionParams& set_detail_log(bool value);
};

class AcceptorThreads {
 public:
  AcceptorThreads() = default;
  AcceptorThreads(AcceptorThreads&& other)
    : next_client_thread_(other.next_client_thread_.load()),
      client_threads_(std::move(other.client_threads_)) {}
  AcceptorThreads(const AcceptorThreads& other)
    : next_client_thread_(other.next_client_thread_.load()),
      client_threads_(other.client_threads_) {}
  AcceptorThreads& operator=(AcceptorThreads&& other) {
    next_client_thread_.store(other.next_client_thread_.load());
    client_threads_ = std::move(other.client_threads_);
    return *this;
  }
  AcceptorThreads& set_client_threads(
      std::vector<SelectorThread*> client_threads);
  Selector* GetNextSelector();

 private:
  std::atomic_size_t next_client_thread_ = ATOMIC_VAR_INIT(0);
  std::vector<SelectorThread*> client_threads_;
};

struct TcpAcceptorParams {
  // Threads that run the acceptor selector threads.
  AcceptorThreads acceptor_threads;
  // Parameters for the TCP connection in the acceptor.
  TcpConnectionParams tcp_connection_params;
  // Maximum number of connections to have in waiting, and not yet accepted.
  size_t max_backlog = 100;
  // If detail description should be logged about this acceptor.
  bool detail_log = false;

  TcpAcceptorParams& set_acceptor_threads(AcceptorThreads value);
  TcpAcceptorParams& set_tcp_connection_params(TcpConnectionParams value);
  TcpAcceptorParams& set_max_backlog(size_t value);
  TcpAcceptorParams& set_detail_log(bool value);
};

class TcpAcceptor : public Acceptor, private Selectable {
 public:
  TcpAcceptor(Selector* selector, TcpAcceptorParams params);
  ~TcpAcceptor() override;

  struct Statistics {
    // Number of hang-ups received on the acceptor socket.
    std::atomic_size_t hang_ups_handled = ATOMIC_VAR_INIT(0);
    // Number of errors received on the acceptor socket.
    // This can be at most 1, as the acceptor is closed upon errors.
    std::atomic_size_t errors_handled = ATOMIC_VAR_INIT(0);
    // Errors parsing the peer client address.
    std::atomic_size_t peer_parse_errors = ATOMIC_VAR_INIT(0);
    // Client connections filtered by the filter handler.
    std::atomic_size_t filtered_connections = ATOMIC_VAR_INIT(0);
    // Fully accepted clients, that got scheduled for their connection
    // completion in the proper allocated selector.
    std::atomic_size_t connections_accept_scheduled = ATOMIC_VAR_INIT(0);
    // Fully accepted clients, with their connection completion executed.
    std::atomic_size_t connections_accepted = ATOMIC_VAR_INIT(0);
    // Accepted clients that could not wrap their assigned socket.
    std::atomic_size_t connection_wrap_errors = ATOMIC_VAR_INIT(0);
    // Successfully accepted and initialized clients.
    std::atomic_size_t connections_initialized = ATOMIC_VAR_INIT(0);
  };
  const Statistics& stats() const;

  ////////// Acceptor interface override:

  absl::Status Listen(const HostPort& local_addr) override;
  void Close() override;
  std::string ToString() const override;

 private:
  ////////// Selectable interface override - private.
  // - Should be called from the selector thread.
  bool HandleReadEvent(SelectorEventData event) override;
  bool HandleWriteEvent(SelectorEventData event) override;
  bool HandleErrorEvent(SelectorEventData event) override;
  int GetFd() const override;
  // Close is already defined above under the same signature.

  // Initializes a new connection in the provided selector.
  void InitializeAcceptedConnection(Selector* selector, int client_fd);
  // Reads local_address from associate file descriptor socket.
  absl::Status InitializeLocalAddress();
  // Sets normal socket options: non-blocking, fast bind reusing.
  absl::Status SetSocketOptions();
  // Close internal socket fd_.
  void InternalClose(const absl::Status& status);

  // Parameters for this acceptor.
  TcpAcceptorParams params_;
  // The fd of the socket
  std::atomic_int fd_ = ATOMIC_VAR_INIT(kInvalidFdValue);
  // Statistics of our run.
  Statistics stats_;
};

class TcpConnection
  : public Connection,
    private Selectable {
 public:
  TcpConnection(Selector* selector, TcpConnectionParams params);
  virtual ~TcpConnection();

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

  // Starts the process of closing of the communication, and of the
  // eventual closing of the connection, for a connected connection.
  void CloseCommunication(CloseDirective directive);

 private:

  ////////// Selectable interface methods
  // - Should be called from the selector thread.
  bool HandleReadEvent(SelectorEventData event) override;
  bool HandleWriteEvent(SelectorEventData event) override;
  bool HandleErrorEvent(SelectorEventData event) override;
  int GetFd() const override;
  void Close() override;

  //////////////////////////////////////////////////////////////////////

  // Use an already connected fd - this is the way the TcpAcceptor initializes
  // the connection. The local and peer addresses are obtained from fd.
  absl::Status Wrap(int fd);
  friend class TcpAcceptor;

  bool read_closed() const { return read_closed_.load(); }
  bool write_closed() const { return write_closed_.load(); }
  void set_read_closed(bool value) { read_closed_.store(value); }
  void set_write_closed(bool value) { write_closed_.store(value); }

  // Internal event processing - Should be called from the selector thread.
  // immediate closes the underlying file descriptor.
  void InternalClose(const absl::Status& status, bool call_close_handler);
  // The timeouter handler
  void HandleTimeoutEvent(int64_t timeout_id);
  // The DNS resolve handler:
  void HandleDnsResult(absl::StatusOr<std::shared_ptr<DnsHostInfo>> info);

  // Sets normal socket options: non-blocking, disable Nagel, apply tcp params.
  absl::Status SetSocketOptions();

  // Reads the local address from the socket and sets it into local_address_.
  absl::Status InitializeLocalAddress();
  // Reads the remote address from the socket and sets it into remote_address_;
  absl::Status InitializeRemoteAddress();
  // We need some extra checks for CallCloseHandler.
  void CallCloseHandler(const absl::Status& status, CloseDirective directive);
  // A deferred connect completion, that is scheduled on the first i/o event/
  bool PerformConnectOnFirstOperation();
  // Helper for reading from the input fd_.
  absl::StatusOr<ssize_t> PerformRead();

  // Id for the timeout raised by this connection.
  static constexpr int64_t kShutdownTimeoutId = -100;

  // parameters for this connection
  TcpConnectionParams params_;
  // The fd of the socket used by this connection.
  std::atomic_int fd_ = ATOMIC_VAR_INIT(kInvalidFdValue);
  // Local address bound to the connection socket.
  HostPort local_address_;
  // Remote(peer) address bound to the connection socket.
  HostPort remote_address_;
  // true = the write half of the connection is closed; no more transmissions.
  std::atomic_bool write_closed_ = ATOMIC_VAR_INIT(false);
  // true = the read half of the connection is closed; no more receptions.
  std::atomic_bool read_closed_ = ATOMIC_VAR_INIT(false);
  // Timestamp of the last read event
  std::atomic_int64_t last_read_ts_ =
    ATOMIC_VAR_INIT(absl::ToUnixNanos(absl::InfinitePast()));
  // Timestamp of the last write event
  std::atomic_int64_t last_write_ts_ =
    ATOMIC_VAR_INIT(absl::ToUnixNanos(absl::InfinitePast()));
  // Raises timeouts for this connection.
  Timeouter timeouter_;
  // Set if a close is requested while doing dns resolve.
  absl::optional<bool> close_on_resolve_;
};

}  // namespace net
}  // namespace whisper

#endif  // WHISPERLIB_NET_CONNECTION_H_
