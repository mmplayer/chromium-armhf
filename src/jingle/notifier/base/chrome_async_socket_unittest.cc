// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/base/chrome_async_socket.h"

#if defined(OS_WIN)
#include <winsock2.h>
#elif defined(OS_POSIX)
#include <arpa/inet.h>
#endif

#include <deque>
#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "jingle/notifier/base/resolving_client_socket_factory.h"
#include "net/base/cert_verifier.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/url_request/url_request_context_getter.h"
#include "talk/base/sigslot.h"
#include "talk/base/socketaddress.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifier {

namespace {

// Data provider that handles reads/writes for ChromeAsyncSocket.
class AsyncSocketDataProvider : public net::SocketDataProvider {
 public:
  AsyncSocketDataProvider() : has_pending_read_(false) {}

  virtual ~AsyncSocketDataProvider() {
    EXPECT_TRUE(writes_.empty());
    EXPECT_TRUE(reads_.empty());
  }

  // If there's no read, sets the "has pending read" flag.  Otherwise,
  // pops the next read.
  virtual net::MockRead GetNextRead() {
    if (reads_.empty()) {
      DCHECK(!has_pending_read_);
      has_pending_read_ = true;
      const net::MockRead pending_read(false, net::ERR_IO_PENDING);
      return pending_read;
    }
    net::MockRead mock_read = reads_.front();
    reads_.pop_front();
    return mock_read;
  }

  // Simply pops the next write and, if applicable, compares it to
  // |data|.
  virtual net::MockWriteResult OnWrite(const std::string& data) {
    DCHECK(!writes_.empty());
    net::MockWrite mock_write = writes_.front();
    writes_.pop_front();
    if (mock_write.result != net::OK) {
      return net::MockWriteResult(mock_write.async, mock_write.result);
    }
    std::string expected_data(mock_write.data, mock_write.data_len);
    EXPECT_EQ(expected_data, data);
    if (expected_data != data) {
      return net::MockWriteResult(false, net::ERR_UNEXPECTED);
    }
    return net::MockWriteResult(mock_write.async, data.size());
  }

  // We ignore resets so we can pre-load the socket data provider with
  // read/write events.
  virtual void Reset() {}

  // If there is a pending read, completes it with the given read.
  // Otherwise, queues up the given read.
  void AddRead(const net::MockRead& mock_read) {
    DCHECK_NE(mock_read.result, net::ERR_IO_PENDING);
    if (has_pending_read_) {
      socket()->OnReadComplete(mock_read);
      has_pending_read_ = false;
      return;
    }
    reads_.push_back(mock_read);
  }

  // Simply queues up the given write.
  void AddWrite(const net::MockWrite& mock_write) {
    writes_.push_back(mock_write);
  }

 private:
  std::deque<net::MockRead> reads_;
  bool has_pending_read_;

  std::deque<net::MockWrite> writes_;

  DISALLOW_COPY_AND_ASSIGN(AsyncSocketDataProvider);
};

// Takes a 32-bit integer in host byte order and converts it to a
// net::IPAddressNumber.
net::IPAddressNumber Uint32ToIPAddressNumber(uint32 ip) {
  uint32 ip_nbo = htonl(ip);
  const unsigned char* const ip_start =
      reinterpret_cast<const unsigned char*>(&ip_nbo);
  return net::IPAddressNumber(ip_start, ip_start + (sizeof ip_nbo));
}

net::AddressList SocketAddressToAddressList(
    const talk_base::SocketAddress& address) {
  DCHECK_NE(address.ip(), 0U);
  return net::AddressList::CreateFromIPAddress(
      Uint32ToIPAddressNumber(address.ip()), address.port());
}

class MockXmppClientSocketFactory : public ResolvingClientSocketFactory {
 public:
  MockXmppClientSocketFactory(
      net::ClientSocketFactory* mock_client_socket_factory,
      const net::AddressList& address_list)
          : mock_client_socket_factory_(mock_client_socket_factory),
            address_list_(address_list) {
  }

  // ResolvingClientSocketFactory implementation.
  virtual net::StreamSocket* CreateTransportClientSocket(
      const net::HostPortPair& host_and_port) {
    return mock_client_socket_factory_->CreateTransportClientSocket(
        address_list_, NULL, net::NetLog::Source());
  }

  virtual net::SSLClientSocket* CreateSSLClientSocket(
      net::ClientSocketHandle* transport_socket,
      const net::HostPortPair& host_and_port) {
    net::SSLClientSocketContext context;
    context.cert_verifier = &cert_verifier_;
    return mock_client_socket_factory_->CreateSSLClientSocket(
        transport_socket, host_and_port, ssl_config_, NULL, context);
  }

