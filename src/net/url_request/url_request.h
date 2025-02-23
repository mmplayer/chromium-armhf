// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_H_
#define NET_URL_REQUEST_URL_REQUEST_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/debug/leak_tracker.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "base/threading/non_thread_safe.h"
#include "googleurl/src/gurl.h"
#include "net/base/auth.h"
#include "net/base/completion_callback.h"
#include "net/base/load_states.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"
#include "net/base/network_delegate.h"
#include "net/base/request_priority.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_info.h"
#include "net/url_request/url_request_status.h"

class FilePath;
// Temporary layering violation to allow existing users of a deprecated
// interface.
class AutoUpdateInterceptor;
class ChildProcessSecurityPolicyTest;
class ComponentUpdateInterceptor;
class ResourceDispatcherHostTest;
class TestAutomationProvider;
class URLRequestAutomationJob;
class UserScriptListenerTest;
class NetworkDelayListenerTest;

// Temporary layering violation to allow existing users of a deprecated
// interface.
namespace appcache {
class AppCacheInterceptor;
class AppCacheRequestHandlerTest;
class AppCacheURLRequestJobTest;
}

namespace base {
class Time;
}  // namespace base

// Temporary layering violation to allow existing users of a deprecated
// interface.
namespace chrome_browser_net {
class ConnectInterceptor;
}

// Temporary layering violation to allow existing users of a deprecated
// interface.
namespace fileapi {
class FileSystemDirURLRequestJobTest;
class FileSystemOperationWriteTest;
class FileSystemURLRequestJobTest;
class FileWriterDelegateTest;
}

// Temporary layering violation to allow existing users of a deprecated
// interface.
namespace policy {
class CannedResponseInterceptor;
}

// Temporary layering violation to allow existing users of a deprecated
// interface.
namespace webkit_blob {
class BlobURLRequestJobTest;
}

namespace net {

class CookieList;
class CookieOptions;
class HostPortPair;
class IOBuffer;
class SSLCertRequestInfo;
class SSLInfo;
class UploadData;
class URLRequestContext;
class URLRequestJob;
class X509Certificate;

// This stores the values of the Set-Cookie headers received during the request.
// Each item in the vector corresponds to a Set-Cookie: line received,
// excluding the "Set-Cookie:" part.
typedef std::vector<std::string> ResponseCookies;

//-----------------------------------------------------------------------------
// A class  representing the asynchronous load of a data stream from an URL.
//
// The lifetime of an instance of this class is completely controlled by the
// consumer, and the instance is not required to live on the heap or be
// allocated in any special way.  It is also valid to delete an URLRequest
// object during the handling of a callback to its delegate.  Of course, once
// the URLRequest is deleted, no further callbacks to its delegate will occur.
//
// NOTE: All usage of all instances of this class should be on the same thread.
//
class NET_EXPORT URLRequest : NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  // Callback function implemented by protocol handlers to create new jobs.
  // The factory may return NULL to indicate an error, which will cause other
  // factories to be queried.  If no factory handles the request, then the
  // default job will be used.
  typedef URLRequestJob* (ProtocolFactory)(URLRequest* request,
                                           const std::string& scheme);

  // HTTP request/response header IDs (via some preprocessor fun) for use with
  // SetRequestHeaderById and GetResponseHeaderById.
  enum {
#define HTTP_ATOM(x) HTTP_ ## x,
#include "net/http/http_atom_list.h"
#undef HTTP_ATOM
  };

  // Derive from this class and add your own data members to associate extra
  // information with a URLRequest. Use GetUserData(key) and SetUserData()
  class UserData {
   public:
    UserData() {}
    virtual ~UserData() {}
  };

  // This class handles network interception.  Use with
  // (Un)RegisterRequestInterceptor.
  class NET_EXPORT Interceptor {
  public:
    virtual ~Interceptor() {}

    // Called for every request made.  Should return a new job to handle the
    // request if it should be intercepted, or NULL to allow the request to
    // be handled in the normal manner.
    virtual URLRequestJob* MaybeIntercept(URLRequest* request) = 0;

