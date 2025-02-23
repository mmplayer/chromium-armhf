// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_TRANSACTION_H_
#define NET_HTTP_HTTP_TRANSACTION_H_
#pragma once

#include "base/string16.h"
#include "net/base/completion_callback.h"
#include "net/base/load_states.h"
#include "net/base/net_export.h"

namespace net {

class BoundNetLog;
struct HttpRequestInfo;
class HttpResponseInfo;
class IOBuffer;
class X509Certificate;
class SSLHostInfo;

// Represents a single HTTP transaction (i.e., a single request/response pair).
// HTTP redirects are not followed and authentication challenges are not
// answered.  Cookies are assumed to be managed by the caller.
class NET_EXPORT_PRIVATE HttpTransaction {
 public:
  // Stops any pending IO and destroys the transaction object.
  virtual ~HttpTransaction() {}

  // Starts the HTTP transaction (i.e., sends the HTTP request).
  //
  // Returns OK if the transaction could be started synchronously, which means
  // that the request was served from the cache.  ERR_IO_PENDING is returned to
  // indicate that the OldCompletionCallback will be notified once response info
  // is available or if an IO error occurs.  Any other return value indicates
  // that the transaction could not be started.
  //
  // Regardless of the return value, the caller is expected to keep the
  // request_info object alive until Destroy is called on the transaction.
  //
  // NOTE: The transaction is not responsible for deleting the callback object.
  //
  // Profiling information for the request is saved to |net_log| if non-NULL.
  virtual int Start(const HttpRequestInfo* request_info,
                    OldCompletionCallback* callback,
                    const BoundNetLog& net_log) = 0;

  // Restarts the HTTP transaction, ignoring the last error.  This call can
  // only be made after a call to Start (or RestartIgnoringLastError) failed.
  // Once Read has been called, this method cannot be called.  This method is
  // used, for example, to continue past various SSL related errors.
  //
  // Not all errors can be ignored using this method.  See error code
  // descriptions for details about errors that can be ignored.
  //
  // NOTE: The transaction is not responsible for deleting the callback object.
  //
  virtual int RestartIgnoringLastError(OldCompletionCallback* callback) = 0;

  // Restarts the HTTP transaction with a client certificate.
  virtual int RestartWithCertificate(X509Certificate* client_cert,
                                     OldCompletionCallback* callback) = 0;

  // Restarts the HTTP transaction with authentication credentials.
  virtual int RestartWithAuth(const string16& username,
                              const string16& password,
                              OldCompletionCallback* callback) = 0;

  // Returns true if auth is ready to be continued. Callers should check
  // this value anytime Start() completes: if it is true, the transaction
  // can be resumed with RestartWithAuth(L"", L"", callback) to resume
  // the automatic auth exchange. This notification gives the caller a
  // chance to process the response headers from all of the intermediate
  // restarts needed for authentication.
  virtual bool IsReadyToRestartForAuth() = 0;

  // Once response info is available for the transaction, response data may be
  // read by calling this method.
  //
  // Response data is copied into the given buffer and the number of bytes
  // copied is returned.  ERR_IO_PENDING is returned if response data is not
  // yet available.  The OldCompletionCallback is notified when the data copy
  // completes, and it is passed the number of bytes that were successfully
  // copied.  Or, if a read error occurs, the OldCompletionCallback is notified of
  // the error.  Any other negative return value indicates that the transaction
  // could not be read.
  //
  // NOTE: The transaction is not responsible for deleting the callback object.
  // If the operation is not completed immediately, the transaction must acquire
  // a reference to the provided buffer.
  //
  virtual int Read(IOBuffer* buf, int buf_len,
                   OldCompletionCallback* callback) = 0;

  // Stops further caching of this request by the HTTP cache, if there is any.
  virtual void StopCaching() = 0;

  // Called to tell the transaction that we have successfully reached the end
  // of the stream. This is equivalent to performing an extra Read() at the end
  // that should return 0 bytes. This method should not be called if the
  // transaction is busy processing a previous operation (like a pending Read).
  virtual void DoneReading() = 0;

  // Returns the response info for this transaction or NULL if the response
  // info is not available.
  virtual const HttpResponseInfo* GetResponseInfo() const = 0;

  // Returns the load state for this transaction.
  virtual LoadState GetLoadState() const = 0;

  // Returns the upload progress in bytes.  If there is no upload data,
  // zero will be returned.  This does not include the request headers.
  virtual uint64 GetUploadProgress() const = 0;

  // SetSSLHostInfo sets a object which reads and writes public information
  // about an SSL server. It's used to implement Snap Start.
  // TODO(agl): remove this.
  virtual void SetSSLHostInfo(SSLHostInfo*) { };
};

}  // namespace net

#endif  // NET_HTTP_HTTP_TRANSACTION_H_
