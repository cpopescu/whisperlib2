#include "whisperlib/net/connection.h"

#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include "absl/functional/bind_front.h"
#include "whisperlib/base/call_on_return.h"
#include "whisperlib/io/errno.h"

namespace whisper {
namespace net {

int ExtractSocketErrno(int fd) {
  int err = 0;
  socklen_t len = sizeof(err);
  if (ABSL_PREDICT_FALSE(::getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) <
                         0)) {
    return errno;
  }
  return err;
}
bool IsProperError(int err) {
  return err != 0 && err != EAGAIN && err != EWOULDBLOCK;
}
const sockaddr* AsSockAddr(const sockaddr_storage* addr) {
  return reinterpret_cast<const sockaddr*>(addr);
}
sockaddr* AsSockAddr(sockaddr_storage* addr) {
  return reinterpret_cast<sockaddr*>(addr);
}
size_t SockAddrLen(const sockaddr_storage& addr) {
  return addr.ss_family == AF_INET ? sizeof(struct sockaddr_in)
                                   : sizeof(struct sockaddr_in6);
}

Acceptor::~Acceptor() {
  // The super class should call Close() in destructor;
  // we cannot call it, because Close is virtual.
  CHECK_EQ(state(), DISCONNECTED);
}

absl::string_view Acceptor::StateName(Acceptor::State s) {
  switch (s) {
    case DISCONNECTED:
      return "DISCONNECTED";
    case LISTENING:
      return "LISTENING";
    default:
      return "UNKNOWN";
  }
}
Acceptor::State Acceptor::state() const { return state_.load(); }
absl::string_view Acceptor::state_name() const {
  return Acceptor::StateName(state());
}
HostPort Acceptor::local_address() const {
  absl::ReaderMutexLock l(&mutex_);
  return local_address_;
}
absl::Status Acceptor::last_error() const {
  absl::ReaderMutexLock l(&mutex_);
  return last_error_;
}
void Acceptor::set_state(State value) { state_.store(value); }
void Acceptor::set_local_address(HostPort value) {
  absl::WriterMutexLock l(&mutex_);
  local_address_ = std::move(value);
}
void Acceptor::set_last_error(const absl::Status& value) {
  if (!value.ok()) {
    LOG_IF(WARNING, detail_log_)
        << ToString() << " - Updating error to: " << value;
    absl::WriterMutexLock l(&mutex_);
    status::UpdateOrAnnotate(last_error_, value);
  }
}

Acceptor& Acceptor::set_filter_handler(FilterHandler handler) {
  filter_handler_ = std::move(handler);
  return *this;
}
Acceptor& Acceptor::clear_filter_handler() {
  filter_handler_ = nullptr;
  return *this;
}
Acceptor& Acceptor::set_accept_handler(AcceptHandler handler) {
  accept_handler_ = std::move(handler);
  return *this;
}
Acceptor& Acceptor::clear_accept_handler() {
  accept_handler_ = nullptr;
  return *this;
}
Acceptor& Acceptor::set_close_handler(CloseHandler handler) {
  close_handler_ = handler;
  return *this;
}
Acceptor& Acceptor::clear_close_handler() {
  close_handler_ = nullptr;
  return *this;
}

bool Acceptor::CallFilterHandler(const net::HostPort& peer_address) {
  return (filter_handler_ == nullptr) || (filter_handler_(peer_address));
}
void Acceptor::CallAcceptHandler(std::unique_ptr<Connection> new_connection) {
  if (ABSL_PREDICT_TRUE(accept_handler_ != nullptr)) {
    accept_handler_(std::move(new_connection));
  } else {
    LOG_IF(WARNING, detail_log_)
        << ToString() << " - No accept handler provided for connection: "
        << new_connection->ToString() << " - will be dropped.";
    LOG_EVERY_N(WARNING, 500)
        << ToString() << " - No accept handler provided for connection: "
        << new_connection->ToString() << " - will be dropped.";
    auto pconnection = new_connection.release();
    pconnection->net_selector()->RunInSelectLoop([pconnection]() {
      pconnection->ForceClose();
      delete pconnection;
    });
  }
}
void Acceptor::CallCloseHandler(const absl::Status& status) {
  if (close_handler_ != nullptr) {
    close_handler_(status);
  }
}

absl::string_view Connection::StateName(State value) {
  switch (value) {
    case DISCONNECTED:
      return "DISCONNECTED";
    case RESOLVING:
      return "RESOLVING";
    case CONNECTING:
      return "CONNECTING";
    case CONNECTED:
      return "CONNECTED";
    case FLUSHING:
      return "FLUSHING";
    default:
      return "UNKNOWN";
  }
}
absl::string_view Connection::CloseDirectiveName(CloseDirective value) {
  switch (value) {
    case CLOSE_READ:
      return "CLOSE_READ";
    case CLOSE_WRITE:
      return "CLOSE_WRITE";
    case CLOSE_READ_WRITE:
      return "CLOSE_READ_WRITE";
    default:
      return "UNKNOWN";
  }
}

Connection::Connection(Selector* net_selector) : net_selector_(net_selector) {}

const Selector* Connection::net_selector() const { return net_selector_; }
Selector* Connection::net_selector() { return net_selector_; }
Connection::State Connection::state() const { return state_.load(); }
absl::string_view Connection::state_name() const {
  return Connection::StateName(state());
}
absl::Status Connection::last_error() const {
  absl::MutexLock l(&mutex_);
  return last_error_;
}
int64_t Connection::count_bytes_written() const {
  return count_bytes_written_.load();
}
int64_t Connection::count_bytes_read() const {
  return count_bytes_read_.load();
}
absl::Cord* Connection::inbuf() { return &inbuf_; }
absl::Cord* Connection::outbuf() { return &outbuf_; }

Connection& Connection::set_connect_handler(ConnectHandler handler) {
  connect_handler_ = std::move(handler);
  return *this;
}
Connection& Connection::clear_connect_handler() {
  connect_handler_ = nullptr;
  return *this;
}
Connection& Connection::set_read_handler(ReadHandler handler) {
  read_handler_ = std::move(handler);
  return *this;
}
Connection& Connection::clear_read_handler() {
  read_handler_ = nullptr;
  return *this;
}
Connection& Connection::set_write_handler(WriteHandler handler) {
  write_handler_ = std::move(handler);
  return *this;
}
Connection& Connection::clear_write_handler() {
  write_handler_ = nullptr;
  return *this;
}
Connection& Connection::set_close_handler(CloseHandler handler) {
  close_handler_ = std::move(handler);
  return *this;
}
Connection& Connection::clear_close_handler() {
  close_handler_ = nullptr;
  return *this;
}
Connection& Connection::clear_all_handlers() {
  connect_handler_ = nullptr;
  read_handler_ = nullptr;
  write_handler_ = nullptr;
  close_handler_ = nullptr;
  return *this;
}

void Connection::Write(const absl::Cord& buffer) {
  outbuf()->Append(buffer);
  LOG_IF_ERROR(WARNING, RequestWriteEvents(true));
}
void Connection::Write(absl::Cord&& buffer) {
  outbuf()->Append(buffer);
  LOG_IF_ERROR(WARNING, RequestWriteEvents(true));
}
void Connection::Write(absl::string_view buffer) {
  outbuf()->Append(buffer);
  LOG_IF_ERROR(WARNING, RequestWriteEvents(true));
}
void Connection::Write(std::string&& buffer) {
  outbuf()->Append(buffer);
  LOG_IF_ERROR(WARNING, RequestWriteEvents(true));
}

void Connection::set_net_selector(Selector* value) {
  CHECK(net_selector_ == nullptr);
  net_selector_ = value;
}
void Connection::set_state(State value) { state_.store(value); }
void Connection::set_last_error(const absl::Status& value) {
  if (!value.ok()) {
    LOG_IF(WARNING, detail_log_)
        << ToString() << " - Updating error to: " << value;
    absl::MutexLock l(&mutex_);
    status::UpdateOrAnnotate(last_error_, value);
  }
}
void Connection::inc_bytes_read(int64_t value) {
  count_bytes_read_.fetch_add(value);
}
void Connection::inc_bytes_written(int64_t value) {
  count_bytes_written_.fetch_add(value);
}

void Connection::CallConnectHandler() {
  if (ABSL_PREDICT_TRUE(connect_handler_ != nullptr)) {
    connect_handler_();
  } else {
    LOG_EVERY_N(WARNING, 500)
        << "Connect handler not set for connection: " << ToString();
  }
}

absl::Status Connection::CallReadHandler() {
  RET_CHECK(read_handler_ != nullptr)
      << "No read handler set for connection: " << ToString();
  return read_handler_();
}
absl::Status Connection::CallWriteHandler() {
  RET_CHECK(write_handler_ != nullptr)
      << "No write handler set for connection: " << ToString();
  return write_handler_();
}
void Connection::CallCloseHandler(const absl::Status& err,
                                  CloseDirective directive) {
  if (close_handler_ != nullptr) {
    close_handler_(err, directive);
  } else {
    LOG_IF(INFO, detail_log_) << ToString() << " - No close handler found.";
    FlushAndClose();
  }
}

TcpConnectionParams& TcpConnectionParams::set_send_buffer_size(size_t value) {
  send_buffer_size = value;
  return *this;
}
TcpConnectionParams& TcpConnectionParams::set_recv_buffer_size(size_t value) {
  recv_buffer_size = value;
  return *this;
}
TcpConnectionParams& TcpConnectionParams::set_read_limit(size_t value) {
  read_limit = value;
  return *this;
}
TcpConnectionParams& TcpConnectionParams::set_write_limit(size_t value) {
  write_limit = value;
  return *this;
}
TcpConnectionParams& TcpConnectionParams::set_block_size(size_t value) {
  block_size = value;
  return *this;
}
TcpConnectionParams& TcpConnectionParams::set_shutdown_linger_timeout(
    absl::Duration value) {
  shutdown_linger_timeout = value;
  return *this;
}
TcpConnectionParams& TcpConnectionParams::set_detail_log(bool value) {
  detail_log = value;
  return *this;
}

TcpAcceptorParams& TcpAcceptorParams::set_acceptor_threads(
    AcceptorThreads value) {
  acceptor_threads = std::move(value);
  return *this;
}
TcpAcceptorParams& TcpAcceptorParams::set_tcp_connection_params(
    TcpConnectionParams value) {
  tcp_connection_params = std::move(value);
  return *this;
}
TcpAcceptorParams& TcpAcceptorParams::set_max_backlog(size_t value) {
  max_backlog = value;
  return *this;
}
TcpAcceptorParams& TcpAcceptorParams::set_detail_log(bool value) {
  detail_log = value;
  return *this;
}

AcceptorThreads& AcceptorThreads::set_client_threads(
    std::vector<SelectorThread*> client_threads) {
  client_threads_ = std::move(client_threads);
  return *this;
}

Selector* AcceptorThreads::GetNextSelector() {
  if (ABSL_PREDICT_FALSE(client_threads_.empty())) {
    return nullptr;
  }
  return client_threads_[next_client_thread_.fetch_add(1) %
                         client_threads_.size()]
      ->selector();
}

TcpAcceptor::TcpAcceptor(Selector* selector, TcpAcceptorParams params)
    : Acceptor(),
      Selectable(ABSL_DIE_IF_NULL(selector)),
      params_(std::move(params)) {
  detail_log_ = params_.detail_log;
}
TcpAcceptor::~TcpAcceptor() {
  CHECK_EQ(state(), DISCONNECTED) << "Can only delete disconnected acceptors.";
  CHECK_EQ(fd_.load(), kInvalidFdValue);
}

const TcpAcceptor::Statistics& TcpAcceptor::stats() const { return stats_; }

absl::Status TcpAcceptor::Listen(const HostPort& local_addr) {
  RET_CHECK(fd_ == kInvalidFdValue)
      << "Attempting listening again, with valid socket: " << ToString();
  RET_CHECK(state() == DISCONNECTED)
      << "Attempting listening on non-disconnected acceptor: " << ToString();

  struct sockaddr_storage addr;
  RETURN_IF_ERROR(local_addr.ToSockAddr(&addr))
      << "Setting listening address for TCP acceptor";

  // create socket
  const int fd = ::socket(addr.ss_family, SOCK_STREAM, 0);
  if (fd_ < 0) {
    return error::ErrnoToStatus(error::Errno())
           << "::socket failed for: " << ToString();
  }
  fd_.store(fd);
  base::CallOnReturn close_fd([this]() {
    if (::close(fd_.load())) {
      LOG(WARNING) << ToString()
                   << " - ::close failed for Listen  error. "
                      "Close error: "
                   << error::ErrnoToString(error::Errno());
    }
    fd_.store(kInvalidFdValue);
  });
  RETURN_IF_ERROR(SetSocketOptions());
  if (::bind(fd_.load(), AsSockAddr(&addr), SockAddrLen(addr)) < 0) {
    return error::ErrnoToStatus(error::Errno())
           << "::bind failed for: " << ToString();
  }
  if (::listen(fd_, params_.max_backlog)) {
    return error::ErrnoToStatus(error::Errno())
           << "::listen failed for: " << ToString();
  }
  RETURN_IF_ERROR(selector()->Register(this))
      << "Registering acceptor with selector for: " << ToString();

  // Initialize local address from socket.
  // In case the user supplied port 0 we learn the system chosen port now.
  RETURN_IF_ERROR(InitializeLocalAddress());
  LOG_IF(INFO, detail_log_) << ToString() << " - Bound and listening.";
  set_state(LISTENING);
  close_fd.reset();

  // Note: Read Events are enabled by default
  return absl::OkStatus();
}
void TcpAcceptor::Close() {
  if (!selector()->IsInSelectThread()) {
    selector()->RunInSelectLoop(absl::bind_front(&TcpAcceptor::Close, this));
  } else {
    LOG_IF(INFO, detail_log_) << ToString() << " - Closing acceptor.";
    InternalClose(absl::OkStatus());
  }
}

std::string TcpAcceptor::ToString() const {
  return absl::StrCat("TcpAcceptor [ ", local_address().ToString(),
                      " state: ", state_name(), " fd: ", fd_.load(), " ]");
}

int TcpAcceptor::GetFd() const { return fd_.load(); }

bool TcpAcceptor::HandleReadEvent(SelectorEventData event) {
  CHECK(selector()->IsInSelectThread());
  // new client connection - perform ::accept
  struct sockaddr_storage addr;
  socklen_t addrlen = sizeof(addr);
  const int client_fd = ::accept(fd_.load(), AsSockAddr(&addr), &addrlen);
  if (client_fd < 0) {
    if (error::Errno() == EAGAIN || error::Errno() == EWOULDBLOCK) {
      // This could happen if the connecting client goes away just before
      // we execute "accept".
      return true;
    }
    LOG(WARNING) << ToString() << " - ::accept failed: "
                 << error::ErrnoToString(error::Errno())
                 << ". It will get closed";
    return false;  // stop accepting
  }
  base::CallOnReturn close_fd([client_fd]() { ::close(client_fd); });
  auto host_port_result =
      HostPort::ParseFromSockAddr(AsSockAddr(&addr), SockAddrLen(addr));
  if (!host_port_result.ok()) {
    LOG(WARNING) << "Cannot parse remote address from sockaddr: "
                 << host_port_result.status() << " - closing connection.";
    stats_.peer_parse_errors.fetch_add(1);
    return true;  // continue accepting
  }
  if (!CallFilterHandler(host_port_result.value())) {
    LOG_IF(INFO, detail_log_) << ToString() << " - Connection filtered out: "
                              << host_port_result.value().ToString();
    stats_.filtered_connections.fetch_add(1);
    return true;  // continue accepting
  }
  close_fd.reset();
  stats_.connections_accept_scheduled.fetch_add(1);
  LOG_IF(INFO, detail_log_)
      << ToString()
      << " - connection accepted from: " << host_port_result.value().ToString();

  Selector* const selector_to_use = params_.acceptor_threads.GetNextSelector();
  if (selector_to_use != nullptr) {
    selector_to_use->RunInSelectLoop([this, selector_to_use, client_fd]() {
      InitializeAcceptedConnection(selector_to_use, client_fd);
    });
  } else {
    InitializeAcceptedConnection(selector_, client_fd);
  }
  return true;
}
bool TcpAcceptor::HandleWriteEvent(SelectorEventData event) {
  CHECK(selector()->IsInSelectThread());
  LOG(WARNING) << ToString() << " - HandleWriteEvent called on server socket";
  return false;
}

bool TcpAcceptor::HandleErrorEvent(SelectorEventData event) {
  CHECK(selector()->IsInSelectThread());
  const int value = event.internal_event;
  if (selector()->IsAnyHangUpEvent(value)) {  // "HUP on server socket"
    LOG_IF(INFO, detail_log_)
        << ToString() << " - Hang up event received on server socket.";
    stats_.hang_ups_handled.fetch_add(1);
    return true;  // continue accepting
  }
  if (selector()->IsErrorEvent(value)) {
    const int err = ExtractSocketErrno(fd_.load());
    stats_.errors_handled.fetch_add(1);
    InternalClose(error::ErrnoToStatus(err)
                  << " - error detected on accept socket for: " << ToString());
    return false;  // closing the acceptor
  }
  return true;  // continue accepting
}

absl::Status TcpAcceptor::SetSocketOptions() {
  const int fd = fd_.load();
  RET_CHECK(fd != kInvalidFdValue);
  const int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return error::ErrnoToStatus(error::Errno())
           << "::fcntl with F_GETFL failed for: " << ToString();
  }
  const int new_flags = flags | O_NONBLOCK;
  if (::fcntl(fd, F_SETFL, new_flags)) {
    return error::ErrnoToStatus(error::Errno())
           << "::fcntl with F_SETFL, " << new_flags
           << " failed for: " << ToString();
  }
  // Enable fast bind reusing (without this option, closing the socket
  //  will switch OS port to CLOSE_WAIT state for ~1 minute, during which
  //  bind fails with EADDRINUSE.
  const int true_flag = 1;
  if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &true_flag,
                   sizeof(true_flag)) < 0) {
    return error::ErrnoToStatus(error::Errno())
           << "::setsockopt with SO_REUSEADDR failed for: " << ToString();
  }
