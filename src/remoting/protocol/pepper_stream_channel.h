// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PEPPER_STREAM_CHANNEL_H_
#define REMOTING_PROTOCOL_PEPPER_STREAM_CHANNEL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/pepper_channel.h"
#include "remoting/protocol/pepper_transport_socket_adapter.h"
#include "remoting/protocol/session.h"

namespace net {
class CertVerifier;
class StreamSocket;
class SSLClientSocket;
class SSLServerSocket;
}  // namespace net

namespace remoting {
namespace protocol {

class PepperSession;

class PepperStreamChannel : public PepperChannel,
                            public PepperTransportSocketAdapter::Observer {
 public:
  PepperStreamChannel(PepperSession* session,
                      const std::string& name,
                      const Session::StreamChannelCallback& callback);
  virtual ~PepperStreamChannel();

  // PepperChannel implementation.
  virtual void Connect(pp::Instance* pp_instance,
                       const TransportConfig& transport_config,
                       const std::string& remote_cert) OVERRIDE;
  virtual void AddRemoveCandidate(const cricket::Candidate& candidate) OVERRIDE;
  virtual const std::string& name() OVERRIDE;

  // PepperTransportSocketAdapter implementation.
  virtual void OnChannelDeleted() OVERRIDE;
  virtual void OnChannelNewLocalCandidate(
      const std::string& candidate) OVERRIDE;

 private:
  void OnP2PConnect(int result);

  bool EstablishSSLConnection();
  void OnSSLConnect(int result);

  void AuthenticateChannel();
  void OnAuthenticationDone(ChannelAuthenticator::Result result);

  void NotifyConnected(net::StreamSocket* socket);
  void NotifyConnectFailed();

  PepperSession* session_;
  std::string name_;
  Session::StreamChannelCallback callback_;

  std::string remote_cert_;

  // We own |channel_| until it is connected. After that
  // SSLClientSocket owns it.
  scoped_ptr<PepperTransportSocketAdapter> owned_channel_;
  PepperTransportSocketAdapter* channel_;

  // Indicates that we've finished connecting.
  bool connected_;

  scoped_ptr<net::StreamSocket> socket_;
  net::SSLClientSocket* ssl_client_socket_;

  // Used to verify the certificate received in SSLClientSocket.
  scoped_ptr<net::CertVerifier> cert_verifier_;

  scoped_ptr<ChannelAuthenticator> authenticator_;

  // Callback called by the TCP and SSL layers.
  net::OldCompletionCallbackImpl<PepperStreamChannel> p2p_connect_callback_;
  net::OldCompletionCallbackImpl<PepperStreamChannel> ssl_connect_callback_;

  DISALLOW_COPY_AND_ASSIGN(PepperStreamChannel);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PEPPER_STREAM_CHANNEL_H_
