// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_GSSAPI_POSIX_H_
#define NET_HTTP_HTTP_AUTH_GSSAPI_POSIX_H_
#pragma once

#include <gssapi.h>

#include <string>

#include "base/gtest_prod_util.h"
#include "base/native_library.h"
#include "base/string16.h"
#include "net/base/net_export.h"
#include "net/http/http_auth.h"

namespace net {

NET_EXPORT_PRIVATE extern gss_OID CHROME_GSS_C_NT_HOSTBASED_SERVICE_X;
NET_EXPORT_PRIVATE extern gss_OID CHROME_GSS_C_NT_HOSTBASED_SERVICE;
NET_EXPORT_PRIVATE extern gss_OID CHROME_GSS_KRB5_MECH_OID_DESC;

// GSSAPILibrary is introduced so unit tests can mock the calls to the GSSAPI
// library. The default implementation attempts to load one of the standard
// GSSAPI library implementations, then simply passes the arguments on to
// that implementation.
class NET_EXPORT_PRIVATE GSSAPILibrary {
 public:
  virtual ~GSSAPILibrary() {}

  // Initializes the library, including any necessary dynamic libraries.
  // This is done separately from construction (which happens at startup time)
  // in order to delay work until the class is actually needed.
  virtual bool Init() = 0;

  // These methods match the ones in the GSSAPI library.
  virtual OM_uint32 import_name(
      OM_uint32* minor_status,
      const gss_buffer_t input_name_buffer,
      const gss_OID input_name_type,
      gss_name_t* output_name) = 0;
  virtual OM_uint32 release_name(
      OM_uint32* minor_status,
      gss_name_t* input_name) = 0;
  virtual OM_uint32 release_buffer(
      OM_uint32* minor_status,
      gss_buffer_t buffer) = 0;
  virtual OM_uint32 display_name(
      OM_uint32* minor_status,
      const gss_name_t input_name,
      gss_buffer_t output_name_buffer,
      gss_OID* output_name_type) = 0;
  virtual OM_uint32 display_status(
      OM_uint32* minor_status,
      OM_uint32 status_value,
      int status_type,
      const gss_OID mech_type,
      OM_uint32* message_contex,
      gss_buffer_t status_string) = 0;
  virtual OM_uint32 init_sec_context(
      OM_uint32* minor_status,
      const gss_cred_id_t initiator_cred_handle,
      gss_ctx_id_t* context_handle,
      const gss_name_t target_name,
      const gss_OID mech_type,
      OM_uint32 req_flags,
      OM_uint32 time_req,
      const gss_channel_bindings_t input_chan_bindings,
      const gss_buffer_t input_token,
      gss_OID* actual_mech_type,
      gss_buffer_t output_token,
      OM_uint32* ret_flags,
      OM_uint32* time_rec) = 0;
  virtual OM_uint32 wrap_size_limit(
      OM_uint32* minor_status,
      const gss_ctx_id_t context_handle,
      int conf_req_flag,
      gss_qop_t qop_req,
      OM_uint32 req_output_size,
      OM_uint32* max_input_size) = 0;
  virtual OM_uint32 delete_sec_context(
      OM_uint32* minor_status,
      gss_ctx_id_t* context_handle,
      gss_buffer_t output_token) = 0;
  virtual OM_uint32 inquire_context(
      OM_uint32* minor_status,
      const gss_ctx_id_t context_handle,
      gss_name_t* src_name,
      gss_name_t* targ_name,
      OM_uint32* lifetime_rec,
      gss_OID* mech_type,
      OM_uint32* ctx_flags,
      int* locally_initiated,
      int* open) = 0;

};

// GSSAPISharedLibrary class is defined here so that unit tests can access it.
class NET_EXPORT_PRIVATE GSSAPISharedLibrary : public GSSAPILibrary {
 public:
  // If |gssapi_library_name| is empty, hard-coded default library names are
  // used.
  explicit GSSAPISharedLibrary(const std::string& gssapi_library_name);
  virtual ~GSSAPISharedLibrary();