#ifdef SO_NOSIGPIPE
  // Also disable the SIGPIPE for systems that support it (e.g. OSX & IOS)
  if (::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &true_flag,
                   sizeof(true_flag))) {
    return error::ErrnoToStatus(error::Errno())
           << "::setsockopt with SO_NOSIGPIPE failed for: " << ToString();
  }
#endif  // SO_NOSIGPIPE
  return absl::OkStatus();
}

absl::Status TcpAcceptor::InitializeLocalAddress() {
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);
  if (::getsockname(fd_.load(), AsSockAddr(&addr), &len) < 0) {
    return error::ErrnoToStatus(error::Errno())
           << "::getsockname failed for: " << ToString();
  }
  ASSIGN_OR_RETURN(auto local_address,
                   HostPort::ParseFromSockAddr(AsSockAddr(&addr), len),
                   _ << "Parsing local socket address for: " << ToString());
  absl::WriterMutexLock l(&mutex_);
  local_address_.Update(local_address);
  return absl::OkStatus();
}

void TcpAcceptor::InitializeAcceptedConnection(Selector* net_selector,
                                               int client_fd) {
  // WARNING: net_selector = is a secondary net selector
  //          selector_ = is the main media selector
  //          CallAcceptHandler() is running on the secondary net selector.
  CHECK(net_selector->IsInSelectThread());
  stats_.connections_accepted.fetch_add(1);

  // create a TcpConnection object for this client, and wrap the provided
  // file descriptor.
  auto client = absl::make_unique<TcpConnection>(net_selector,
                                                 params_.tcp_connection_params);
  auto wrap_status = client->Wrap(client_fd);
  if (!wrap_status.ok()) {
    stats_.connection_wrap_errors.fetch_add(1);
    LOG(WARNING) << "Failed to wrap incoming client fd: " << client_fd << " - "
                 << wrap_status;
    if (::close(client_fd) < 0) {
      LOG(WARNING) << ToString() << " - ::close failed on unwrapped client fd: "
                   << error::ErrnoToString(error::Errno());
    }
    return;
  }
  stats_.connections_initialized.fetch_add(1);
  // for TCP an accepted fd should be fully connected
  DCHECK_EQ(client->state(), TcpConnection::CONNECTED);
  LOG_IF(INFO, detail_log_)
      << ToString()
      << " - Incoming connection accepted: " << client->ToString();
  // deliver this new client to application
  CallAcceptHandler(std::move(client));
}