 private:
  scoped_ptr<net::ClientSocketFactory> mock_client_socket_factory_;
  net::AddressList address_list_;
  net::SSLConfig ssl_config_;
  net::CertVerifier cert_verifier_;
};

class ChromeAsyncSocketTest
    : public testing::Test,
      public sigslot::has_slots<> {
 protected:
  ChromeAsyncSocketTest()
      : ssl_socket_data_provider_(true, net::OK),
        addr_(0xaabbccdd, 35) {}

  virtual ~ChromeAsyncSocketTest() {}

  virtual void SetUp() {
    scoped_ptr<net::MockClientSocketFactory> mock_client_socket_factory(
        new net::MockClientSocketFactory());
    mock_client_socket_factory->AddSocketDataProvider(
        &async_socket_data_provider_);
    mock_client_socket_factory->AddSSLSocketDataProvider(
        &ssl_socket_data_provider_);

    scoped_ptr<MockXmppClientSocketFactory> mock_xmpp_client_socket_factory(
        new MockXmppClientSocketFactory(
            mock_client_socket_factory.release(),
            SocketAddressToAddressList(addr_)));
    chrome_async_socket_.reset(
        new ChromeAsyncSocket(mock_xmpp_client_socket_factory.release(),
                              14, 20)),

    chrome_async_socket_->SignalConnected.connect(
        this, &ChromeAsyncSocketTest::OnConnect);
    chrome_async_socket_->SignalSSLConnected.connect(
        this, &ChromeAsyncSocketTest::OnSSLConnect);
    chrome_async_socket_->SignalClosed.connect(
        this, &ChromeAsyncSocketTest::OnClose);
    chrome_async_socket_->SignalRead.connect(
        this, &ChromeAsyncSocketTest::OnRead);
    chrome_async_socket_->SignalError.connect(
        this, &ChromeAsyncSocketTest::OnError);
  }

  virtual void TearDown() {
    // Run any tasks that we forgot to pump.
    message_loop_.RunAllPending();
    ExpectClosed();
    ExpectNoSignal();
    chrome_async_socket_.reset();
  }

  enum Signal {
    SIGNAL_CONNECT,
    SIGNAL_SSL_CONNECT,
    SIGNAL_CLOSE,
    SIGNAL_READ,
    SIGNAL_ERROR,
  };

  // Helper struct that records the state at the time of a signal.

  struct SignalSocketState {
    SignalSocketState()
        : signal(SIGNAL_ERROR),
          state(ChromeAsyncSocket::STATE_CLOSED),
          error(ChromeAsyncSocket::ERROR_NONE),
          net_error(net::OK) {}

    SignalSocketState(
        Signal signal,
        ChromeAsyncSocket::State state,
        ChromeAsyncSocket::Error error,
        net::Error net_error)
        : signal(signal),
          state(state),
          error(error),
          net_error(net_error) {}

    bool IsEqual(const SignalSocketState& other) const {
      return
          (signal == other.signal) &&
          (state == other.state) &&
          (error == other.error) &&
          (net_error == other.net_error);
    }

    static SignalSocketState FromAsyncSocket(
        Signal signal,
        buzz::AsyncSocket* async_socket) {
      return SignalSocketState(signal,
                               async_socket->state(),
                               async_socket->error(),
                               static_cast<net::Error>(
                                   async_socket->GetError()));
    }

    static SignalSocketState NoError(
        Signal signal, buzz::AsyncSocket::State state) {
        return SignalSocketState(signal, state,
                                 buzz::AsyncSocket::ERROR_NONE,
                                 net::OK);
    }

    Signal signal;
    ChromeAsyncSocket::State state;
    ChromeAsyncSocket::Error error;
    net::Error net_error;
  };

  void CaptureSocketState(Signal signal) {
    signal_socket_states_.push_back(
        SignalSocketState::FromAsyncSocket(
            signal, chrome_async_socket_.get()));
  }

  void OnConnect() {
    CaptureSocketState(SIGNAL_CONNECT);
  }

  void OnSSLConnect() {
    CaptureSocketState(SIGNAL_SSL_CONNECT);
  }

  void OnClose() {
    CaptureSocketState(SIGNAL_CLOSE);
  }

  void OnRead() {
    CaptureSocketState(SIGNAL_READ);
  }

  void OnError() {
    ADD_FAILURE();
  }

  // State expect functions.

  void ExpectState(ChromeAsyncSocket::State state,
                   ChromeAsyncSocket::Error error,
                   net::Error net_error) {
    EXPECT_EQ(state, chrome_async_socket_->state());
    EXPECT_EQ(error, chrome_async_socket_->error());
    EXPECT_EQ(net_error, chrome_async_socket_->GetError());
  }

  void ExpectNonErrorState(ChromeAsyncSocket::State state) {
    ExpectState(state, ChromeAsyncSocket::ERROR_NONE, net::OK);
  }

  void ExpectErrorState(ChromeAsyncSocket::State state,
                        ChromeAsyncSocket::Error error) {
    ExpectState(state, error, net::OK);
  }

  void ExpectClosed() {
    ExpectNonErrorState(ChromeAsyncSocket::STATE_CLOSED);
  }

  // Signal expect functions.

  void ExpectNoSignal() {
    if (!signal_socket_states_.empty()) {
      ADD_FAILURE() << signal_socket_states_.front().signal;
    }
  }

  void ExpectSignalSocketState(
      SignalSocketState expected_signal_socket_state) {
    if (signal_socket_states_.empty()) {
      ADD_FAILURE() << expected_signal_socket_state.signal;
      return;
    }
    EXPECT_TRUE(expected_signal_socket_state.IsEqual(
        signal_socket_states_.front()))
        << signal_socket_states_.front().signal;
    signal_socket_states_.pop_front();
  }

  void ExpectReadSignal() {
    ExpectSignalSocketState(
        SignalSocketState::NoError(
            SIGNAL_READ, ChromeAsyncSocket::STATE_OPEN));
  }

  void ExpectSSLConnectSignal() {
    ExpectSignalSocketState(
        SignalSocketState::NoError(SIGNAL_SSL_CONNECT,
                                   ChromeAsyncSocket::STATE_TLS_OPEN));
  }

  void ExpectSSLReadSignal() {
    ExpectSignalSocketState(
        SignalSocketState::NoError(
            SIGNAL_READ, ChromeAsyncSocket::STATE_TLS_OPEN));
  }

  // Open/close utility functions.

  void DoOpenClosed() {
    ExpectClosed();
    async_socket_data_provider_.set_connect_data(
        net::MockConnect(false, net::OK));
    EXPECT_TRUE(chrome_async_socket_->Connect(addr_));
    ExpectNonErrorState(ChromeAsyncSocket::STATE_CONNECTING);

    message_loop_.RunAllPending();
    // We may not necessarily be open; may have been other events
    // queued up.
    ExpectSignalSocketState(
        SignalSocketState::NoError(
            SIGNAL_CONNECT, ChromeAsyncSocket::STATE_OPEN));
  }

  void DoCloseOpened(SignalSocketState expected_signal_socket_state) {
    // We may be in an error state, so just compare state().
    EXPECT_EQ(ChromeAsyncSocket::STATE_OPEN, chrome_async_socket_->state());
    EXPECT_TRUE(chrome_async_socket_->Close());
    ExpectSignalSocketState(expected_signal_socket_state);
    ExpectNonErrorState(ChromeAsyncSocket::STATE_CLOSED);
  }

  void DoCloseOpenedNoError() {
    DoCloseOpened(
        SignalSocketState::NoError(
            SIGNAL_CLOSE, ChromeAsyncSocket::STATE_CLOSED));
  }

  void DoSSLOpenClosed() {
    const char kDummyData[] = "dummy_data";
    async_socket_data_provider_.AddRead(net::MockRead(kDummyData));
    DoOpenClosed();
    ExpectReadSignal();
    EXPECT_EQ(kDummyData, DrainRead(1));

    EXPECT_TRUE(chrome_async_socket_->StartTls("fakedomain.com"));
    message_loop_.RunAllPending();
    ExpectSSLConnectSignal();
    ExpectNoSignal();
    ExpectNonErrorState(ChromeAsyncSocket::STATE_TLS_OPEN);
  }

  void DoSSLCloseOpened(SignalSocketState expected_signal_socket_state) {
    // We may be in an error state, so just compare state().
    EXPECT_EQ(ChromeAsyncSocket::STATE_TLS_OPEN,
              chrome_async_socket_->state());
    EXPECT_TRUE(chrome_async_socket_->Close());
    ExpectSignalSocketState(expected_signal_socket_state);
    ExpectNonErrorState(ChromeAsyncSocket::STATE_CLOSED);
  }

  void DoSSLCloseOpenedNoError() {
    DoSSLCloseOpened(
        SignalSocketState::NoError(
            SIGNAL_CLOSE, ChromeAsyncSocket::STATE_CLOSED));
  }

  // Read utility fucntions.

  std::string DrainRead(size_t buf_size) {
    std::string read;
    scoped_array<char> buf(new char[buf_size]);
    size_t len_read;
    while (true) {
      bool success =
          chrome_async_socket_->Read(buf.get(), buf_size, &len_read);
      if (!success) {
        ADD_FAILURE();
        break;
      }
      if (len_read == 0U) {
        break;
      }
      read.append(buf.get(), len_read);
    }
    return read;
  }

  // ChromeAsyncSocket expects a message loop.
  MessageLoop message_loop_;

  AsyncSocketDataProvider async_socket_data_provider_;
  net::SSLSocketDataProvider ssl_socket_data_provider_;

  scoped_ptr<ChromeAsyncSocket> chrome_async_socket_;
  std::deque<SignalSocketState> signal_socket_states_;
  const talk_base::SocketAddress addr_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeAsyncSocketTest);
};