  // GSSAPILibrary methods:
  virtual bool Init();
  virtual OM_uint32 import_name(
      OM_uint32* minor_status,
      const gss_buffer_t input_name_buffer,
      const gss_OID input_name_type,
      gss_name_t* output_name);
  virtual OM_uint32 release_name(
      OM_uint32* minor_status,
      gss_name_t* input_name);
  virtual OM_uint32 release_buffer(
      OM_uint32* minor_status,
      gss_buffer_t buffer);
  virtual OM_uint32 display_name(
      OM_uint32* minor_status,
      const gss_name_t input_name,
      gss_buffer_t output_name_buffer,
      gss_OID* output_name_type);
  virtual OM_uint32 display_status(
      OM_uint32* minor_status,
      OM_uint32 status_value,
      int status_type,
      const gss_OID mech_type,
      OM_uint32* message_contex,
      gss_buffer_t status_string);
  virtual OM_uint32 init_sec_context(
      OM_uint32* minor_status,
      const gss_cred_id_t initiator_cred_handle,
      gss_ctx_id_t* context_handle,
      const gss_name_t target_name,
      const gss_OID mech_type,
      OM_uint32 req_flags,
      OM_uint32 time_req,
      const gss_channel_bindings_t input_chan_bindings,
      const gss_buffer_t input_token,
      gss_OID* actual_mech_type,
      gss_buffer_t output_token,
      OM_uint32* ret_flags,
      OM_uint32* time_rec);
  virtual OM_uint32 wrap_size_limit(
      OM_uint32* minor_status,
      const gss_ctx_id_t context_handle,
      int conf_req_flag,
      gss_qop_t qop_req,
      OM_uint32 req_output_size,
      OM_uint32* max_input_size);
  virtual OM_uint32 delete_sec_context(
      OM_uint32* minor_status,
      gss_ctx_id_t* context_handle,
      gss_buffer_t output_token);
  virtual OM_uint32 inquire_context(
      OM_uint32* minor_status,
      const gss_ctx_id_t context_handle,
      gss_name_t* src_name,
      gss_name_t* targ_name,
      OM_uint32* lifetime_rec,
      gss_OID* mech_type,
      OM_uint32* ctx_flags,
      int* locally_initiated,
      int* open);

 private:
  typedef typeof(&gss_import_name) gss_import_name_type;
  typedef typeof(&gss_release_name) gss_release_name_type;
  typedef typeof(&gss_release_buffer) gss_release_buffer_type;
  typedef typeof(&gss_display_name) gss_display_name_type;
  typedef typeof(&gss_display_status) gss_display_status_type;
  typedef typeof(&gss_init_sec_context) gss_init_sec_context_type;
  typedef typeof(&gss_wrap_size_limit) gss_wrap_size_limit_type;
  typedef typeof(&gss_delete_sec_context) gss_delete_sec_context_type;
  typedef typeof(&gss_inquire_context) gss_inquire_context_type;

  FRIEND_TEST_ALL_PREFIXES(HttpAuthGSSAPIPOSIXTest, GSSAPIStartup);

  bool InitImpl();
  // Finds a usable dynamic library for GSSAPI and loads it.  The criteria are:
  //   1. The library must exist.
  //   2. The library must export the functions we need.
  base::NativeLibrary LoadSharedLibrary();
  bool BindMethods(base::NativeLibrary lib);

  bool initialized_;

  std::string gssapi_library_name_;
  // Need some way to invalidate the library.
  base::NativeLibrary gssapi_library_;

  // Function pointers
  gss_import_name_type import_name_;
  gss_release_name_type release_name_;
  gss_release_buffer_type release_buffer_;
  gss_display_name_type display_name_;
  gss_display_status_type display_status_;
  gss_init_sec_context_type init_sec_context_;
  gss_wrap_size_limit_type wrap_size_limit_;
  gss_delete_sec_context_type delete_sec_context_;
  gss_inquire_context_type inquire_context_;
};

// ScopedSecurityContext releases a gss_ctx_id_t when it goes out of
// scope.
class ScopedSecurityContext {
 public:
  explicit ScopedSecurityContext(GSSAPILibrary* gssapi_lib);
  ~ScopedSecurityContext();

  gss_ctx_id_t get() const { return security_context_; }
  gss_ctx_id_t* receive() { return &security_context_; }

 private:
  gss_ctx_id_t security_context_;
  GSSAPILibrary* gssapi_lib_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSecurityContext);
};


// TODO(ahendrickson): Share code with HttpAuthSSPI.
class NET_EXPORT_PRIVATE HttpAuthGSSAPI {
 public:
  HttpAuthGSSAPI(GSSAPILibrary* library,
                 const std::string& scheme,
                 const gss_OID gss_oid);
  ~HttpAuthGSSAPI();

  bool Init();

  bool NeedsIdentity() const;

  bool AllowsExplicitCredentials() const;

  HttpAuth::AuthorizationResult ParseChallenge(
      HttpAuth::ChallengeTokenizer* tok);

  // Generates an authentication token.
  // The return value is an error code. If it's not |OK|, the value of
  // |*auth_token| is unspecified.
  // |spn| is the Service Principal Name of the server that the token is
  // being generated for.
  // If this is the first round of a multiple round scheme, credentials are
  // obtained using |*username| and |*password|. If |username| and |password|
  // are NULL, the default credentials are used instead.
  int GenerateAuthToken(const string16* username,
                        const string16* password,
                        const std::wstring& spn,
                        std::string* auth_token);

  // Delegation is allowed on the Kerberos ticket. This allows certain servers
  // to act as the user, such as an IIS server retrieiving data from a
  // Kerberized MSSQL server.
  void Delegate();

 private:
  int GetNextSecurityToken(const std::wstring& spn,
                           gss_buffer_t in_token,
                           gss_buffer_t out_token);

  std::string scheme_;
  gss_OID gss_oid_;
  GSSAPILibrary* library_;
  std::string decoded_server_auth_token_;
  ScopedSecurityContext scoped_sec_context_;
  bool can_delegate_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_GSSAPI_POSIX_H_