void TcpAcceptor::InternalClose(const absl::Status& status) {
  CHECK(selector()->IsInSelectThread());
  const int fd = fd_.exchange(kInvalidFdValue);
  set_last_error(status);
  if (fd == kInvalidFdValue) {
    CHECK_EQ(state(), DISCONNECTED);
    return;
  }
  LOG_IF_ERROR(WARNING, selector()->Unregister(this))
      << "Unregistering acceptor from selector: " << ToString();
  if (::close(fd) < 0) {
    LOG(WARNING) << ToString() << " - ::close failed: "
                 << error::ErrnoToString(error::Errno());
  }
  set_state(DISCONNECTED);
  CallCloseHandler(status);
}

////////////////////////////////////////////////////////////////////////////////

TcpConnection::TcpConnection(Selector* selector, TcpConnectionParams params)
    : Connection(ABSL_DIE_IF_NULL(selector)),
      Selectable(ABSL_DIE_IF_NULL(selector)),
      params_(std::move(params)),
      timeouter_(selector,
                 absl::bind_front(&TcpConnection::HandleTimeoutEvent, this)) {
  detail_log_ = params_.detail_log;
}

TcpConnection::~TcpConnection() {
  CHECK_EQ(state(), DISCONNECTED)
      << "Can only delete disconnected connections.";
  CHECK_EQ(fd_.load(), kInvalidFdValue);
}