TEST_F(ChromeAsyncSocketTest, InitialState) {
  ExpectClosed();
  ExpectNoSignal();
}

TEST_F(ChromeAsyncSocketTest, EmptyClose) {
  ExpectClosed();
  EXPECT_TRUE(chrome_async_socket_->Close());
  ExpectClosed();
}

TEST_F(ChromeAsyncSocketTest, ImmediateConnectAndClose) {
  DoOpenClosed();

  ExpectNonErrorState(ChromeAsyncSocket::STATE_OPEN);

  DoCloseOpenedNoError();
}

// After this, no need to test immediate successful connect and
// Close() so thoroughly.

TEST_F(ChromeAsyncSocketTest, DoubleClose) {
  DoOpenClosed();

  EXPECT_TRUE(chrome_async_socket_->Close());
  ExpectClosed();
  ExpectSignalSocketState(
      SignalSocketState::NoError(
          SIGNAL_CLOSE, ChromeAsyncSocket::STATE_CLOSED));

  EXPECT_TRUE(chrome_async_socket_->Close());
  ExpectClosed();
}

TEST_F(ChromeAsyncSocketTest, UnresolvedConnect) {
  const talk_base::SocketAddress unresolved_addr(0, 0);
  EXPECT_FALSE(chrome_async_socket_->Connect(unresolved_addr));
  ExpectErrorState(ChromeAsyncSocket::STATE_CLOSED,
                   ChromeAsyncSocket::ERROR_DNS);

  EXPECT_TRUE(chrome_async_socket_->Close());
  ExpectClosed();
}