    // Called after having received a redirect response, but prior to the
    // the request delegate being informed of the redirect. Can return a new
    // job to replace the existing job if it should be intercepted, or NULL
    // to allow the normal handling to continue. If a new job is provided,
    // the delegate never sees the original redirect response, instead the
    // response produced by the intercept job will be returned.
    virtual URLRequestJob* MaybeInterceptRedirect(URLRequest* request,
                                                  const GURL& location);

    // Called after having received a final response, but prior to the
    // the request delegate being informed of the response. This is also
    // called when there is no server response at all to allow interception
    // on dns or network errors. Can return a new job to replace the existing
    // job if it should be intercepted, or NULL to allow the normal handling to
    // continue. If a new job is provided, the delegate never sees the original
    // response, instead the response produced by the intercept job will be
    // returned.
    virtual URLRequestJob* MaybeInterceptResponse(URLRequest* request);
  };

  // Deprecated interfaces in net::URLRequest. They have been moved to
  // URLRequest's private section to prevent new uses. Existing uses are
  // explicitly friended here and should be removed over time.
  class NET_EXPORT Deprecated {
   private:
    // TODO(willchan): Kill off these friend declarations.
    friend class ::AutoUpdateInterceptor;
    friend class ::ChildProcessSecurityPolicyTest;
    friend class ::ComponentUpdateInterceptor;
    friend class ::ResourceDispatcherHostTest;
    friend class ::TestAutomationProvider;
    friend class ::UserScriptListenerTest;
    friend class ::NetworkDelayListenerTest;
    friend class ::URLRequestAutomationJob;
    friend class TestInterceptor;
    friend class URLRequestFilter;
    friend class appcache::AppCacheInterceptor;
    friend class appcache::AppCacheRequestHandlerTest;
    friend class appcache::AppCacheURLRequestJobTest;
    friend class fileapi::FileSystemDirURLRequestJobTest;
    friend class fileapi::FileSystemOperationWriteTest;
    friend class fileapi::FileSystemURLRequestJobTest;
    friend class fileapi::FileWriterDelegateTest;
    friend class policy::CannedResponseInterceptor;
    friend class webkit_blob::BlobURLRequestJobTest;

    // Use URLRequestJobFactory::ProtocolHandler instead.
    static ProtocolFactory* RegisterProtocolFactory(const std::string& scheme,
                                                    ProtocolFactory* factory);

    // Use URLRequestJobFactory::Interceptor instead.
    static void RegisterRequestInterceptor(Interceptor* interceptor);
    static void UnregisterRequestInterceptor(Interceptor* interceptor);

    DISALLOW_IMPLICIT_CONSTRUCTORS(Deprecated);
  };

  // The delegate's methods are called from the message loop of the thread
  // on which the request's Start() method is called. See above for the
  // ordering of callbacks.
  //
  // The callbacks will be called in the following order:
  //   Start()
  //    - OnCertificateRequested* (zero or more calls, if the SSL server and/or
  //      SSL proxy requests a client certificate for authentication)
  //    - OnSSLCertificateError* (zero or one call, if the SSL server's
  //      certificate has an error)
  //    - OnReceivedRedirect* (zero or more calls, for the number of redirects)
  //    - OnAuthRequired* (zero or more calls, for the number of
  //      authentication failures)
  //    - OnResponseStarted
  //   Read() initiated by delegate
  //    - OnReadCompleted* (zero or more calls until all data is read)
  //
  // Read() must be called at least once. Read() returns true when it completed
  // immediately, and false if an IO is pending or if there is an error.  When
  // Read() returns false, the caller can check the Request's status() to see
  // if an error occurred, or if the IO is just pending.  When Read() returns
  // true with zero bytes read, it indicates the end of the response.
  //
  class NET_EXPORT Delegate {
   public:
    virtual ~Delegate() {}

