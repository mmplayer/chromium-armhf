// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See "SSPI Sample Application" at
// http://msdn.microsoft.com/en-us/library/aa918273.aspx

#include "net/http/http_auth_sspi_win.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth.h"

namespace net {

namespace {

int MapAcquireCredentialsStatusToError(SECURITY_STATUS status,
                                       const SEC_WCHAR* package) {
  VLOG(1) << "AcquireCredentialsHandle returned 0x" << std::hex << status;
  switch (status) {
    case SEC_E_OK:
      return OK;
    case SEC_E_INSUFFICIENT_MEMORY:
      return ERR_OUT_OF_MEMORY;
    case SEC_E_INTERNAL_ERROR:
      LOG(WARNING)
          << "AcquireCredentialsHandle returned unexpected status 0x"
          << std::hex << status;
      return ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS;
    case SEC_E_NO_CREDENTIALS:
    case SEC_E_NOT_OWNER:
    case SEC_E_UNKNOWN_CREDENTIALS:
      return ERR_INVALID_AUTH_CREDENTIALS;
    case SEC_E_SECPKG_NOT_FOUND:
      // This indicates that the SSPI configuration does not match expectations
      return ERR_UNSUPPORTED_AUTH_SCHEME;
    default:
      LOG(WARNING)
          << "AcquireCredentialsHandle returned undocumented status 0x"
          << std::hex << status;
      return ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS;
  }
}

int AcquireExplicitCredentials(SSPILibrary* library,
                               const SEC_WCHAR* package,
                               const string16& domain,
                               const string16& user,
                               const string16& password,
                               CredHandle* cred) {
  SEC_WINNT_AUTH_IDENTITY identity;
  identity.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
  identity.User =
      reinterpret_cast<unsigned short*>(const_cast<wchar_t*>(user.c_str()));
  identity.UserLength = user.size();
  identity.Domain =
      reinterpret_cast<unsigned short*>(const_cast<wchar_t*>(domain.c_str()));
  identity.DomainLength = domain.size();
  identity.Password =
      reinterpret_cast<unsigned short*>(const_cast<wchar_t*>(password.c_str()));
  identity.PasswordLength = password.size();

  TimeStamp expiry;

  // Pass the username/password to get the credentials handle.
  SECURITY_STATUS status = library->AcquireCredentialsHandle(
      NULL,  // pszPrincipal
      const_cast<SEC_WCHAR*>(package),  // pszPackage
      SECPKG_CRED_OUTBOUND,  // fCredentialUse
      NULL,  // pvLogonID
      &identity,  // pAuthData
      NULL,  // pGetKeyFn (not used)
      NULL,  // pvGetKeyArgument (not used)
      cred,  // phCredential
      &expiry);  // ptsExpiry

  return MapAcquireCredentialsStatusToError(status, package);
}

int AcquireDefaultCredentials(SSPILibrary* library, const SEC_WCHAR* package,
                              CredHandle* cred) {
  TimeStamp expiry;

  // Pass the username/password to get the credentials handle.
  // Note: Since the 5th argument is NULL, it uses the default
  // cached credentials for the logged in user, which can be used
  // for a single sign-on.
  SECURITY_STATUS status = library->AcquireCredentialsHandle(
      NULL,  // pszPrincipal
      const_cast<SEC_WCHAR*>(package),  // pszPackage
      SECPKG_CRED_OUTBOUND,  // fCredentialUse
      NULL,  // pvLogonID
      NULL,  // pAuthData
      NULL,  // pGetKeyFn (not used)
      NULL,  // pvGetKeyArgument (not used)
      cred,  // phCredential
      &expiry);  // ptsExpiry

  return MapAcquireCredentialsStatusToError(status, package);
}

int MapInitializeSecurityContextStatusToError(SECURITY_STATUS status) {
  VLOG(1) << "InitializeSecurityContext returned 0x" << std::hex << status;
  switch (status) {
    case SEC_E_OK:
    case SEC_I_CONTINUE_NEEDED:
      return OK;
    case SEC_I_COMPLETE_AND_CONTINUE:
    case SEC_I_COMPLETE_NEEDED:
    case SEC_I_INCOMPLETE_CREDENTIALS:
    case SEC_E_INCOMPLETE_MESSAGE:
    case SEC_E_INTERNAL_ERROR:
      // These are return codes reported by InitializeSecurityContext
      // but not expected by Chrome (for example, INCOMPLETE_CREDENTIALS
      // and INCOMPLETE_MESSAGE are intended for schannel).
      LOG(WARNING)
          << "InitializeSecurityContext returned unexpected status 0x"
          << std::hex << status;
      return ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS;
    case SEC_E_INSUFFICIENT_MEMORY:
      return ERR_OUT_OF_MEMORY;
    case SEC_E_UNSUPPORTED_FUNCTION:
      NOTREACHED();
      return ERR_UNEXPECTED;
    case SEC_E_INVALID_HANDLE:
      NOTREACHED();
      return ERR_INVALID_HANDLE;
    case SEC_E_INVALID_TOKEN:
      return ERR_INVALID_RESPONSE;
    case SEC_E_LOGON_DENIED:
      return ERR_ACCESS_DENIED;
    case SEC_E_NO_CREDENTIALS:
    case SEC_E_WRONG_PRINCIPAL:
      return ERR_INVALID_AUTH_CREDENTIALS;
    case SEC_E_NO_AUTHENTICATING_AUTHORITY:
    case SEC_E_TARGET_UNKNOWN:
      return ERR_MISCONFIGURED_AUTH_ENVIRONMENT;
    default:
      LOG(WARNING)
          << "InitializeSecurityContext returned undocumented status 0x"
          << std::hex << status;
      return ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS;
  }
}

int MapQuerySecurityPackageInfoStatusToError(SECURITY_STATUS status) {
  VLOG(1) << "QuerySecurityPackageInfo returned 0x" << std::hex << status;
  switch (status) {
    case SEC_E_OK:
      return OK;
    case SEC_E_SECPKG_NOT_FOUND:
      // This isn't a documented return code, but has been encountered
      // during testing.
      return ERR_UNSUPPORTED_AUTH_SCHEME;
    default:
      LOG(WARNING)
          << "QuerySecurityPackageInfo returned undocumented status 0x"
          << std::hex << status;
      return ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS;
  }
}

int MapFreeContextBufferStatusToError(SECURITY_STATUS status) {
  VLOG(1) << "FreeContextBuffer returned 0x" << std::hex << status;
  switch (status) {
    case SEC_E_OK:
      return OK;
    default:
      // The documentation at
      // http://msdn.microsoft.com/en-us/library/aa375416(VS.85).aspx
      // only mentions that a non-zero (or non-SEC_E_OK) value is returned
      // if the function fails, and does not indicate what the failure
      // conditions are.
      LOG(WARNING)
          << "FreeContextBuffer returned undocumented status 0x"
          << std::hex << status;
      return ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS;
  }
}

}  // anonymous namespace

HttpAuthSSPI::HttpAuthSSPI(SSPILibrary* library,
                           const std::string& scheme,
                           SEC_WCHAR* security_package,
                           ULONG max_token_length)
    : library_(library),
      scheme_(scheme),
      security_package_(security_package),
      max_token_length_(max_token_length),
      can_delegate_(false) {
  DCHECK(library_);
  SecInvalidateHandle(&cred_);
  SecInvalidateHandle(&ctxt_);
}

HttpAuthSSPI::~HttpAuthSSPI() {
  ResetSecurityContext();
  if (SecIsValidHandle(&cred_)) {
    library_->FreeCredentialsHandle(&cred_);
    SecInvalidateHandle(&cred_);
  }
}

bool HttpAuthSSPI::NeedsIdentity() const {
  return decoded_server_auth_token_.empty();
}

bool HttpAuthSSPI::AllowsExplicitCredentials() const {
  return true;
}

void HttpAuthSSPI::Delegate() {
  can_delegate_ = true;
}

void HttpAuthSSPI::ResetSecurityContext() {
  if (SecIsValidHandle(&ctxt_)) {
    library_->DeleteSecurityContext(&ctxt_);
    SecInvalidateHandle(&ctxt_);
  }
}

HttpAuth::AuthorizationResult HttpAuthSSPI::ParseChallenge(
    HttpAuth::ChallengeTokenizer* tok) {
  // Verify the challenge's auth-scheme.
  if (!LowerCaseEqualsASCII(tok->scheme(), StringToLowerASCII(scheme_).c_str()))
    return HttpAuth::AUTHORIZATION_RESULT_INVALID;

  std::string encoded_auth_token = tok->base64_param();
  if (encoded_auth_token.empty()) {
    // If a context has already been established, an empty challenge
    // should be treated as a rejection of the current attempt.
    if (SecIsValidHandle(&ctxt_))
      return HttpAuth::AUTHORIZATION_RESULT_REJECT;
    DCHECK(decoded_server_auth_token_.empty());
    return HttpAuth::AUTHORIZATION_RESULT_ACCEPT;
  } else {
    // If a context has not already been established, additional tokens should
    // not be present in the auth challenge.
    if (!SecIsValidHandle(&ctxt_))
      return HttpAuth::AUTHORIZATION_RESULT_INVALID;
  }

  std::string decoded_auth_token;
  bool base64_rv = base::Base64Decode(encoded_auth_token, &decoded_auth_token);
  if (!base64_rv)
    return HttpAuth::AUTHORIZATION_RESULT_INVALID;
  decoded_server_auth_token_ = decoded_auth_token;
  return HttpAuth::AUTHORIZATION_RESULT_ACCEPT;
}

int HttpAuthSSPI::GenerateAuthToken(const string16* username,
                                    const string16* password,
                                    const std::wstring& spn,
                                    std::string* auth_token) {
  DCHECK((username == NULL) == (password == NULL));

  // Initial challenge.
  if (!SecIsValidHandle(&cred_)) {
    int rv = OnFirstRound(username, password);
    if (rv != OK)
      return rv;
  }

  DCHECK(SecIsValidHandle(&cred_));
  void* out_buf;
  int out_buf_len;
  int rv = GetNextSecurityToken(
      spn,
      static_cast<void *>(const_cast<char *>(
          decoded_server_auth_token_.c_str())),
      decoded_server_auth_token_.length(),
      &out_buf,
      &out_buf_len);
  if (rv != OK)
    return rv;

  // Base64 encode data in output buffer and prepend the scheme.
  std::string encode_input(static_cast<char*>(out_buf), out_buf_len);
  std::string encode_output;
  bool base64_rv = base::Base64Encode(encode_input, &encode_output);
  // OK, we are done with |out_buf|
  free(out_buf);
  if (!base64_rv) {
    LOG(ERROR) << "Base64 encoding of auth token failed.";
    return ERR_ENCODING_CONVERSION_FAILED;
  }
  *auth_token = scheme_ + " " + encode_output;
  return OK;
}

int HttpAuthSSPI::OnFirstRound(const string16* username,
                               const string16* password) {
  DCHECK((username == NULL) == (password == NULL));
  DCHECK(!SecIsValidHandle(&cred_));
  int rv = OK;
  if (username) {
    string16 domain;
    string16 user;
    SplitDomainAndUser(*username, &domain, &user);
    rv = AcquireExplicitCredentials(library_, security_package_, domain,
                                    user, *password, &cred_);
    if (rv != OK)
      return rv;
  } else {
    rv = AcquireDefaultCredentials(library_, security_package_, &cred_);
    if (rv != OK)
      return rv;
  }

  return rv;
}

int HttpAuthSSPI::GetNextSecurityToken(
    const std::wstring& spn,
    const void* in_token,
    int in_token_len,
    void** out_token,
    int* out_token_len) {
  CtxtHandle* ctxt_ptr;
  SecBufferDesc in_buffer_desc, out_buffer_desc;
  SecBufferDesc* in_buffer_desc_ptr;
  SecBuffer in_buffer, out_buffer;

  if (in_token_len > 0) {
    // Prepare input buffer.
    in_buffer_desc.ulVersion = SECBUFFER_VERSION;
    in_buffer_desc.cBuffers = 1;
    in_buffer_desc.pBuffers = &in_buffer;
    in_buffer.BufferType = SECBUFFER_TOKEN;
    in_buffer.cbBuffer = in_token_len;
    in_buffer.pvBuffer = const_cast<void*>(in_token);
    ctxt_ptr = &ctxt_;
    in_buffer_desc_ptr = &in_buffer_desc;
  } else {
    // If there is no input token, then we are starting a new authentication
    // sequence.  If we have already initialized our security context, then
    // we're incorrectly reusing the auth handler for a new sequence.
    if (SecIsValidHandle(&ctxt_)) {
      NOTREACHED();
      return ERR_UNEXPECTED;
    }
    ctxt_ptr = NULL;
    in_buffer_desc_ptr = NULL;
  }

  // Prepare output buffer.
  out_buffer_desc.ulVersion = SECBUFFER_VERSION;
  out_buffer_desc.cBuffers = 1;
  out_buffer_desc.pBuffers = &out_buffer;
  out_buffer.BufferType = SECBUFFER_TOKEN;
  out_buffer.cbBuffer = max_token_length_;
  out_buffer.pvBuffer = malloc(out_buffer.cbBuffer);
  if (!out_buffer.pvBuffer)
    return ERR_OUT_OF_MEMORY;

  DWORD context_flags = 0;
  // Firefox only sets ISC_REQ_DELEGATE, but MSDN documentation indicates that
  // ISC_REQ_MUTUAL_AUTH must also be set.
  if (can_delegate_)
    context_flags |= (ISC_REQ_DELEGATE | ISC_REQ_MUTUAL_AUTH);

  // This returns a token that is passed to the remote server.
  DWORD context_attribute;
  SECURITY_STATUS status = library_->InitializeSecurityContext(
      &cred_,  // phCredential
      ctxt_ptr,  // phContext
      const_cast<wchar_t *>(spn.c_str()),  // pszTargetName
      context_flags,  // fContextReq
      0,  // Reserved1 (must be 0)
      SECURITY_NATIVE_DREP,  // TargetDataRep
      in_buffer_desc_ptr,  // pInput
      0,  // Reserved2 (must be 0)
      &ctxt_,  // phNewContext
      &out_buffer_desc,  // pOutput
      &context_attribute,  // pfContextAttr
      NULL);  // ptsExpiry
  int rv = MapInitializeSecurityContextStatusToError(status);
  if (rv != OK) {
    ResetSecurityContext();
    free(out_buffer.pvBuffer);
    return rv;
  }
  if (!out_buffer.cbBuffer) {
    free(out_buffer.pvBuffer);
    out_buffer.pvBuffer = NULL;
  }
  *out_token = out_buffer.pvBuffer;
  *out_token_len = out_buffer.cbBuffer;
  return OK;
}

void SplitDomainAndUser(const string16& combined,
                        string16* domain,
                        string16* user) {
  // |combined| may be in the form "user" or "DOMAIN\user".
  // Separate the two parts if they exist.
  // TODO(cbentzel): I believe user@domain is also a valid form.
  size_t backslash_idx = combined.find(L'\\');
  if (backslash_idx == string16::npos) {
    domain->clear();
    *user = combined;
  } else {
    *domain = combined.substr(0, backslash_idx);
    *user = combined.substr(backslash_idx + 1);
  }
}

int DetermineMaxTokenLength(SSPILibrary* library,
                            const std::wstring& package,
                            ULONG* max_token_length) {
  DCHECK(library);
  DCHECK(max_token_length);
  PSecPkgInfo pkg_info = NULL;
  SECURITY_STATUS status = library->QuerySecurityPackageInfo(
      const_cast<wchar_t *>(package.c_str()), &pkg_info);
  int rv = MapQuerySecurityPackageInfoStatusToError(status);
  if (rv != OK)
    return rv;
  int token_length = pkg_info->cbMaxToken;
  status = library->FreeContextBuffer(pkg_info);
  rv = MapFreeContextBufferStatusToError(status);
  if (rv != OK)
    return rv;
  *max_token_length = token_length;
  return OK;
}

}  // namespace net