TEST_F(ChromeAsyncSocketTest, DoubleConnect) {
  EXPECT_DEBUG_DEATH({
    DoOpenClosed();

    EXPECT_FALSE(chrome_async_socket_->Connect(addr_));
    ExpectErrorState(ChromeAsyncSocket::STATE_OPEN,
                     ChromeAsyncSocket::ERROR_WRONGSTATE);

    DoCloseOpened(
        SignalSocketState(SIGNAL_CLOSE,
                          ChromeAsyncSocket::STATE_CLOSED,
                          ChromeAsyncSocket::ERROR_WRONGSTATE,
                          net::OK));
  }, "non-closed socket");
}

TEST_F(ChromeAsyncSocketTest, ImmediateConnectCloseBeforeRead) {
  DoOpenClosed();

  EXPECT_TRUE(chrome_async_socket_->Close());
  ExpectClosed();
  ExpectSignalSocketState(
      SignalSocketState::NoError(
          SIGNAL_CLOSE, ChromeAsyncSocket::STATE_CLOSED));

  message_loop_.RunAllPending();
}

TEST_F(ChromeAsyncSocketTest, HangingConnect) {
  EXPECT_TRUE(chrome_async_socket_->Connect(addr_));
  ExpectNonErrorState(ChromeAsyncSocket::STATE_CONNECTING);
  ExpectNoSignal();

  EXPECT_TRUE(chrome_async_socket_->Close());
  ExpectClosed();
  ExpectSignalSocketState(
      SignalSocketState::NoError(
          SIGNAL_CLOSE, ChromeAsyncSocket::STATE_CLOSED));
}

TEST_F(ChromeAsyncSocketTest, PendingConnect) {
  async_socket_data_provider_.set_connect_data(
      net::MockConnect(true, net::OK));
  EXPECT_TRUE(chrome_async_socket_->Connect(addr_));
  ExpectNonErrorState(ChromeAsyncSocket::STATE_CONNECTING);
  ExpectNoSignal();

  message_loop_.RunAllPending();
  ExpectNonErrorState(ChromeAsyncSocket::STATE_OPEN);
  ExpectSignalSocketState(
      SignalSocketState::NoError(
          SIGNAL_CONNECT, ChromeAsyncSocket::STATE_OPEN));
  ExpectNoSignal();

  message_loop_.RunAllPending();

  DoCloseOpenedNoError();
}

// After this no need to test successful pending connect so
// thoroughly.

TEST_F(ChromeAsyncSocketTest, PendingConnectCloseBeforeRead) {
  async_socket_data_provider_.set_connect_data(
      net::MockConnect(true, net::OK));
  EXPECT_TRUE(chrome_async_socket_->Connect(addr_));

  message_loop_.RunAllPending();
  ExpectSignalSocketState(
      SignalSocketState::NoError(
          SIGNAL_CONNECT, ChromeAsyncSocket::STATE_OPEN));

  DoCloseOpenedNoError();

  message_loop_.RunAllPending();
}

TEST_F(ChromeAsyncSocketTest, PendingConnectError) {
  async_socket_data_provider_.set_connect_data(
      net::MockConnect(true, net::ERR_TIMED_OUT));
  EXPECT_TRUE(chrome_async_socket_->Connect(addr_));

  message_loop_.RunAllPending();

  ExpectSignalSocketState(
      SignalSocketState(
          SIGNAL_CLOSE, ChromeAsyncSocket::STATE_CLOSED,
          ChromeAsyncSocket::ERROR_WINSOCK, net::ERR_TIMED_OUT));
}

// After this we can assume Connect() and Close() work as expected.