    // Called upon a server-initiated redirect.  The delegate may call the
    // request's Cancel method to prevent the redirect from being followed.
    // Since there may be multiple chained redirects, there may also be more
    // than one redirect call.
    //
    // When this function is called, the request will still contain the
    // original URL, the destination of the redirect is provided in 'new_url'.
    // If the delegate does not cancel the request and |*defer_redirect| is
    // false, then the redirect will be followed, and the request's URL will be
    // changed to the new URL.  Otherwise if the delegate does not cancel the
    // request and |*defer_redirect| is true, then the redirect will be
    // followed once FollowDeferredRedirect is called on the URLRequest.
    //
    // The caller must set |*defer_redirect| to false, so that delegates do not
    // need to set it if they are happy with the default behavior of not
    // deferring redirect.
    virtual void OnReceivedRedirect(URLRequest* request,
                                    const GURL& new_url,
                                    bool* defer_redirect);

    // Called when we receive an authentication failure.  The delegate should
    // call request->SetAuth() with the user's credentials once it obtains them,
    // or request->CancelAuth() to cancel the login and display the error page.
    // When it does so, the request will be reissued, restarting the sequence
    // of On* callbacks.
    virtual void OnAuthRequired(URLRequest* request,
                                AuthChallengeInfo* auth_info);

    // Called when we receive an SSL CertificateRequest message for client
    // authentication.  The delegate should call
    // request->ContinueWithCertificate() with the client certificate the user
    // selected, or request->ContinueWithCertificate(NULL) to continue the SSL
    // handshake without a client certificate.
    virtual void OnCertificateRequested(
        URLRequest* request,
        SSLCertRequestInfo* cert_request_info);

    // Called when using SSL and the server responds with a certificate with
    // an error, for example, whose common name does not match the common name
    // we were expecting for that host.  The delegate should either do the
    // safe thing and Cancel() the request or decide to proceed by calling
    // ContinueDespiteLastError().  cert_error is a ERR_* error code
    // indicating what's wrong with the certificate.
    // If |is_hsts_host| is true then the host in question is an HSTS host
    // which demands a higher level of security. In this case, errors must not
    // be bypassable by the user.
    virtual void OnSSLCertificateError(URLRequest* request,
                                       const SSLInfo& ssl_info,
                                       bool is_hsts_host);

    // Called when reading cookies to allow the delegate to block access to the
    // cookie. This method will never be invoked when LOAD_DO_NOT_SEND_COOKIES
    // is specified.
    virtual bool CanGetCookies(const URLRequest* request,
                               const CookieList& cookie_list) const;

    // Called when a cookie is set to allow the delegate to block access to the
    // cookie. This method will never be invoked when LOAD_DO_NOT_SAVE_COOKIES
    // is specified.
    virtual bool CanSetCookie(const URLRequest* request,
                              const std::string& cookie_line,
                              CookieOptions* options) const;

    // After calling Start(), the delegate will receive an OnResponseStarted
    // callback when the request has completed.  If an error occurred, the
    // request->status() will be set.  On success, all redirects have been
    // followed and the final response is beginning to arrive.  At this point,
    // meta data about the response is available, including for example HTTP
    // response headers if this is a request for a HTTP resource.
    virtual void OnResponseStarted(URLRequest* request) = 0;

    // Called when the a Read of the response body is completed after an
    // IO_PENDING status from a Read() call.
    // The data read is filled into the buffer which the caller passed
    // to Read() previously.
    //
    // If an error occurred, request->status() will contain the error,
    // and bytes read will be -1.
    virtual void OnReadCompleted(URLRequest* request, int bytes_read) = 0;
  };

  // Initialize an URL request.
  URLRequest(const GURL& url, Delegate* delegate);

  // If destroyed after Start() has been called but while IO is pending,
  // then the request will be effectively canceled and the delegate
  // will not have any more of its methods called.
  ~URLRequest();