absl::Status TcpConnection::Wrap(int fd) {
  CHECK(selector()->IsInSelectThread());
  RET_CHECK(fd_.load() == kInvalidFdValue)
      << "Should wrap only on unconnected connection.";
  fd_.store(fd);
  base::CallOnReturn close_fd([this]() { fd_.store(kInvalidFdValue); });
  RETURN_IF_ERROR(SetSocketOptions());
  RETURN_IF_ERROR(selector()->Register(this));
  RETURN_IF_ERROR(InitializeLocalAddress());
  RETURN_IF_ERROR(InitializeRemoteAddress());
  RETURN_IF_ERROR(RequestReadEvents(true));
  close_fd.reset();

  set_read_closed(false);
  set_write_closed(false);
  set_state(TcpConnection::CONNECTED);
  return absl::OkStatus();
}

absl::Status TcpConnection::Connect(const HostPort& remote_addr) {
  CHECK(selector()->IsInSelectThread());
  RET_CHECK(state() == DISCONNECTED || state() == RESOLVING)
      << "Illegal state: " << state_name();
  RET_CHECK(fd_.load() == kInvalidFdValue) << "Connection fd already created";
  if (!remote_addr.port().has_value()) {
    return status::InvalidArgumentErrorBuilder()
           << "Hostport for TCP connection has no port specified: "
           << remote_addr.ToString();
  }
  // maybe start DNS resolve
  if (state() == DISCONNECTED && !remote_addr.IsResolved()) {
    if (!remote_addr.host().has_value()) {
      return status::InvalidArgumentErrorBuilder()
             << "Hostport for TCP connection has no host or ip specified: "
             << remote_addr.ToString();
    }
    {
      absl::WriterMutexLock l(&mutex_);
      remote_address_ = remote_addr;
    }
    LOG_IF(INFO, detail_log_) << ToString() << " - Starting DNS resolve.";
    set_state(RESOLVING);
    DnsResolver::Default().ResolveAsync(
        remote_addr.host().value(),
        absl::bind_front(&TcpConnection::HandleDnsResult, this));
    return absl::OkStatus();  // for now
  }

  struct sockaddr_storage addr;
  RETURN_IF_ERROR(remote_addr.ToSockAddr(&addr))
      << "Setting listening address for TCP connection.";

  const int fd = ::socket(addr.ss_family, SOCK_STREAM, 0);
  if (fd_ < 0) {
    return error::ErrnoToStatus(error::Errno())
           << "::socket failed for connecting to: " << remote_addr.ToString();
  }
  fd_.store(fd);
  base::CallOnReturn close_fd([this]() {
    if (::close(fd_.load())) {
      LOG(WARNING) << ToString()
                   << " - ::close failed for Connect error. "
                      "Close error: "
                   << error::ErrnoToString(error::Errno());
    }
    fd_.store(kInvalidFdValue);
  });
  RETURN_IF_ERROR(SetSocketOptions());
  RETURN_IF_ERROR(selector()->Register(this));
  close_fd.reset();

  // We are all set to begin connecting - if we fail here we will advance the
  // state accordingly, and close the socket in different ways - through proper
  // error handling.
  {
    absl::WriterMutexLock l(&mutex_);
    remote_address_ = remote_addr;
  }
  set_state(CONNECTING);
  set_read_closed(false);
  set_write_closed(false);

  if (::connect(fd_.load(), AsSockAddr(&addr), SockAddrLen(addr)) < 0) {
    if (error::Errno() != EINPROGRESS) {
      LOG_IF(INFO, detail_log_)
          << ToString()
          << " - Error in connect: " << error::ErrnoToString(error::Errno());
      return error::ErrnoToStatus(error::Errno())
             << "::connect failed for: " << ToString();
    }
    // For EINPROGRESS we need to wait for actually connecting.
  }  // else connect already completed, but to simplify logic, we wait for the
  // first HandleRead/Write and call InvokeConnectHandler there - see
  // PerformConnectOnFirstOperation.
  RETURN_IF_ERROR(RequestWriteEvents(true));
  RETURN_IF_ERROR(RequestReadEvents(true));

  LOG_IF(INFO, detail_log_) << ToString() << " - Connecting";
  return absl::OkStatus();
}