TEST_F(ChromeAsyncSocketTest, EmptyRead) {
  DoOpenClosed();

  char buf[4096];
  size_t len_read = 10000U;
  EXPECT_TRUE(chrome_async_socket_->Read(buf, sizeof(buf), &len_read));
  EXPECT_EQ(0U, len_read);

  DoCloseOpenedNoError();
}

TEST_F(ChromeAsyncSocketTest, WrongRead) {
  EXPECT_DEBUG_DEATH({
    async_socket_data_provider_.set_connect_data(
        net::MockConnect(true, net::OK));
    EXPECT_TRUE(chrome_async_socket_->Connect(addr_));
    ExpectNonErrorState(ChromeAsyncSocket::STATE_CONNECTING);
    ExpectNoSignal();

    char buf[4096];
    size_t len_read;
    EXPECT_FALSE(chrome_async_socket_->Read(buf, sizeof(buf), &len_read));
    ExpectErrorState(ChromeAsyncSocket::STATE_CONNECTING,
                     ChromeAsyncSocket::ERROR_WRONGSTATE);
    EXPECT_TRUE(chrome_async_socket_->Close());
    ExpectSignalSocketState(
        SignalSocketState(
            SIGNAL_CLOSE, ChromeAsyncSocket::STATE_CLOSED,
            ChromeAsyncSocket::ERROR_WRONGSTATE, net::OK));
  }, "non-open");
}

TEST_F(ChromeAsyncSocketTest, WrongReadClosed) {
  char buf[4096];
  size_t len_read;
  EXPECT_FALSE(chrome_async_socket_->Read(buf, sizeof(buf), &len_read));
  ExpectErrorState(ChromeAsyncSocket::STATE_CLOSED,
                   ChromeAsyncSocket::ERROR_WRONGSTATE);
  EXPECT_TRUE(chrome_async_socket_->Close());
}

const char kReadData[] = "mydatatoread";

TEST_F(ChromeAsyncSocketTest, Read) {
  async_socket_data_provider_.AddRead(net::MockRead(kReadData));
  DoOpenClosed();

  ExpectReadSignal();
  ExpectNoSignal();

  EXPECT_EQ(kReadData, DrainRead(1));

  message_loop_.RunAllPending();

  DoCloseOpenedNoError();
}

TEST_F(ChromeAsyncSocketTest, ReadTwice) {
  async_socket_data_provider_.AddRead(net::MockRead(kReadData));
  DoOpenClosed();

  ExpectReadSignal();
  ExpectNoSignal();

  EXPECT_EQ(kReadData, DrainRead(1));

  message_loop_.RunAllPending();

  const char kReadData2[] = "mydatatoread2";
  async_socket_data_provider_.AddRead(net::MockRead(kReadData2));

  ExpectReadSignal();
  ExpectNoSignal();

  EXPECT_EQ(kReadData2, DrainRead(1));

  DoCloseOpenedNoError();
}

TEST_F(ChromeAsyncSocketTest, ReadError) {
  async_socket_data_provider_.AddRead(net::MockRead(kReadData));
  DoOpenClosed();

  ExpectReadSignal();
  ExpectNoSignal();

  EXPECT_EQ(kReadData, DrainRead(1));

  message_loop_.RunAllPending();

  async_socket_data_provider_.AddRead(
      net::MockRead(false, net::ERR_TIMED_OUT));

  ExpectSignalSocketState(
      SignalSocketState(
          SIGNAL_CLOSE, ChromeAsyncSocket::STATE_CLOSED,
          ChromeAsyncSocket::ERROR_WINSOCK, net::ERR_TIMED_OUT));
}

TEST_F(ChromeAsyncSocketTest, ReadEmpty) {
  async_socket_data_provider_.AddRead(net::MockRead(""));
  DoOpenClosed();

  ExpectSignalSocketState(
      SignalSocketState::NoError(
          SIGNAL_CLOSE, ChromeAsyncSocket::STATE_CLOSED));
}

TEST_F(ChromeAsyncSocketTest, PendingRead) {
  DoOpenClosed();

  ExpectNoSignal();

  async_socket_data_provider_.AddRead(net::MockRead(kReadData));

  ExpectSignalSocketState(
      SignalSocketState::NoError(
          SIGNAL_READ, ChromeAsyncSocket::STATE_OPEN));
  ExpectNoSignal();

  EXPECT_EQ(kReadData, DrainRead(1));

  message_loop_.RunAllPending();

  DoCloseOpenedNoError();
}

TEST_F(ChromeAsyncSocketTest, PendingEmptyRead) {
  DoOpenClosed();

  ExpectNoSignal();

  async_socket_data_provider_.AddRead(net::MockRead(""));

  ExpectSignalSocketState(
      SignalSocketState::NoError(
          SIGNAL_CLOSE, ChromeAsyncSocket::STATE_CLOSED));
}

