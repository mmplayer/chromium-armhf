// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ORIGIN_BOUND_CERT_STORE_H_
#define NET_BASE_ORIGIN_BOUND_CERT_STORE_H_
#pragma once

#include <string>
#include <vector>

#include "net/base/net_export.h"

namespace net {

// An interface for storing and retrieving origin bound certs. Origin bound
// certificates are specified in
// http://balfanz.github.com/tls-obc-spec/draft-balfanz-tls-obc-00.html.

// Owned only by a single OriginBoundCertService object, which is responsible
// for deleting it.

class NET_EXPORT OriginBoundCertStore {
 public:
  // Used by GetAllOriginBoundCerts.
  struct OriginBoundCertInfo {
    std::string origin;  // Origin, for instance "https://www.verisign.com:443".
    std::string private_key;  // DER-encoded PrivateKeyInfo struct.
    std::string cert;  // DER-encoded certificate.
  };

  virtual ~OriginBoundCertStore() {}

  // TODO(rkn): Specify certificate type (RSA or DSA).
  // TODO(rkn): File I/O may be required, so this should have an asynchronous
  // interface.
  // Returns true on success. |private_key_result| stores a DER-encoded
  // PrivateKeyInfo struct and |cert_result| stores a DER-encoded
  // certificate. Returns false if no origin bound cert exists for the
  // specified origin.
  virtual bool GetOriginBoundCert(const std::string& origin,
                                  std::string* private_key_result,
                                  std::string* cert_result) = 0;

  // Adds an origin bound cert and the corresponding private key to the store.
  virtual void SetOriginBoundCert(const std::string& origin,
                                  const std::string& private_key,
                                  const std::string& cert) = 0;

  // Removes an origin bound cert and the corresponding private key from the
  // store.
  virtual void DeleteOriginBoundCert(const std::string& origin) = 0;

  // Removes all origin bound certs and the corresponding private keys from
  // the store.
  virtual void DeleteAll() = 0;

  // Returns all origin bound certs and the corresponding private keys.
  virtual void GetAllOriginBoundCerts(
      std::vector<OriginBoundCertInfo>* origin_bound_certs) = 0;

  // Returns the number of certs in the store.
  // Public only for unit testing.
  virtual int GetCertCount() = 0;
};

}  // namespace net

#endif  // NET_BASE_ORIGIN_BOUND_CERT_STORE_H_
