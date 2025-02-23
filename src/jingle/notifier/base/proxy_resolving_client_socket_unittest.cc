// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/base/proxy_resolving_client_socket.h"

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// TODO(sanjeevr): Move this to net_test_support.
// Used to return a dummy context.
class TestURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  TestURLRequestContextGetter()
      : message_loop_proxy_(base::MessageLoopProxy::current()) {
  }
  virtual ~TestURLRequestContextGetter() { }

  // net::URLRequestContextGetter:
  virtual net::URLRequestContext* GetURLRequestContext() {
    if (!context_)
      CreateURLRequestContext();
    return context_.get();
  }
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() const {
    return message_loop_proxy_;
  }

 private:
  void CreateURLRequestContext() {
    context_ = new TestURLRequestContext();
    context_->set_host_resolver(new net::MockHostResolver());
    context_->set_proxy_service(net::ProxyService::CreateFixedFromPacResult(
        "PROXY bad:99; PROXY maybe:80; DIRECT"));
  }

  scoped_refptr<net::URLRequestContext> context_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
};
}  // namespace

namespace notifier {

class ProxyResolvingClientSocketTest : public testing::Test {
 protected:
  ProxyResolvingClientSocketTest()
      : url_request_context_getter_(new TestURLRequestContextGetter()) {}

  virtual ~ProxyResolvingClientSocketTest() {}

  virtual void TearDown() {
    // Clear out any messages posted by ProxyResolvingClientSocket's
    // destructor.
    message_loop_.RunAllPending();
  }

  // Needed by XmppConnection.
  MessageLoopForIO message_loop_;
  scoped_refptr<TestURLRequestContextGetter> url_request_context_getter_;
};

// TODO(sanjeevr): Fix this test on Linux.
TEST_F(ProxyResolvingClientSocketTest, DISABLED_ConnectError) {
  net::HostPortPair dest("0.0.0.0", 0);
  ProxyResolvingClientSocket proxy_resolving_socket(
      NULL,
      url_request_context_getter_,
      net::SSLConfig(),
      dest);
  TestOldCompletionCallback callback;
  int status = proxy_resolving_socket.Connect(&callback);
  // Connect always returns ERR_IO_PENDING because it is always asynchronous.
  EXPECT_EQ(net::ERR_IO_PENDING, status);
  status = callback.WaitForResult();
  // ProxyResolvingClientSocket::Connect() will always return an error of
  // ERR_ADDRESS_INVALID for a 0 IP address.
  EXPECT_EQ(net::ERR_ADDRESS_INVALID, status);
}

TEST_F(ProxyResolvingClientSocketTest, ReportsBadProxies) {
  net::HostPortPair dest("example.com", 443);
  net::MockClientSocketFactory socket_factory;

  net::StaticSocketDataProvider socket_data1;
  socket_data1.set_connect_data(
      net::MockConnect(true, net::ERR_ADDRESS_UNREACHABLE));
  socket_factory.AddSocketDataProvider(&socket_data1);

  net::MockRead reads[] = {
    net::MockRead("HTTP/1.1 200 Success\r\n\r\n")
  };
  net::MockWrite writes[] = {
    net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                   "Host: example.com:443\r\n"
                   "Proxy-Connection: keep-alive\r\n\r\n")
  };
  net::StaticSocketDataProvider socket_data2(reads, arraysize(reads),
                                             writes, arraysize(writes));
  socket_data2.set_connect_data(net::MockConnect(true, net::OK));
  socket_factory.AddSocketDataProvider(&socket_data2);

  ProxyResolvingClientSocket proxy_resolving_socket(
      &socket_factory,
      url_request_context_getter_,
      net::SSLConfig(),
      dest);

  TestOldCompletionCallback callback;
  int status = proxy_resolving_socket.Connect(&callback);
  EXPECT_EQ(net::ERR_IO_PENDING, status);
  status = callback.WaitForResult();
  EXPECT_EQ(net::OK, status);

  net::URLRequestContext* context =
      url_request_context_getter_->GetURLRequestContext();
  const net::ProxyRetryInfoMap& retry_info =
      context->proxy_service()->proxy_retry_info();

  EXPECT_EQ(1u, retry_info.size());
  net::ProxyRetryInfoMap::const_iterator iter = retry_info.find("bad:99");
  EXPECT_TRUE(iter != retry_info.end());
}

// TODO(sanjeevr): Add more unit-tests.
}  // namespace notifier