TEST_F(ChromeAsyncSocketTest, PendingReadError) {
  DoOpenClosed();

  ExpectNoSignal();

  async_socket_data_provider_.AddRead(
      net::MockRead(true, net::ERR_TIMED_OUT));

  ExpectSignalSocketState(
      SignalSocketState(
          SIGNAL_CLOSE, ChromeAsyncSocket::STATE_CLOSED,
          ChromeAsyncSocket::ERROR_WINSOCK, net::ERR_TIMED_OUT));
}

// After this we can assume non-SSL Read() works as expected.

TEST_F(ChromeAsyncSocketTest, WrongWrite) {
  EXPECT_DEBUG_DEATH({
    std::string data("foo");
    EXPECT_FALSE(chrome_async_socket_->Write(data.data(), data.size()));
    ExpectErrorState(ChromeAsyncSocket::STATE_CLOSED,
                     ChromeAsyncSocket::ERROR_WRONGSTATE);
    EXPECT_TRUE(chrome_async_socket_->Close());
  }, "non-open");
}

const char kWriteData[] = "mydatatowrite";

TEST_F(ChromeAsyncSocketTest, SyncWrite) {
  async_socket_data_provider_.AddWrite(
      net::MockWrite(false, kWriteData, 3));
  async_socket_data_provider_.AddWrite(
      net::MockWrite(false, kWriteData + 3, 5));
  async_socket_data_provider_.AddWrite(
      net::MockWrite(false, kWriteData + 8, arraysize(kWriteData) - 8));
  DoOpenClosed();

  EXPECT_TRUE(chrome_async_socket_->Write(kWriteData, 3));
  message_loop_.RunAllPending();
  EXPECT_TRUE(chrome_async_socket_->Write(kWriteData + 3, 5));
  message_loop_.RunAllPending();
  EXPECT_TRUE(chrome_async_socket_->Write(kWriteData + 8,
                                          arraysize(kWriteData) - 8));
  message_loop_.RunAllPending();

  ExpectNoSignal();

  DoCloseOpenedNoError();
}

TEST_F(ChromeAsyncSocketTest, AsyncWrite) {
  DoOpenClosed();

  async_socket_data_provider_.AddWrite(
      net::MockWrite(true, kWriteData, 3));
  async_socket_data_provider_.AddWrite(
      net::MockWrite(true, kWriteData + 3, 5));
  async_socket_data_provider_.AddWrite(
      net::MockWrite(true, kWriteData + 8, arraysize(kWriteData) - 8));

  EXPECT_TRUE(chrome_async_socket_->Write(kWriteData, 3));
  message_loop_.RunAllPending();
  EXPECT_TRUE(chrome_async_socket_->Write(kWriteData + 3, 5));
  message_loop_.RunAllPending();
  EXPECT_TRUE(chrome_async_socket_->Write(kWriteData + 8,
                                          arraysize(kWriteData) - 8));
  message_loop_.RunAllPending();

  ExpectNoSignal();

  DoCloseOpenedNoError();
}

TEST_F(ChromeAsyncSocketTest, AsyncWriteError) {
  DoOpenClosed();

  async_socket_data_provider_.AddWrite(
      net::MockWrite(true, kWriteData, 3));
  async_socket_data_provider_.AddWrite(
      net::MockWrite(true, kWriteData + 3, 5));
  async_socket_data_provider_.AddWrite(
      net::MockWrite(true, net::ERR_TIMED_OUT));

  EXPECT_TRUE(chrome_async_socket_->Write(kWriteData, 3));
  message_loop_.RunAllPending();
  EXPECT_TRUE(chrome_async_socket_->Write(kWriteData + 3, 5));
  message_loop_.RunAllPending();
  EXPECT_TRUE(chrome_async_socket_->Write(kWriteData + 8,
                                          arraysize(kWriteData) - 8));
  message_loop_.RunAllPending();

  ExpectSignalSocketState(
      SignalSocketState(
          SIGNAL_CLOSE, ChromeAsyncSocket::STATE_CLOSED,
          ChromeAsyncSocket::ERROR_WINSOCK, net::ERR_TIMED_OUT));
}

TEST_F(ChromeAsyncSocketTest, LargeWrite) {
  EXPECT_DEBUG_DEATH({
    DoOpenClosed();

    std::string large_data(100, 'x');
    EXPECT_FALSE(chrome_async_socket_->Write(large_data.data(),
                                             large_data.size()));
    ExpectState(ChromeAsyncSocket::STATE_OPEN,
                ChromeAsyncSocket::ERROR_WINSOCK,
                net::ERR_INSUFFICIENT_RESOURCES);
    DoCloseOpened(
        SignalSocketState(
            SIGNAL_CLOSE, ChromeAsyncSocket::STATE_CLOSED,
            ChromeAsyncSocket::ERROR_WINSOCK,
            net::ERR_INSUFFICIENT_RESOURCES));
    }, "exceed the max write buffer");
}