  // The user data allows the clients to associate data with this request.
  // Multiple user data values can be stored under different keys.
  // This request will TAKE OWNERSHIP of the given data pointer, and will
  // delete the object if it is changed or the request is destroyed.
  UserData* GetUserData(const void* key) const;
  void SetUserData(const void* key, UserData* data);

  // Returns true if the scheme can be handled by URLRequest. False otherwise.
  static bool IsHandledProtocol(const std::string& scheme);

  // Returns true if the url can be handled by URLRequest. False otherwise.
  // The function returns true for invalid urls because URLRequest knows how
  // to handle those.
  // NOTE: This will also return true for URLs that are handled by
  // ProtocolFactories that only work for requests that are scoped to a
  // Profile.
  static bool IsHandledURL(const GURL& url);

  // Allow access to file:// on ChromeOS for tests.
  static void AllowFileAccess();
  static bool IsFileAccessAllowed();

  // See switches::kEnableMacCookies.
  static void EnableMacCookies();
  static bool AreMacCookiesEnabled();

  // The original url is the url used to initialize the request, and it may
  // differ from the url if the request was redirected.
  const GURL& original_url() const { return url_chain_.front(); }
  // The chain of urls traversed by this request.  If the request had no
  // redirects, this vector will contain one element.
  const std::vector<GURL>& url_chain() const { return url_chain_; }
  const GURL& url() const { return url_chain_.back(); }

  // The URL that should be consulted for the third-party cookie blocking
  // policy.
  const GURL& first_party_for_cookies() const {
    return first_party_for_cookies_;
  }
  // This method may be called before Start() or FollowDeferredRedirect() is
  // called.
  void set_first_party_for_cookies(const GURL& first_party_for_cookies);

  // The request method, as an uppercase string.  "GET" is the default value.
  // The request method may only be changed before Start() is called and
  // should only be assigned an uppercase value.
  const std::string& method() const { return method_; }
  void set_method(const std::string& method);

  // The referrer URL for the request.  This header may actually be suppressed
  // from the underlying network request for security reasons (e.g., a HTTPS
  // URL will not be sent as the referrer for a HTTP request).  The referrer
  // may only be changed before Start() is called.
  const std::string& referrer() const { return referrer_; }
  void set_referrer(const std::string& referrer);
  // Returns the referrer header with potential username and password removed.
  GURL GetSanitizedReferrer() const;

  // Sets the delegate of the request.  This value may be changed at any time,
  // and it is permissible for it to be null.
  void set_delegate(Delegate* delegate);

  // The data comprising the request message body is specified as a sequence of
  // data segments and/or files containing data to upload.  These methods may
  // be called to construct the data sequence to upload, and they may only be
  // called before Start() is called.  For POST requests, the user must call
  // SetRequestHeaderBy{Id,Name} to set the Content-Type of the request to the
  // appropriate value before calling Start().
  //
  // When uploading data, bytes_len must be non-zero.
  // When uploading a file range, length must be non-zero. If length
  // exceeds the end-of-file, the upload is clipped at end-of-file. If the
  // expected modification time is provided (non-zero), it will be used to
  // check if the underlying file has been changed or not. The granularity of
  // the time comparison is 1 second since time_t precision is used in WebKit.
  void AppendBytesToUpload(const char* bytes, int bytes_len);  // takes a copy
  void AppendFileRangeToUpload(const FilePath& file_path,
                               uint64 offset, uint64 length,
                               const base::Time& expected_modification_time);
  void AppendFileToUpload(const FilePath& file_path) {
    AppendFileRangeToUpload(file_path, 0, kuint64max, base::Time());
  }

  // Indicates that the request body should be sent using chunked transfer
  // encoding. This method may only be called before Start() is called.
  void EnableChunkedUpload();

  // Appends the given bytes to the request's upload data to be sent
  // immediately via chunked transfer encoding. When all data has been sent,
  // call MarkEndOfChunks() to indicate the end of upload data.
  //
  // This method may be called only after calling EnableChunkedUpload().
  void AppendChunkToUpload(const char* bytes,
                           int bytes_len,
                           bool is_last_chunk);

