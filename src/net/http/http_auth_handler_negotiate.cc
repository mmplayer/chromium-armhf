// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_negotiate.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "net/base/address_family.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/single_request_host_resolver.h"
#include "net/http/http_auth_filter.h"
#include "net/http/url_security_manager.h"

namespace net {

HttpAuthHandlerNegotiate::Factory::Factory()
    : disable_cname_lookup_(false),
      use_port_(false),
#if defined(OS_WIN)
      max_token_length_(0),
      first_creation_(true),
#endif
      is_unsupported_(false),
      auth_library_(NULL) {
}

HttpAuthHandlerNegotiate::Factory::~Factory() {
}

void HttpAuthHandlerNegotiate::Factory::set_host_resolver(
    HostResolver* resolver) {
  resolver_ = resolver;
}

int HttpAuthHandlerNegotiate::Factory::CreateAuthHandler(
    HttpAuth::ChallengeTokenizer* challenge,
    HttpAuth::Target target,
    const GURL& origin,
    CreateReason reason,
    int digest_nonce_count,
    const BoundNetLog& net_log,
    scoped_ptr<HttpAuthHandler>* handler) {
#if defined(OS_WIN)
  if (is_unsupported_ || reason == CREATE_PREEMPTIVE)
    return ERR_UNSUPPORTED_AUTH_SCHEME;
  if (max_token_length_ == 0) {
    int rv = DetermineMaxTokenLength(auth_library_.get(), NEGOSSP_NAME,
                                     &max_token_length_);
    if (rv == ERR_UNSUPPORTED_AUTH_SCHEME)
      is_unsupported_ = true;
    if (rv != OK)
      return rv;
  }
  // TODO(cbentzel): Move towards model of parsing in the factory
  //                 method and only constructing when valid.
  scoped_ptr<HttpAuthHandler> tmp_handler(
      new HttpAuthHandlerNegotiate(auth_library_.get(), max_token_length_,
                                   url_security_manager(), resolver_,
                                   disable_cname_lookup_, use_port_));
  if (!tmp_handler->InitFromChallenge(challenge, target, origin, net_log))
    return ERR_INVALID_RESPONSE;
  handler->swap(tmp_handler);
  return OK;
#elif defined(OS_POSIX)
  if (is_unsupported_)
    return ERR_UNSUPPORTED_AUTH_SCHEME;
  if (!auth_library_->Init()) {
    is_unsupported_ = true;
    return ERR_UNSUPPORTED_AUTH_SCHEME;
  }
  // TODO(ahendrickson): Move towards model of parsing in the factory
  //                     method and only constructing when valid.
  scoped_ptr<HttpAuthHandler> tmp_handler(
      new HttpAuthHandlerNegotiate(auth_library_.get(), url_security_manager(),
                                   resolver_, disable_cname_lookup_,
                                   use_port_));
  if (!tmp_handler->InitFromChallenge(challenge, target, origin, net_log))
    return ERR_INVALID_RESPONSE;
  handler->swap(tmp_handler);
  return OK;
#endif
}

HttpAuthHandlerNegotiate::HttpAuthHandlerNegotiate(
    AuthLibrary* auth_library,
#if defined(OS_WIN)
    ULONG max_token_length,
#endif
    URLSecurityManager* url_security_manager,
    HostResolver* resolver,
    bool disable_cname_lookup,
    bool use_port)
#if defined(OS_WIN)
    : auth_system_(auth_library, "Negotiate", NEGOSSP_NAME, max_token_length),
#elif defined(OS_POSIX)
    : auth_system_(auth_library, "Negotiate", CHROME_GSS_KRB5_MECH_OID_DESC),
#endif
      disable_cname_lookup_(disable_cname_lookup),
      use_port_(use_port),
      ALLOW_THIS_IN_INITIALIZER_LIST(io_callback_(
          this, &HttpAuthHandlerNegotiate::OnIOComplete)),
      resolver_(resolver),
      already_called_(false),
      has_username_and_password_(false),
      user_callback_(NULL),
      auth_token_(NULL),
      next_state_(STATE_NONE),
      url_security_manager_(url_security_manager) {
}

HttpAuthHandlerNegotiate::~HttpAuthHandlerNegotiate() {
}

std::wstring HttpAuthHandlerNegotiate::CreateSPN(
    const AddressList& address_list, const GURL& origin) {
  // Kerberos Web Server SPNs are in the form HTTP/<host>:<port> through SSPI,
  // and in the form HTTP@<host>:<port> through GSSAPI
  //   http://msdn.microsoft.com/en-us/library/ms677601%28VS.85%29.aspx
  //
  // However, reality differs from the specification. A good description of
  // the problems can be found here:
  //   http://blog.michelbarneveld.nl/michel/archive/2009/11/14/the-reason-why-kb911149-and-kb908209-are-not-the-soluton.aspx
  //
  // Typically the <host> portion should be the canonical FQDN for the service.
  // If this could not be resolved, the original hostname in the URL will be
  // attempted instead. However, some intranets register SPNs using aliases
  // for the same canonical DNS name to allow multiple web services to reside
  // on the same host machine without requiring different ports. IE6 and IE7
  // have hotpatches that allow the default behavior to be overridden.
  //   http://support.microsoft.com/kb/911149
  //   http://support.microsoft.com/kb/938305
  //
  // According to the spec, the <port> option should be included if it is a
  // non-standard port (i.e. not 80 or 443 in the HTTP case). However,
  // historically browsers have not included the port, even on non-standard
  // ports. IE6 required a hotpatch and a registry setting to enable
  // including non-standard ports, and IE7 and IE8 also require the same
  // registry setting, but no hotpatch. Firefox does not appear to have an
  // option to include non-standard ports as of 3.6.
  //   http://support.microsoft.com/kb/908209
  //
  // Without any command-line flags, Chrome matches the behavior of Firefox
  // and IE. Users can override the behavior so aliases are allowed and
  // non-standard ports are included.
  int port = origin.EffectiveIntPort();
  std::string server;
  if (!address_list.GetCanonicalName(&server))
    server = origin.host();
#if defined(OS_WIN)
  static const char kSpnSeparator = '/';
#elif defined(OS_POSIX)
  static const char kSpnSeparator = '@';
#endif
  if (port != 80 && port != 443 && use_port_) {
    return ASCIIToWide(base::StringPrintf("HTTP%c%s:%d", kSpnSeparator,
                                          server.c_str(), port));
  } else {
    return ASCIIToWide(base::StringPrintf("HTTP%c%s", kSpnSeparator,
                                          server.c_str()));
  }
}

HttpAuth::AuthorizationResult HttpAuthHandlerNegotiate::HandleAnotherChallenge(
    HttpAuth::ChallengeTokenizer* challenge) {
  return auth_system_.ParseChallenge(challenge);
}

// Require identity on first pass instead of second.
bool HttpAuthHandlerNegotiate::NeedsIdentity() {
  return auth_system_.NeedsIdentity();
}

bool HttpAuthHandlerNegotiate::AllowsDefaultCredentials() {
  if (target_ == HttpAuth::AUTH_PROXY)
    return true;
  if (!url_security_manager_)
    return false;
  return url_security_manager_->CanUseDefaultCredentials(origin_);
}

bool HttpAuthHandlerNegotiate::AllowsExplicitCredentials() {
  return auth_system_.AllowsExplicitCredentials();
}

// The Negotiate challenge header looks like:
//   WWW-Authenticate: NEGOTIATE auth-data
bool HttpAuthHandlerNegotiate::Init(HttpAuth::ChallengeTokenizer* challenge) {
#if defined(OS_POSIX)
  if (!auth_system_.Init()) {
    VLOG(1) << "can't initialize GSSAPI library";
    return false;
  }
  // GSSAPI does not provide a way to enter username/password to
  // obtain a TGT. If the default credentials are not allowed for
  // a particular site (based on whitelist), fall back to a
  // different scheme.
  if (!AllowsDefaultCredentials())
    return false;
#endif
  if (CanDelegate())
    auth_system_.Delegate();
  auth_scheme_ = HttpAuth::AUTH_SCHEME_NEGOTIATE;
  score_ = 4;
  properties_ = ENCRYPTS_IDENTITY | IS_CONNECTION_BASED;
  HttpAuth::AuthorizationResult auth_result =
      auth_system_.ParseChallenge(challenge);
  return (auth_result == HttpAuth::AUTHORIZATION_RESULT_ACCEPT);
}

int HttpAuthHandlerNegotiate::GenerateAuthTokenImpl(
    const string16* username,
    const string16* password,
    const HttpRequestInfo* request,
    OldCompletionCallback* callback,
    std::string* auth_token) {
  DCHECK(user_callback_ == NULL);
  DCHECK((username == NULL) == (password == NULL));
  DCHECK(auth_token_ == NULL);
  auth_token_ = auth_token;
  if (already_called_) {
    DCHECK((!has_username_and_password_ && username == NULL) ||
           (has_username_and_password_ && *username == username_ &&
            *password == password_));
    next_state_ = STATE_GENERATE_AUTH_TOKEN;
  } else {
    already_called_ = true;
    if (username) {
      has_username_and_password_ = true;
      username_ = *username;
      password_ = *password;
    }
    next_state_ = STATE_RESOLVE_CANONICAL_NAME;
  }
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

void HttpAuthHandlerNegotiate::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

void HttpAuthHandlerNegotiate::DoCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(user_callback_);
  OldCompletionCallback* callback = user_callback_;
  user_callback_ = NULL;
  callback->Run(rv);
}

int HttpAuthHandlerNegotiate::DoLoop(int result) {
  DCHECK(next_state_ != STATE_NONE);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_RESOLVE_CANONICAL_NAME:
        DCHECK_EQ(OK, rv);
        rv = DoResolveCanonicalName();
        break;
      case STATE_RESOLVE_CANONICAL_NAME_COMPLETE:
        rv = DoResolveCanonicalNameComplete(rv);
        break;
      case STATE_GENERATE_AUTH_TOKEN:
        DCHECK_EQ(OK, rv);
        rv = DoGenerateAuthToken();
        break;
      case STATE_GENERATE_AUTH_TOKEN_COMPLETE:
        rv = DoGenerateAuthTokenComplete(rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int HttpAuthHandlerNegotiate::DoResolveCanonicalName() {
  next_state_ = STATE_RESOLVE_CANONICAL_NAME_COMPLETE;
  if (disable_cname_lookup_ || !resolver_)
    return OK;

  // TODO(cbentzel): Add reverse DNS lookup for numeric addresses.
  DCHECK(!single_resolve_.get());
  HostResolver::RequestInfo info(HostPortPair(origin_.host(), 0));
  info.set_host_resolver_flags(HOST_RESOLVER_CANONNAME);
  single_resolve_.reset(new SingleRequestHostResolver(resolver_));
  return single_resolve_->Resolve(info, &address_list_, &io_callback_,
                                  net_log_);
}

int HttpAuthHandlerNegotiate::DoResolveCanonicalNameComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv != OK) {
    // Even in the error case, try to use origin_.host instead of
    // passing the failure on to the caller.
    VLOG(1) << "Problem finding canonical name for SPN for host "
            << origin_.host() << ": " << ErrorToString(rv);
    rv = OK;
  }

  next_state_ = STATE_GENERATE_AUTH_TOKEN;
  spn_ = CreateSPN(address_list_, origin_);
  address_list_ = AddressList();
  return rv;
}

int HttpAuthHandlerNegotiate::DoGenerateAuthToken() {
  next_state_ = STATE_GENERATE_AUTH_TOKEN_COMPLETE;
  string16* username = has_username_and_password_ ? &username_ : NULL;
  string16* password = has_username_and_password_ ? &password_ : NULL;
  // TODO(cbentzel): This should possibly be done async.
  return auth_system_.GenerateAuthToken(username, password, spn_, auth_token_);
}

int HttpAuthHandlerNegotiate::DoGenerateAuthTokenComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  auth_token_ = NULL;
  return rv;
}

bool HttpAuthHandlerNegotiate::CanDelegate() const {
  // TODO(cbentzel): Should delegation be allowed on proxies?
  if (target_ == HttpAuth::AUTH_PROXY)
    return false;
  if (!url_security_manager_)
    return false;
  return url_security_manager_->CanDelegate(origin_);
}

}  // namespace net