void TcpConnection::FlushAndClose() {
  if (!selector()->IsInSelectThread()) {
    selector()->RunInSelectLoop(
        absl::bind_front(&TcpConnection::FlushAndClose, this));
  } else {
    LOG_IF(INFO, detail_log_) << ToString() << " - Flush and close.";
    CloseCommunication(CLOSE_WRITE);
  }
}
void TcpConnection::ForceClose() {
  if (!selector()->IsInSelectThread()) {
    selector()->RunInSelectLoop(
        absl::bind_front(&TcpConnection::ForceClose, this));
  } else {
    LOG_IF(INFO, detail_log_) << ToString() << " - Force close.";
    // Force the close without any error.
    InternalClose(absl::OkStatus(), true);
  }
}
absl::Status TcpConnection::SetSendBufferSize(int size) {
  if (::setsockopt(fd_, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size))) {
    return error::ErrnoToStatus(error::Errno())
           << "::Setting send buffer size of: " << size
           << " for: " << ToString();
  }
  return absl::OkStatus();
}
absl::Status TcpConnection::SetRecvBufferSize(int size) {
  if (::setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size))) {
    return error::ErrnoToStatus(error::Errno())
           << "::Setting recv buffer size of: " << size
           << " for: " << ToString();
  }
  return absl::OkStatus();
}
absl::Status TcpConnection::RequestReadEvents(bool enable) {
  return selector()->EnableReadCallback(this, enable);
}
absl::Status TcpConnection::RequestWriteEvents(bool enable) {
  return selector()->EnableWriteCallback(this, enable);
}
HostPort TcpConnection::GetLocalAddress() const {
  absl::MutexLock l(&mutex_);
  return local_address_;
}
HostPort TcpConnection::GetRemoteAddress() const {
  absl::ReaderMutexLock l(&mutex_);
  return remote_address_;
}
std::string TcpConnection::ToString() const {
  return absl::StrCat(
      "TcpConnection [ ", GetLocalAddress().ToString(), " => ",
      GetRemoteAddress().ToString(), " (fd: ", fd_.load(),
      ", state: ", state_name(), ", last read: ",
      absl::FormatTime(absl::FromUnixNanos(last_read_ts_.load())),
      ", last write: ",
      absl::FormatTime(absl::FromUnixNanos(last_write_ts_.load())), ") ]");
}