TEST_F(ChromeAsyncSocketTest, LargeAccumulatedWrite) {
  EXPECT_DEBUG_DEATH({
    DoOpenClosed();

    std::string data(15, 'x');
    EXPECT_TRUE(chrome_async_socket_->Write(data.data(), data.size()));
    EXPECT_FALSE(chrome_async_socket_->Write(data.data(), data.size()));
    ExpectState(ChromeAsyncSocket::STATE_OPEN,
                ChromeAsyncSocket::ERROR_WINSOCK,
                net::ERR_INSUFFICIENT_RESOURCES);
    DoCloseOpened(
        SignalSocketState(
            SIGNAL_CLOSE, ChromeAsyncSocket::STATE_CLOSED,
            ChromeAsyncSocket::ERROR_WINSOCK,
            net::ERR_INSUFFICIENT_RESOURCES));
    }, "exceed the max write buffer");
}

// After this we can assume non-SSL I/O works as expected.

TEST_F(ChromeAsyncSocketTest, HangingSSLConnect) {
  async_socket_data_provider_.AddRead(net::MockRead(kReadData));
  DoOpenClosed();
  ExpectReadSignal();

  EXPECT_TRUE(chrome_async_socket_->StartTls("fakedomain.com"));
  ExpectNoSignal();

  ExpectNonErrorState(ChromeAsyncSocket::STATE_TLS_CONNECTING);
  EXPECT_TRUE(chrome_async_socket_->Close());
  ExpectSignalSocketState(
      SignalSocketState::NoError(SIGNAL_CLOSE,
                                 ChromeAsyncSocket::STATE_CLOSED));
  ExpectNonErrorState(ChromeAsyncSocket::STATE_CLOSED);
}

TEST_F(ChromeAsyncSocketTest, ImmediateSSLConnect) {
  async_socket_data_provider_.AddRead(net::MockRead(kReadData));
  DoOpenClosed();
  ExpectReadSignal();

  EXPECT_TRUE(chrome_async_socket_->StartTls("fakedomain.com"));
  message_loop_.RunAllPending();
  ExpectSSLConnectSignal();
  ExpectNoSignal();
  ExpectNonErrorState(ChromeAsyncSocket::STATE_TLS_OPEN);

  DoSSLCloseOpenedNoError();
}

TEST_F(ChromeAsyncSocketTest, DoubleSSLConnect) {
  EXPECT_DEBUG_DEATH({
    async_socket_data_provider_.AddRead(net::MockRead(kReadData));
    DoOpenClosed();
    ExpectReadSignal();

    EXPECT_TRUE(chrome_async_socket_->StartTls("fakedomain.com"));
    message_loop_.RunAllPending();
    ExpectSSLConnectSignal();
    ExpectNoSignal();
    ExpectNonErrorState(ChromeAsyncSocket::STATE_TLS_OPEN);

    EXPECT_FALSE(chrome_async_socket_->StartTls("fakedomain.com"));

    DoSSLCloseOpened(
        SignalSocketState(SIGNAL_CLOSE,
                          ChromeAsyncSocket::STATE_CLOSED,
                          ChromeAsyncSocket::ERROR_WRONGSTATE,
                          net::OK));
  }, "wrong state");
}

TEST_F(ChromeAsyncSocketTest, FailedSSLConnect) {
  ssl_socket_data_provider_.connect =
      net::MockConnect(true, net::ERR_CERT_COMMON_NAME_INVALID),

  async_socket_data_provider_.AddRead(net::MockRead(kReadData));
  DoOpenClosed();
  ExpectReadSignal();

  EXPECT_TRUE(chrome_async_socket_->StartTls("fakedomain.com"));
  message_loop_.RunAllPending();
  ExpectSignalSocketState(
      SignalSocketState(
          SIGNAL_CLOSE, ChromeAsyncSocket::STATE_CLOSED,
          ChromeAsyncSocket::ERROR_WINSOCK,
          net::ERR_CERT_COMMON_NAME_INVALID));

  EXPECT_TRUE(chrome_async_socket_->Close());
  ExpectClosed();
}

TEST_F(ChromeAsyncSocketTest, ReadDuringSSLConnecting) {
  async_socket_data_provider_.AddRead(net::MockRead(kReadData));
  DoOpenClosed();
  ExpectReadSignal();
  EXPECT_EQ(kReadData, DrainRead(1));

  EXPECT_TRUE(chrome_async_socket_->StartTls("fakedomain.com"));
  ExpectNoSignal();

  // Shouldn't do anything.
  async_socket_data_provider_.AddRead(net::MockRead(kReadData));

  char buf[4096];
  size_t len_read = 10000U;
  EXPECT_TRUE(chrome_async_socket_->Read(buf, sizeof(buf), &len_read));
  EXPECT_EQ(0U, len_read);

  message_loop_.RunAllPending();
  ExpectSSLConnectSignal();
  ExpectSSLReadSignal();
  ExpectNoSignal();
  ExpectNonErrorState(ChromeAsyncSocket::STATE_TLS_OPEN);

  len_read = 10000U;
  EXPECT_TRUE(chrome_async_socket_->Read(buf, sizeof(buf), &len_read));
  EXPECT_EQ(kReadData, std::string(buf, len_read));

  DoSSLCloseOpenedNoError();
}