  // Set the upload data directly.
  void set_upload(UploadData* upload);

  // Get the upload data directly.
  UploadData* get_upload();

  // Returns true if the request has a non-empty message body to upload.
  bool has_upload() const;

  // Set an extra request header by ID or name.  These methods may only be
  // called before Start() is called.  It is an error to call it later.
  void SetExtraRequestHeaderById(int header_id, const std::string& value,
                                 bool overwrite);
  void SetExtraRequestHeaderByName(const std::string& name,
                                   const std::string& value, bool overwrite);

  // Sets all extra request headers.  Any extra request headers set by other
  // methods are overwritten by this method.  This method may only be called
  // before Start() is called.  It is an error to call it later.
  void SetExtraRequestHeaders(const HttpRequestHeaders& headers);

  const HttpRequestHeaders& extra_request_headers() const {
    return extra_request_headers_;
  }

  // Returns the current load state for the request. |param| is an optional
  // parameter describing details related to the load state. Not all load states
  // have a parameter.
  LoadStateWithParam GetLoadState() const;
  void SetLoadStateParam(const string16& param) {
    load_state_param_ = param;
  }

  // Returns the current upload progress in bytes.
  uint64 GetUploadProgress() const;

  // Get response header(s) by ID or name.  These methods may only be called
  // once the delegate's OnResponseStarted method has been called.  Headers
  // that appear more than once in the response are coalesced, with values
  // separated by commas (per RFC 2616). This will not work with cookies since
  // comma can be used in cookie values.
  // TODO(darin): add API to enumerate response headers.
  void GetResponseHeaderById(int header_id, std::string* value);
  void GetResponseHeaderByName(const std::string& name, std::string* value);

  // Get all response headers, \n-delimited and \n\0-terminated.  This includes
  // the response status line.  Restrictions on GetResponseHeaders apply.
  void GetAllResponseHeaders(std::string* headers);

  // The time at which the returned response was requested.  For cached
  // responses, this is the last time the cache entry was validated.
  const base::Time& request_time() const {
    return response_info_.request_time;
  }

  // The time at which the returned response was generated.  For cached
  // responses, this is the last time the cache entry was validated.
  const base::Time& response_time() const {
    return response_info_.response_time;
  }

  // Indicate if this response was fetched from disk cache.
  bool was_cached() const { return response_info_.was_cached; }

  // True if response could use alternate protocol. However, browser will
  // ignore the alternate protocol if spdy is not enabled.
  bool was_fetched_via_spdy() const {
    return response_info_.was_fetched_via_spdy;
  }

  // Returns true if the URLRequest was delivered after NPN is negotiated,
  // using either SPDY or HTTP.
  bool was_npn_negotiated() const {
    return response_info_.was_npn_negotiated;
  }

  // Returns true if the URLRequest was delivered through a proxy.
  bool was_fetched_via_proxy() const {
    return response_info_.was_fetched_via_proxy;
  }

  // Returns the host and port that the content was fetched from.  See
  // http_response_info.h for caveats relating to cached content.
  HostPortPair GetSocketAddress() const;

  // Get all response headers, as a HttpResponseHeaders object.  See comments
  // in HttpResponseHeaders class as to the format of the data.
  HttpResponseHeaders* response_headers() const;

  // Get the SSL connection info.
  const SSLInfo& ssl_info() const {
    return response_info_.ssl_info;
  }

  // Returns the cookie values included in the response, if the request is one
  // that can have cookies.  Returns true if the request is a cookie-bearing
  // type, false otherwise.  This method may only be called once the
  // delegate's OnResponseStarted method has been called.
  bool GetResponseCookies(ResponseCookies* cookies);

  // Get the mime type.  This method may only be called once the delegate's
  // OnResponseStarted method has been called.
  void GetMimeType(std::string* mime_type);

  // Get the charset (character encoding).  This method may only be called once
  // the delegate's OnResponseStarted method has been called.
  void GetCharset(std::string* charset);