//////////////////////////////////////////////////////////////////////

int TcpConnection::GetFd() const { return fd_.load(); }
bool TcpConnection::HandleReadEvent(SelectorEventData event) {
  CHECK(selector()->IsInSelectThread());
  CHECK(state() != DISCONNECTED) << "Invalid state: " << state_name();
  if (state() == CONNECTING) {
    return PerformConnectOnFirstOperation();
  }
  CHECK(state() == CONNECTED || state() == FLUSHING)
      << "Illegal state during read: " << state_name();
  auto read_result = PerformRead();
  if (!read_result.ok()) {
    InternalClose(read_result.status(), true);
    return false;
  }
  const ssize_t cb = read_result.value();
  // Call application level data processing for a non-zero read.
  if (cb > 0) {
    auto read_handler_status = CallReadHandler();
    if (ABSL_PREDICT_FALSE(!read_handler_status.ok())) {
      InternalClose(read_handler_status, true);
      return false;
    }
  }
  if (write_closed() || state() == FLUSHING || IsProperError(error::Errno())) {
    // Previous read returned 0 bytes, READ half closed.
    set_read_closed(true);
  }
  if (read_closed()) {
    CallCloseHandler(absl::OkStatus(), CLOSE_READ);
    if (fd_.load() != kInvalidFdValue) {
      // we need this because (E)POLLIN continuously fires
      auto read_enable_status = RequestReadEvents(false);
      if (ABSL_PREDICT_FALSE(!read_enable_status.ok())) {
        InternalClose(read_enable_status, true);
        return false;
      }
      // TODO(cpopescu): may remove remove
      //                 - application should close WRITE anyway.
      // Close(CLOSE_WRITE);
    }
  }
  return true;
}

bool TcpConnection::HandleWriteEvent(SelectorEventData event) {
  CHECK(selector()->IsInSelectThread());
  CHECK(state() != DISCONNECTED) << "Invalid state: " << state_name();
  if (state() == CONNECTING) {
    return PerformConnectOnFirstOperation();
  }
  CHECK(state() == CONNECTED || state() == FLUSHING)
      << "Illegal state during write: " << state_name();

  auto write_result = Selectable::WriteCord(*outbuf(), params_.write_limit);
  if (!write_result.ok()) {
    InternalClose(write_result.status(), true);
    return false;
  }
  const ssize_t cb = write_result.value();
  outbuf()->RemovePrefix(cb);
  inc_bytes_written(cb);
  last_write_ts_.store(absl::ToUnixNanos(selector()->now()));

  // Call application level data write processing.
  if (state() != FLUSHING) {
    auto write_handler_status = CallWriteHandler();
    if (ABSL_PREDICT_FALSE(!write_handler_status.ok())) {
      InternalClose(write_handler_status, true);
      return false;
    }
  }
  if (!outbuf()->empty()) {
    return true;  // Continue writing & the connection - we have more data.
  }
  // Stop write events for now.
  auto write_request_status = RequestWriteEvents(false);
  if (ABSL_PREDICT_FALSE(!write_request_status.ok())) {
    InternalClose(write_request_status, true);
    return false;
  }
  if (state() != FLUSHING) {
    return true;  //  Normal operation - continue the connection.
  }
  // We are in FLUSHING, and we finished sending all buffered data.
  // Execute ::shutdown write half.
  if (::shutdown(fd_, SHUT_WR) < 0) {
    InternalClose(error::ErrnoToStatus(error::Errno())
                      << " - ::shutdown after flush failed for: " << ToString(),
                  true);
    return false;
  }
  set_write_closed(true);
  // We closed the write half, the peer is notified by RDHUP.
  // Now, we wait for it to close the connection too, and when it does, we get
  // a HUP. In case of linger_timeout happens, we force close the connection.
  timeouter_.SetTimeout(kShutdownTimeoutId, params_.shutdown_linger_timeout);
  return true;
}