TEST_F(ChromeAsyncSocketTest, WriteDuringSSLConnecting) {
  async_socket_data_provider_.AddRead(net::MockRead(kReadData));
  DoOpenClosed();
  ExpectReadSignal();

  EXPECT_TRUE(chrome_async_socket_->StartTls("fakedomain.com"));
  ExpectNoSignal();
  ExpectNonErrorState(ChromeAsyncSocket::STATE_TLS_CONNECTING);

  async_socket_data_provider_.AddWrite(
      net::MockWrite(true, kWriteData, 3));

  // Shouldn't do anything.
  EXPECT_TRUE(chrome_async_socket_->Write(kWriteData, 3));

  // TODO(akalin): Figure out how to test that the write happens
  // *after* the SSL connect.

  message_loop_.RunAllPending();
  ExpectSSLConnectSignal();
  ExpectNoSignal();

  message_loop_.RunAllPending();

  DoSSLCloseOpenedNoError();
}

TEST_F(ChromeAsyncSocketTest, SSLConnectDuringPendingRead) {
  EXPECT_DEBUG_DEATH({
    DoOpenClosed();

    EXPECT_FALSE(chrome_async_socket_->StartTls("fakedomain.com"));

    DoCloseOpened(
        SignalSocketState(SIGNAL_CLOSE,
                          ChromeAsyncSocket::STATE_CLOSED,
                          ChromeAsyncSocket::ERROR_WRONGSTATE,
                          net::OK));
  }, "wrong state");
}

TEST_F(ChromeAsyncSocketTest, SSLConnectDuringPostedWrite) {
  EXPECT_DEBUG_DEATH({
    DoOpenClosed();

    async_socket_data_provider_.AddWrite(
        net::MockWrite(true, kWriteData, 3));
    EXPECT_TRUE(chrome_async_socket_->Write(kWriteData, 3));

    EXPECT_FALSE(chrome_async_socket_->StartTls("fakedomain.com"));

    message_loop_.RunAllPending();

    DoCloseOpened(
        SignalSocketState(SIGNAL_CLOSE,
                          ChromeAsyncSocket::STATE_CLOSED,
                          ChromeAsyncSocket::ERROR_WRONGSTATE,
                          net::OK));
  }, "wrong state");
}

// After this we can assume SSL connect works as expected.

TEST_F(ChromeAsyncSocketTest, SSLRead) {
  DoSSLOpenClosed();
  async_socket_data_provider_.AddRead(net::MockRead(kReadData));
  message_loop_.RunAllPending();

  ExpectSSLReadSignal();
  ExpectNoSignal();

  EXPECT_EQ(kReadData, DrainRead(1));

  message_loop_.RunAllPending();

  DoSSLCloseOpenedNoError();
}

TEST_F(ChromeAsyncSocketTest, SSLSyncWrite) {
  async_socket_data_provider_.AddWrite(
      net::MockWrite(false, kWriteData, 3));
  async_socket_data_provider_.AddWrite(
      net::MockWrite(false, kWriteData + 3, 5));
  async_socket_data_provider_.AddWrite(
      net::MockWrite(false, kWriteData + 8, arraysize(kWriteData) - 8));
  DoSSLOpenClosed();

  EXPECT_TRUE(chrome_async_socket_->Write(kWriteData, 3));
  message_loop_.RunAllPending();
  EXPECT_TRUE(chrome_async_socket_->Write(kWriteData + 3, 5));
  message_loop_.RunAllPending();
  EXPECT_TRUE(chrome_async_socket_->Write(kWriteData + 8,
                                          arraysize(kWriteData) - 8));
  message_loop_.RunAllPending();

  ExpectNoSignal();

  DoSSLCloseOpenedNoError();
}

TEST_F(ChromeAsyncSocketTest, SSLAsyncWrite) {
  DoSSLOpenClosed();

  async_socket_data_provider_.AddWrite(
      net::MockWrite(true, kWriteData, 3));
  async_socket_data_provider_.AddWrite(
      net::MockWrite(true, kWriteData + 3, 5));
  async_socket_data_provider_.AddWrite(
      net::MockWrite(true, kWriteData + 8, arraysize(kWriteData) - 8));

  EXPECT_TRUE(chrome_async_socket_->Write(kWriteData, 3));
  message_loop_.RunAllPending();
  EXPECT_TRUE(chrome_async_socket_->Write(kWriteData + 3, 5));
  message_loop_.RunAllPending();
  EXPECT_TRUE(chrome_async_socket_->Write(kWriteData + 8,
                                          arraysize(kWriteData) - 8));
  message_loop_.RunAllPending();

  ExpectNoSignal();

  DoSSLCloseOpenedNoError();
}

}  // namespace

}  // namespace notifier
