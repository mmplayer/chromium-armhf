// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_digest.h"

#include <string>

#include "base/i18n/icu_string_conversions.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_auth.h"
#include "net/http/http_request_info.h"
#include "net/http/http_util.h"

namespace net {

// Digest authentication is specified in RFC 2617.
// The expanded derivations are listed in the tables below.

//==========+==========+==========================================+
//    qop   |algorithm |               response                   |
//==========+==========+==========================================+
//    ?     |  ?, md5, | MD5(MD5(A1):nonce:MD5(A2))               |
//          | md5-sess |                                          |
//--------- +----------+------------------------------------------+
//   auth,  |  ?, md5, | MD5(MD5(A1):nonce:nc:cnonce:qop:MD5(A2)) |
// auth-int | md5-sess |                                          |
//==========+==========+==========================================+
//    qop   |algorithm |                  A1                      |
//==========+==========+==========================================+
//          | ?, md5   | user:realm:password                      |
//----------+----------+------------------------------------------+
//          | md5-sess | MD5(user:realm:password):nonce:cnonce    |
//==========+==========+==========================================+
//    qop   |algorithm |                  A2                      |
//==========+==========+==========================================+
//  ?, auth |          | req-method:req-uri                       |
//----------+----------+------------------------------------------+
// auth-int |          | req-method:req-uri:MD5(req-entity-body)  |
//=====================+==========================================+

HttpAuthHandlerDigest::NonceGenerator::NonceGenerator() {
}

HttpAuthHandlerDigest::NonceGenerator::~NonceGenerator() {
}

HttpAuthHandlerDigest::DynamicNonceGenerator::DynamicNonceGenerator() {
}

std::string HttpAuthHandlerDigest::DynamicNonceGenerator::GenerateNonce()
    const {
  // This is how mozilla generates their cnonce -- a 16 digit hex string.
  static const char domain[] = "0123456789abcdef";
  std::string cnonce;
  cnonce.reserve(16);
  for (int i = 0; i < 16; ++i)
    cnonce.push_back(domain[base::RandInt(0, 15)]);
  return cnonce;
}

HttpAuthHandlerDigest::FixedNonceGenerator::FixedNonceGenerator(
    const std::string& nonce)
    : nonce_(nonce) {
}

std::string HttpAuthHandlerDigest::FixedNonceGenerator::GenerateNonce() const {
  return nonce_;
}

HttpAuthHandlerDigest::Factory::Factory()
    : nonce_generator_(new DynamicNonceGenerator()) {
}

HttpAuthHandlerDigest::Factory::~Factory() {
}

void HttpAuthHandlerDigest::Factory::set_nonce_generator(
    const NonceGenerator* nonce_generator) {
  nonce_generator_.reset(nonce_generator);
}

int HttpAuthHandlerDigest::Factory::CreateAuthHandler(
    HttpAuth::ChallengeTokenizer* challenge,
    HttpAuth::Target target,
    const GURL& origin,
    CreateReason reason,
    int digest_nonce_count,
    const BoundNetLog& net_log,
    scoped_ptr<HttpAuthHandler>* handler) {
  // TODO(cbentzel): Move towards model of parsing in the factory
  //                 method and only constructing when valid.
  scoped_ptr<HttpAuthHandler> tmp_handler(
      new HttpAuthHandlerDigest(digest_nonce_count, nonce_generator_.get()));
  if (!tmp_handler->InitFromChallenge(challenge, target, origin, net_log))
    return ERR_INVALID_RESPONSE;
  handler->swap(tmp_handler);
  return OK;
}

HttpAuth::AuthorizationResult HttpAuthHandlerDigest::HandleAnotherChallenge(
    HttpAuth::ChallengeTokenizer* challenge) {
  // Even though Digest is not connection based, a "second round" is parsed
  // to differentiate between stale and rejected responses.
  // Note that the state of the current handler is not mutated - this way if
  // there is a rejection the realm hasn't changed.
  if (!LowerCaseEqualsASCII(challenge->scheme(), "digest"))
    return HttpAuth::AUTHORIZATION_RESULT_INVALID;

  HttpUtil::NameValuePairsIterator parameters = challenge->param_pairs();

  // Try to find the "stale" value, and also keep track of the realm
  // for the new challenge.
  std::string original_realm;
  while (parameters.GetNext()) {
    if (LowerCaseEqualsASCII(parameters.name(), "stale")) {
      if (LowerCaseEqualsASCII(parameters.value(), "true"))
        return HttpAuth::AUTHORIZATION_RESULT_STALE;
    } else if (LowerCaseEqualsASCII(parameters.name(), "realm")) {
      original_realm = parameters.value();
    }
  }
  return (original_realm_ != original_realm) ?
      HttpAuth::AUTHORIZATION_RESULT_DIFFERENT_REALM :
      HttpAuth::AUTHORIZATION_RESULT_REJECT;
}

bool HttpAuthHandlerDigest::Init(HttpAuth::ChallengeTokenizer* challenge) {
  return ParseChallenge(challenge);
}

int HttpAuthHandlerDigest::GenerateAuthTokenImpl(
    const string16* username,
    const string16* password,
    const HttpRequestInfo* request,
    OldCompletionCallback* callback,
    std::string* auth_token) {
  // Generate a random client nonce.
  std::string cnonce = nonce_generator_->GenerateNonce();

  // Extract the request method and path -- the meaning of 'path' is overloaded
  // in certain cases, to be a hostname.
  std::string method;
  std::string path;
  GetRequestMethodAndPath(request, &method, &path);

  *auth_token = AssembleCredentials(method, path,
                                    *username,
                                    *password,
                                    cnonce, nonce_count_);
  return OK;
}

HttpAuthHandlerDigest::HttpAuthHandlerDigest(
    int nonce_count, const NonceGenerator* nonce_generator)
    : stale_(false),
      algorithm_(ALGORITHM_UNSPECIFIED),
      qop_(QOP_UNSPECIFIED),
      nonce_count_(nonce_count),
      nonce_generator_(nonce_generator) {
  DCHECK(nonce_generator_);
}

HttpAuthHandlerDigest::~HttpAuthHandlerDigest() {
}

// The digest challenge header looks like:
//   WWW-Authenticate: Digest
//     [realm="<realm-value>"]
//     nonce="<nonce-value>"
//     [domain="<list-of-URIs>"]
//     [opaque="<opaque-token-value>"]
//     [stale="<true-or-false>"]
//     [algorithm="<digest-algorithm>"]
//     [qop="<list-of-qop-values>"]
//     [<extension-directive>]
//
// Note that according to RFC 2617 (section 1.2) the realm is required.
// However we allow it to be omitted, in which case it will default to the
// empty string.
//
// This allowance is for better compatibility with webservers that fail to
// send the realm (See http://crbug.com/20984 for an instance where a
// webserver was not sending the realm with a BASIC challenge).
bool HttpAuthHandlerDigest::ParseChallenge(
    HttpAuth::ChallengeTokenizer* challenge) {
  auth_scheme_ = HttpAuth::AUTH_SCHEME_DIGEST;
  score_ = 2;
  properties_ = ENCRYPTS_IDENTITY;

  // Initialize to defaults.
  stale_ = false;
  algorithm_ = ALGORITHM_UNSPECIFIED;
  qop_ = QOP_UNSPECIFIED;
  realm_ = original_realm_ = nonce_ = domain_ = opaque_ = std::string();

  // FAIL -- Couldn't match auth-scheme.
  if (!LowerCaseEqualsASCII(challenge->scheme(), "digest"))
    return false;

  HttpUtil::NameValuePairsIterator parameters = challenge->param_pairs();

  // Loop through all the properties.
  while (parameters.GetNext()) {
    // FAIL -- couldn't parse a property.
    if (!ParseChallengeProperty(parameters.name(),
                                parameters.value()))
      return false;
  }

  // Check if tokenizer failed.
  if (!parameters.valid())
    return false;

  // Check that a minimum set of properties were provided.
  if (nonce_.empty())
    return false;

  return true;
}

bool HttpAuthHandlerDigest::ParseChallengeProperty(const std::string& name,
                                                   const std::string& value) {
  if (LowerCaseEqualsASCII(name, "realm")) {
    std::string realm;
    if (!base::ConvertToUtf8AndNormalize(value, base::kCodepageLatin1, &realm))
      return false;
    realm_ = realm;
    original_realm_ = value;
  } else if (LowerCaseEqualsASCII(name, "nonce")) {
    nonce_ = value;
  } else if (LowerCaseEqualsASCII(name, "domain")) {
    domain_ = value;
  } else if (LowerCaseEqualsASCII(name, "opaque")) {
    opaque_ = value;
  } else if (LowerCaseEqualsASCII(name, "stale")) {
    // Parse the stale boolean.
    stale_ = LowerCaseEqualsASCII(value, "true");
  } else if (LowerCaseEqualsASCII(name, "algorithm")) {
    // Parse the algorithm.
    if (LowerCaseEqualsASCII(value, "md5")) {
      algorithm_ = ALGORITHM_MD5;
    } else if (LowerCaseEqualsASCII(value, "md5-sess")) {
      algorithm_ = ALGORITHM_MD5_SESS;
    } else {
      DVLOG(1) << "Unknown value of algorithm";
      return false;  // FAIL -- unsupported value of algorithm.
    }
  } else if (LowerCaseEqualsASCII(name, "qop")) {
    // Parse the comma separated list of qops.
    // auth is the only supported qop, and all other values are ignored.
    HttpUtil::ValuesIterator qop_values(value.begin(), value.end(), ',');
    qop_ = QOP_UNSPECIFIED;
    while (qop_values.GetNext()) {
      if (LowerCaseEqualsASCII(qop_values.value(), "auth")) {
        qop_ = QOP_AUTH;
        break;
      }
    }
  } else {
    DVLOG(1) << "Skipping unrecognized digest property";
    // TODO(eroman): perhaps we should fail instead of silently skipping?
  }
  return true;
}

// static
std::string HttpAuthHandlerDigest::QopToString(QualityOfProtection qop) {
  switch (qop) {
    case QOP_UNSPECIFIED:
      return "";
    case QOP_AUTH:
      return "auth";
    default:
      NOTREACHED();
      return "";
  }
}

// static
std::string HttpAuthHandlerDigest::AlgorithmToString(
    DigestAlgorithm algorithm) {
  switch (algorithm) {
    case ALGORITHM_UNSPECIFIED:
      return "";
    case ALGORITHM_MD5:
      return "MD5";
    case ALGORITHM_MD5_SESS:
      return "MD5-sess";
    default:
      NOTREACHED();
      return "";
  }
}

void HttpAuthHandlerDigest::GetRequestMethodAndPath(
    const HttpRequestInfo* request,
    std::string* method,
    std::string* path) const {
  DCHECK(request);

  const GURL& url = request->url;

  if (target_ == HttpAuth::AUTH_PROXY && url.SchemeIs("https")) {
    *method = "CONNECT";
    *path = GetHostAndPort(url);
  } else {
    *method = request->method;
    *path = HttpUtil::PathForRequest(url);
  }
}

std::string HttpAuthHandlerDigest::AssembleResponseDigest(
    const std::string& method,
    const std::string& path,
    const string16& username,
    const string16& password,
    const std::string& cnonce,
    const std::string& nc) const {
  // ha1 = MD5(A1)
  // TODO(eroman): is this the right encoding?
  std::string ha1 = base::MD5String(UTF16ToUTF8(username) + ":" +
                                    original_realm_ + ":" +
                                    UTF16ToUTF8(password));
  if (algorithm_ == HttpAuthHandlerDigest::ALGORITHM_MD5_SESS)
    ha1 = base::MD5String(ha1 + ":" + nonce_ + ":" + cnonce);

  // ha2 = MD5(A2)
  // TODO(eroman): need to add MD5(req-entity-body) for qop=auth-int.
  std::string ha2 = base::MD5String(method + ":" + path);

  std::string nc_part;
  if (qop_ != HttpAuthHandlerDigest::QOP_UNSPECIFIED) {
    nc_part = nc + ":" + cnonce + ":" + QopToString(qop_) + ":";
  }

  return base::MD5String(ha1 + ":" + nonce_ + ":" + nc_part + ha2);
}

std::string HttpAuthHandlerDigest::AssembleCredentials(
    const std::string& method,
    const std::string& path,
    const string16& username,
    const string16& password,
    const std::string& cnonce,
    int nonce_count) const {
  // the nonce-count is an 8 digit hex string.
  std::string nc = base::StringPrintf("%08x", nonce_count);

  // TODO(eroman): is this the right encoding?
  std::string authorization = (std::string("Digest username=") +
                               HttpUtil::Quote(UTF16ToUTF8(username)));
  authorization += ", realm=" + HttpUtil::Quote(original_realm_);
  authorization += ", nonce=" + HttpUtil::Quote(nonce_);
  authorization += ", uri=" + HttpUtil::Quote(path);

  if (algorithm_ != ALGORITHM_UNSPECIFIED) {
    authorization += ", algorithm=" + AlgorithmToString(algorithm_);
  }
  std::string response = AssembleResponseDigest(method, path, username,
                                                password, cnonce, nc);
  // No need to call HttpUtil::Quote() as the response digest cannot contain
  // any characters needing to be escaped.
  authorization += ", response=\"" + response + "\"";

  if (!opaque_.empty()) {
    authorization += ", opaque=" + HttpUtil::Quote(opaque_);
  }
  if (qop_ != QOP_UNSPECIFIED) {
    // TODO(eroman): Supposedly IIS server requires quotes surrounding qop.
    authorization += ", qop=" + QopToString(qop_);
    authorization += ", nc=" + nc;
    authorization += ", cnonce=" + HttpUtil::Quote(cnonce);
  }

  return authorization;
}

}  // namespace net