bool TcpConnection::HandleErrorEvent(SelectorEventData event) {
  CHECK(selector()->IsInSelectThread());
  CHECK_NE(state(), DISCONNECTED);
  const int value = event.internal_event;

  // Possible error events, according to epoll_ctl(2) manual page:
  // ("events" is a combination of one or more of these)
  //
  // EPOLLRDHUP  Stream socket peer closed connection, or shut down
  //             writing half of connection. (This flag is especially useful
  //             for writing simple code to detect peer shutdown when using
  //             Edge Triggered monitoring.)
  //
  // EPOLLERR    Error condition happened on the associated file descriptor.
  //
  // EPOLLHUP    Hang up happened on the associated file descriptor.
  //
  // Note: Similar for poll.
  if (selector()->IsErrorEvent(value)) {
    const int err = ExtractSocketErrno(fd_.load());
    InternalClose(absl::Status(error::ErrnoToStatus(err)
                               << " - error detected on connection socket for: "
                               << ToString()),
                  true);
    return false;
  }

  // IMPORTANT:
  // The chain of events on a connected socket:
  //        A                            B
  //    -----------                 -----------
  // executes:
  // a) close fd, or
  // b) shutdown write
  //     ========================>
  //                               receives RDHUP, and executes
  //                               c) close fd, or
  //                               d) shutdown write
  //     <========================
  // a) nothing happens
  // b) receives HUP,
  //    and executes close fd
  //     ========================>
  //                               c) nothing happens
  //                               d) receives HUP,
  //                                  and executes close fd

  if (selector()->IsHangUpEvent(value)) {
    // peer completely closed the connection
    set_write_closed(true);
    if (state() != CONNECTING && selector()->IsInputEvent(value)) {
      // don't close here, let the next HandleReadEvent read pending data.
      // EPOLLHUP is continuously generated.
      LOG_IF(INFO, detail_log_)
          << ToString() << " - HUP detected - continuing on more input";
      return true;
    }
    LOG_IF(INFO, detail_log_) << ToString() << " - HUP detected - stopping";
    InternalClose(absl::OkStatus(), true);
    return false;
  }
  if (selector()->IsRemoteHangUpEvent(value)) {
    set_state(FLUSHING);
    if (state() != CONNECTING && selector()->IsInputEvent(value)) {
      LOG_IF(INFO, detail_log_)
          << ToString() << " - Remote HUP detected - continuing on more input";
      // peer closed write half of the connection there may be pending data on
      // read. So wait until recv() returns 0, then set read_closed_ = true;
      return true;
    }
    LOG_IF(INFO, detail_log_)
        << ToString() << " - Remote HUP detected - stopping";
    // no (E)POLLIN means READ disabled - Peer closed on us, just close.
    InternalClose(absl::OkStatus(), true);
    return false;
  }
  return true;
}

void TcpConnection::Close() {
  // this call comes from Selectable interface
  LOG_IF(INFO, detail_log_) << ToString() << " - External close requested.";
  InternalClose(absl::OkStatus(), true);
}

void TcpConnection::CloseCommunication(CloseDirective directive) {
  if (fd_.load() == kInvalidFdValue) {
    CHECK_EQ(state(), DISCONNECTED);
    return;
  }
  LOG_IF(INFO, detail_log_)
      << ToString()
      << " - Close communication: " << CloseDirectiveName(directive);
  if (!selector()->IsInSelectThread()) {
    selector()->RunInSelectLoop(
        absl::bind_front(&TcpConnection::CloseCommunication, this, directive));
    return;
  }
  // Ignore CLOSE_READ, we don't need to treat it.
  // If CLOSE_WRITE is requested and writing is not closed => go to FLUSHING.
  if ((directive == CLOSE_WRITE || directive == CLOSE_READ_WRITE) &&
      !write_closed() && state() == CONNECTED) {
    set_state(FLUSHING);
    LOG_IF_ERROR(WARNING, RequestWriteEvents(true));
    // NOTE: when outbuf_ gets empty we execute ::shutdown(write)
    //       and set write_closed_ to true
  }
}

absl::Status TcpConnection::SetSocketOptions() {
  const int fd = fd_.load();
  RET_CHECK(fd != kInvalidFdValue);
  const int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return error::ErrnoToStatus(error::Errno())
           << "::fcntl with F_GETFL failed for: " << ToString();
  }
  const int new_flags = flags | O_NONBLOCK;
  if (::fcntl(fd, F_SETFL, new_flags)) {
    return error::ErrnoToStatus(error::Errno())
           << "::fcntl with F_SETFL, " << new_flags
           << " failed for: " << ToString();
  }
  // disable Nagel buffering algorithm:
  const int true_flag = 1;
  if (::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &true_flag,
                   sizeof(true_flag)) < 0) {
    return error::ErrnoToStatus(error::Errno())
           << "::setsockopt with TCP_NODELAY failed for: " << ToString();
  }
#ifdef SO_NOSIGPIPE
  // Also disable the SIGPIPE for systems that support it (e.g. OSX & IOS)
  if (::setsockopt(fd_, IPPROTO_TCP, SO_NOSIGPIPE, &true_flag,
                   sizeof(true_flag)) < 0) {
    return error::ErrnoToStatus(error::Errno())
           << "::setsockopt with SO_NOSIGPIPE failed for: " << ToString();
  }
#endif  // SO_NOSIGPIPE
  // set tcp buffering parameters:
  if (params_.send_buffer_size.has_value()) {
    RETURN_IF_ERROR(SetSendBufferSize(params_.send_buffer_size.value()));
  }
  if (params_.recv_buffer_size.has_value()) {
    RETURN_IF_ERROR(SetRecvBufferSize(params_.recv_buffer_size.value()));
  }
  return absl::OkStatus();
}

absl::Status TcpConnection::InitializeLocalAddress() {
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);
  if (::getsockname(fd_.load(), AsSockAddr(&addr), &len) < 0) {
    return error::ErrnoToStatus(error::Errno())
           << "::getsockname failed for: " << ToString();
  }
  ASSIGN_OR_RETURN(auto local_address,
                   HostPort::ParseFromSockAddr(AsSockAddr(&addr), len),
                   _ << "Parsing local socket address for: " << ToString());
  absl::MutexLock l(&mutex_);
  local_address_.Update(local_address);
  return absl::OkStatus();
}

absl::Status TcpConnection::InitializeRemoteAddress() {
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);
  if (!::getpeername(fd_.load(), AsSockAddr(&addr), &len)) {
    return error::ErrnoToStatus(error::Errno())
           << "::getpeername failed for: " << ToString();
  }
  ASSIGN_OR_RETURN(auto remote_address,
                   HostPort::ParseFromSockAddr(AsSockAddr(&addr), len),
                   _ << "Parsing local socket address for: " << ToString());
  absl::WriterMutexLock l(&mutex_);
  remote_address_.Update(remote_address);
  return absl::OkStatus();
}

