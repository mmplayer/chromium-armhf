// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_JOB_H_
#define NET_WEBSOCKETS_WEBSOCKET_JOB_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/socket_stream/socket_stream_job.h"
#include "net/spdy/spdy_websocket_stream.h"

class GURL;

namespace net {

class DrainableIOBuffer;
class WebSocketFrameHandler;
class WebSocketHandshakeRequestHandler;
class WebSocketHandshakeResponseHandler;

// WebSocket protocol specific job on SocketStream.
// It captures WebSocket handshake message and handles cookie operations.
// Chrome security policy doesn't allow renderer process (except dev tools)
// see HttpOnly cookies, so it injects cookie header in handshake request and
// strips set-cookie headers in handshake response.
// TODO(ukai): refactor websocket.cc to use this.
class NET_EXPORT WebSocketJob
    : public SocketStreamJob,
      public SocketStream::Delegate,
      public SpdyWebSocketStream::Delegate {
 public:
  // This is state of WebSocket, not SocketStream.
  enum State {
    INITIALIZED = -1,
    CONNECTING = 0,
    OPEN = 1,
    CLOSING = 2,
    CLOSED = 3,
  };

  explicit WebSocketJob(SocketStream::Delegate* delegate);

  static void EnsureInit();

  // Enable or Disable WebSocket over SPDY feature.
  // This function is intended to be called before I/O thread starts.
  static void set_websocket_over_spdy_enabled(bool enabled);

  State state() const { return state_; }
  virtual void Connect();
  virtual bool SendData(const char* data, int len);
  virtual void Close();
  virtual void RestartWithAuth(
      const string16& username,
      const string16& password);
  virtual void DetachDelegate();

  // SocketStream::Delegate methods.
  virtual int OnStartOpenConnection(
      SocketStream* socket, OldCompletionCallback* callback);
  virtual void OnConnected(SocketStream* socket, int max_pending_send_allowed);
  virtual void OnSentData(SocketStream* socket, int amount_sent);
  virtual void OnReceivedData(SocketStream* socket, const char* data, int len);
  virtual void OnClose(SocketStream* socket);
  virtual void OnAuthRequired(
      SocketStream* socket, AuthChallengeInfo* auth_info);
  virtual void OnError(const SocketStream* socket, int error);

  // SpdyWebSocketStream::Delegate methods.
  virtual void OnCreatedSpdyStream(int status);
  virtual void OnSentSpdyHeaders(int status);
  virtual int OnReceivedSpdyResponseHeader(
      const spdy::SpdyHeaderBlock& headers, int status);
  virtual void OnSentSpdyData(int amount_sent);
  virtual void OnReceivedSpdyData(const char* data, int length);
  virtual void OnCloseSpdyStream();

 private:
  friend class WebSocketThrottle;
  friend class WebSocketJobTest;
  virtual ~WebSocketJob();

  bool SendHandshakeRequest(const char* data, int len);
  void AddCookieHeaderAndSend();
  void LoadCookieCallback(const std::string& cookie);

  void OnSentHandshakeRequest(SocketStream* socket, int amount_sent);
  void OnReceivedHandshakeResponse(
      SocketStream* socket, const char* data, int len);
  void SaveCookiesAndNotifyHeaderComplete();
  void SaveNextCookie();
  void SaveCookieCallback(bool cookie_status);
  void DoSendData();

  GURL GetURLForCookies() const;

  const AddressList& address_list() const;
  int TrySpdyStream();
  void SetWaiting();
  bool IsWaiting() const;
  void Wakeup();
  void RetryPendingIO();
  void CompleteIO(int result);

  bool SendDataInternal(const char* data, int length);
  void CloseInternal();
  void SendPending();

  static bool websocket_over_spdy_enabled_;

  SocketStream::Delegate* delegate_;
  State state_;
  bool waiting_;
  AddressList addresses_;
  OldCompletionCallback* callback_;  // for throttling.

  scoped_ptr<WebSocketHandshakeRequestHandler> handshake_request_;
  scoped_ptr<WebSocketHandshakeResponseHandler> handshake_response_;

  bool started_to_send_handshake_request_;
  size_t handshake_request_sent_;

  std::vector<std::string> response_cookies_;
  size_t response_cookies_save_index_;

  scoped_ptr<WebSocketFrameHandler> send_frame_handler_;
  scoped_refptr<DrainableIOBuffer> current_buffer_;
  scoped_ptr<WebSocketFrameHandler> receive_frame_handler_;

  scoped_ptr<SpdyWebSocketStream> spdy_websocket_stream_;
  std::string challenge_;

  ScopedRunnableMethodFactory<WebSocketJob> method_factory_;
  base::WeakPtrFactory<WebSocketJob> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketJob);
};

}  // namespace

#endif  // NET_WEBSOCKETS_WEBSOCKET_JOB_H_