  // Returns the HTTP response code (e.g., 200, 404, and so on).  This method
  // may only be called once the delegate's OnResponseStarted method has been
  // called.  For non-HTTP requests, this method returns -1.
  int GetResponseCode();

  // Get the HTTP response info in its entirety.
  const HttpResponseInfo& response_info() const { return response_info_; }

  // Access the LOAD_* flags modifying this request (see load_flags.h).
  int load_flags() const { return load_flags_; }
  void set_load_flags(int flags) { load_flags_ = flags; }

  // Returns true if the request is "pending" (i.e., if Start() has been called,
  // and the response has not yet been called).
  bool is_pending() const { return is_pending_; }

  // Returns the error status of the request.
  const URLRequestStatus& status() const { return status_; }

  // Returns a globally unique identifier for this request.
  uint64 identifier() const { return identifier_; }

  // This method is called to start the request.  The delegate will receive
  // a OnResponseStarted callback when the request is started.
  void Start();

  // This method may be called at any time after Start() has been called to
  // cancel the request.  This method may be called many times, and it has
  // no effect once the response has completed.  It is guaranteed that no
  // methods of the delegate will be called after the request has been
  // cancelled, except that this may call the delegate's OnReadCompleted()
  // during the call to Cancel itself.
  void Cancel();

  // Cancels the request and sets the error to |error| (see net_error_list.h
  // for values).
  void SimulateError(int error);

  // Cancels the request and sets the error to |error| (see net_error_list.h
  // for values) and attaches |ssl_info| as the SSLInfo for that request.  This
  // is useful to attach a certificate and certificate error to a canceled
  // request.
  void SimulateSSLError(int error, const SSLInfo& ssl_info);

  // Read initiates an asynchronous read from the response, and must only
  // be called after the OnResponseStarted callback is received with a
  // successful status.
  // If data is available, Read will return true, and the data and length will
  // be returned immediately.  If data is not available, Read returns false,
  // and an asynchronous Read is initiated.  The Read is finished when
  // the caller receives the OnReadComplete callback.  Unless the request was
  // cancelled, OnReadComplete will always be called, even if the read failed.
  //
  // The buf parameter is a buffer to receive the data.  If the operation
  // completes asynchronously, the implementation will reference the buffer
  // until OnReadComplete is called.  The buffer must be at least max_bytes in
  // length.
  //
  // The max_bytes parameter is the maximum number of bytes to read.
  //
  // The bytes_read parameter is an output parameter containing the
  // the number of bytes read.  A value of 0 indicates that there is no
  // more data available to read from the stream.
  //
  // If a read error occurs, Read returns false and the request->status
  // will be set to an error.
  bool Read(IOBuffer* buf, int max_bytes, int* bytes_read);

  // If this request is being cached by the HTTP cache, stop subsequent caching.
  // Note that this method has no effect on other (simultaneous or not) requests
  // for the same resource. The typical example is a request that results in
  // the data being stored to disk (downloaded instead of rendered) so we don't
  // want to store it twice.
  void StopCaching();

  // This method may be called to follow a redirect that was deferred in
  // response to an OnReceivedRedirect call.
  void FollowDeferredRedirect();

  // One of the following two methods should be called in response to an
  // OnAuthRequired() callback (and only then).
  // SetAuth will reissue the request with the given credentials.
  // CancelAuth will give up and display the error page.
  void SetAuth(const string16& username, const string16& password);
  void CancelAuth();

  // This method can be called after the user selects a client certificate to
  // instruct this URLRequest to continue with the request with the
  // certificate.  Pass NULL if the user doesn't have a client certificate.
  void ContinueWithCertificate(X509Certificate* client_cert);

  // This method can be called after some error notifications to instruct this
  // URLRequest to ignore the current error and continue with the request.  To
  // cancel the request instead, call Cancel().
  void ContinueDespiteLastError();