void TcpConnection::InternalClose(const absl::Status& status,
                                  bool call_close_handler) {
  if (state() == DISCONNECTED) {
    CHECK_EQ(fd_.load(), kInvalidFdValue);
    return;
  }
  CHECK(selector()->IsInSelectThread());
  set_last_error(status);
  if (state() == RESOLVING) {
    LOG_IF(INFO, detail_log_)
        << ToString() << " - Internal close delayed per resolve state.";
    // will get closed when dns resolve completes.
    close_on_resolve_ = call_close_handler;
    return;
  }
  if (fd_.load() != kInvalidFdValue) {
    LOG_IF_ERROR(WARNING, selector()->Unregister(this))
        << "Unregistering connection from selector: " << ToString();
    if (ABSL_PREDICT_FALSE(::shutdown(fd_, SHUT_RDWR) < 0)) {
      LOG(WARNING) << ToString() << " - ::shutdown failed: "
                   << error::ErrnoToString(error::Errno());
    }
    if (::close(fd_) < 0) {
      LOG(WARNING) << ToString() << " - ::close failed: "
                   << error::ErrnoToString(error::Errno());
    }
    fd_.store(kInvalidFdValue);
  }
  set_state(DISCONNECTED);
  set_read_closed(true);
  set_write_closed(true);
  timeouter_.ClearAllTimeouts();
  LOG_IF(WARNING, ABSL_PREDICT_FALSE(!inbuf()->empty()))
      << "Connection: " << ToString()
      << " is closed w/o all in bytes read: " << inbuf()->size();
  LOG_IF(WARNING, ABSL_PREDICT_FALSE(!outbuf()->empty()))
      << "Connection: " << ToString()
      << " is closed w/o all out bytes written: " << outbuf()->size();
  inbuf()->Clear();
  outbuf()->Clear();
  if (call_close_handler) {
    CallCloseHandler(status, CLOSE_READ_WRITE);
  }
}

void TcpConnection::HandleTimeoutEvent(int64_t timeout_id) {
  LOG_IF(WARNING, ABSL_PREDICT_FALSE(timeout_id != kShutdownTimeoutId))
      << "Unknown timeout_id received by " << ToString() << ": " << timeout_id;
  InternalClose(absl::OkStatus(), true);
}

void TcpConnection::HandleDnsResult(
    absl::StatusOr<std::shared_ptr<DnsHostInfo>> info) {
  if (!selector()->IsInSelectThread()) {
    selector()->RunInSelectLoop([this, &info]() { HandleDnsResult(info); });
    return;
  }
  CHECK(state() == RESOLVING);
  if (close_on_resolve_.has_value()) {
    LOG_IF(INFO, detail_log_)
        << ToString() << " - Resolve completed, but closed in the meantime.";
    // A close was ordered upon dns resolve completion - fulfill it now:
    InternalClose(last_error(), close_on_resolve_.value());
    return;
  }
  absl::Status status = info.status();
  if (status.ok()) {
    auto ip = info.value()->PickNextAddress();
    if (ABSL_PREDICT_FALSE(ip.has_value())) {
      status = status::InternalErrorBuilder()
               << "No valid IP address was resolved for " << ToString();
    } else {
      net::HostPort connect_addr;
      {
        absl::WriterMutexLock l(&mutex_);
        remote_address_.set_ip(ip.value());
        connect_addr = remote_address_;
      }
      LOG_IF(INFO, detail_log_) << ToString() << " - Resolve completed OK.";
      status = Connect(connect_addr);
    }
  }
  if (ABSL_PREDICT_FALSE(!status.ok())) {
    InternalClose(status, true);
  }
}

bool TcpConnection::PerformConnectOnFirstOperation() {
  set_state(CONNECTED);
  LOG_IF_ERROR(WARNING, InitializeLocalAddress())
      << "Initializing local address while becoming connected on read.";
  // Read and write events should be enabled. Now call the application
  // handler for the connected event.
  CallConnectHandler();
  // Either the application closed the connection in "ConnectHandler"
  // or the connection continues in the CONNECTED state
  CHECK(state() == CONNECTED || state() == DISCONNECTED || state() == FLUSHING)
      << "Application changed the status to an invalid state: " << state_name();
  LOG_IF(INFO, detail_log_) << ToString() << " - Connected.";
  return state() == CONNECTED;
}

absl::StatusOr<ssize_t> TcpConnection::PerformRead() {
  int count = 0;
  if (ABSL_PREDICT_FALSE(::ioctl(fd_.load(), FIONREAD, &count) < 0)) {
    return error::ErrnoToStatus(error::Errno())
           << " - performing ::ioctl w/ FIONREAD for: " << ToString();
  }
  if (count <= 0) {
    return absl::OkStatus();  // nothing to read.
  }
  if (params_.read_limit.has_value() &&
      size_t(count) > params_.read_limit.value()) {
    count = params_.read_limit.value();
  }
  ASSIGN_OR_RETURN(const ssize_t cb, Selectable::ReadToCord(inbuf(), count),
                   _ << "Reading from input socket for: " << ToString());
  inc_bytes_read(cb);
  last_read_ts_.store(absl::ToUnixNanos(selector()->now()));
  return cb;
}

void TcpConnection::CallCloseHandler(const absl::Status& status,
                                     CloseDirective directive) {
  // When calling the close handle for read, the read closed flag must be on.
  CHECK(read_closed() ||
        (directive != CLOSE_READ && directive != CLOSE_READ_WRITE));
  // When calling the close handle for write, the write closed flag must be on.
  CHECK(write_closed() ||
        (directive != CLOSE_WRITE && directive != CLOSE_READ_WRITE));
  Connection::CallCloseHandler(status, directive);
}

}  // namespace net
}  // namespace whisper