  // Used to specify the context (cookie store, cache) for this request.
  const URLRequestContext* context() const;
  void set_context(const URLRequestContext* context);

  const BoundNetLog& net_log() const { return net_log_; }

  // Returns the expected content size if available
  int64 GetExpectedContentSize() const;

  // Returns the priority level for this request.
  RequestPriority priority() const { return priority_; }
  void set_priority(RequestPriority priority) {
    DCHECK_GE(priority, HIGHEST);
    DCHECK_LT(priority, NUM_PRIORITIES);
    priority_ = priority;
  }

  // This method is intended only for unit tests, but it is being used by
  // unit tests outside of net :(.
  URLRequestJob* job() { return job_; }

 protected:
  // Allow the URLRequestJob class to control the is_pending() flag.
  void set_is_pending(bool value) { is_pending_ = value; }

  // Allow the URLRequestJob class to set our status too
  void set_status(const URLRequestStatus& value) { status_ = value; }

  // Allow the URLRequestJob to redirect this request.  Returns OK if
  // successful, otherwise an error code is returned.
  int Redirect(const GURL& location, int http_status_code);

  // Called by URLRequestJob to allow interception when a redirect occurs.
  void NotifyReceivedRedirect(const GURL& location, bool* defer_redirect);

  // Allow an interceptor's URLRequestJob to restart this request.
  // Should only be called if the original job has not started a response.
  void Restart();

 private:
  friend class URLRequestJob;

  typedef std::map<const void*, linked_ptr<UserData> > UserDataMap;

  // Registers a new protocol handler for the given scheme. If the scheme is
  // already handled, this will overwrite the given factory. To delete the
  // protocol factory, use NULL for the factory BUT this WILL NOT put back
  // any previously registered protocol factory. It will have returned
  // the previously registered factory (or NULL if none is registered) when
  // the scheme was first registered so that the caller can manually put it
  // back if desired.
  //
  // The scheme must be all-lowercase ASCII. See the ProtocolFactory
  // declaration for its requirements.
  //
  // The registered protocol factory may return NULL, which will cause the
  // regular "built-in" protocol factory to be used.
  //
  static ProtocolFactory* RegisterProtocolFactory(const std::string& scheme,
                                                  ProtocolFactory* factory);

  // Registers or unregisters a network interception class.
  static void RegisterRequestInterceptor(Interceptor* interceptor);
  static void UnregisterRequestInterceptor(Interceptor* interceptor);

  // Resumes or blocks a request paused by the NetworkDelegate::OnBeforeRequest
  // handler. If |blocked| is true, the request is blocked and an error page is
  // returned indicating so. This should only be called after Start is called
  // and OnBeforeRequest returns true (signalling that the request should be
  // paused).
  void BeforeRequestComplete(int error);

  void StartJob(URLRequestJob* job);

  // Restarting involves replacing the current job with a new one such as what
  // happens when following a HTTP redirect.
  void RestartWithJob(URLRequestJob* job);
  void PrepareToRestart();

  // Detaches the job from this request in preparation for this object going
  // away or the job being replaced. The job will not call us back when it has
  // been orphaned.
  void OrphanJob();

  // Cancels the request and set the error and ssl info for this request to the
  // passed values.
  void DoCancel(int error, const SSLInfo& ssl_info);

  // Notifies the network delegate that the request has been completed.
  // This does not imply a successful completion. Also a canceled request is
  // considered completed.
  void NotifyRequestCompleted();

  // Called by URLRequestJob to allow interception when the final response
  // occurs.
  void NotifyResponseStarted();

  bool has_delegate() const { return delegate_ != NULL; }

  // These functions delegate to |delegate_| and may only be used if
  // |delegate_| is not NULL. See URLRequest::Delegate for the meaning
  // of these functions.
  void NotifyAuthRequired(AuthChallengeInfo* auth_info);
  void NotifyAuthRequiredComplete(NetworkDelegate::AuthRequiredResponse result);
  void NotifyCertificateRequested(SSLCertRequestInfo* cert_request_info);
  void NotifySSLCertificateError(const SSLInfo& ssl_info,
                                 bool is_hsts_host);
  bool CanGetCookies(const CookieList& cookie_list) const;
  bool CanSetCookie(const std::string& cookie_line,
                    CookieOptions* options) const;
  void NotifyReadCompleted(int bytes_read);

  // Called when the delegate blocks or unblocks this request when intercepting
  // certain requests.
  void SetBlockedOnDelegate();
  void SetUnblockedOnDelegate();

  // Contextual information used for this request (can be NULL). This contains
  // most of the dependencies which are shared between requests (disk cache,
  // cookie store, socket pool, etc.)
  scoped_refptr<const URLRequestContext> context_;

  // Tracks the time spent in various load states throughout this request.
  BoundNetLog net_log_;

  scoped_refptr<URLRequestJob> job_;
  scoped_refptr<UploadData> upload_;
  std::vector<GURL> url_chain_;
  GURL first_party_for_cookies_;
  GURL delegate_redirect_url_;
  std::string method_;  // "GET", "POST", etc. Should be all uppercase.
  std::string referrer_;
  HttpRequestHeaders extra_request_headers_;
  int load_flags_;  // Flags indicating the request type for the load;
                    // expected values are LOAD_* enums above.

  // Never access methods of the |delegate_| directly. Always use the
  // Notify... methods for this.
  Delegate* delegate_;

  // Current error status of the job. When no error has been encountered, this
  // will be SUCCESS. If multiple errors have been encountered, this will be
  // the first non-SUCCESS status seen.
  URLRequestStatus status_;

  // The HTTP response info, lazily initialized.
  HttpResponseInfo response_info_;

  // Tells us whether the job is outstanding. This is true from the time
  // Start() is called to the time we dispatch RequestComplete and indicates
  // whether the job is active.
  bool is_pending_;

  // Externally-defined data accessible by key
  UserDataMap user_data_;

  // Number of times we're willing to redirect.  Used to guard against
  // infinite redirects.
  int redirect_limit_;

  // Cached value for use after we've orphaned the job handling the
  // first transaction in a request involving redirects.
  uint64 final_upload_progress_;

  // The priority level for this request.  Objects like ClientSocketPool use
  // this to determine which URLRequest to allocate sockets to first.
  RequestPriority priority_;

  // TODO(battre): The only consumer of the identifier_ is currently the
  // web request API. We need to match identifiers of requests between the
  // web request API and the web navigation API. As the URLRequest does not
  // exist when the web navigation API is triggered, the tracking probably
  // needs to be done outside of the URLRequest anyway. Therefore, this
  // identifier should be deleted here. http://crbug.com/89321
  // A globally unique identifier for this request.
  const uint64 identifier_;

  // True if this request is blocked waiting for the network delegate to resume
  // it.
  bool blocked_on_delegate_;

  // An optional parameter that provides additional information about the load
  // state. Only used with the LOAD_STATE_WAITING_FOR_DELEGATE state.
  string16 load_state_param_;

  base::debug::LeakTracker<URLRequest> leak_tracker_;

  // Callback passed to the network delegate to notify us when a blocked request
  // is ready to be resumed or canceled.
  OldCompletionCallbackImpl<URLRequest> before_request_callback_;

  // Safe-guard to ensure that we do not send multiple "I am completed"
  // messages to network delegate.
  // TODO(battre): Remove this. http://crbug.com/89049
  bool has_notified_completion_;

  // Authentication data used by the NetworkDelegate for this request,
  // if one is present. |auth_credentials_| may be filled in when calling
  // |NotifyAuthRequired| on the NetworkDelegate. |auth_info_| holds
  // the authentication challenge being handled by |NotifyAuthRequired|.
  AuthCredentials auth_credentials_;
  scoped_refptr<AuthChallengeInfo> auth_info_;

  DISALLOW_COPY_AND_ASSIGN(URLRequest);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_H_
