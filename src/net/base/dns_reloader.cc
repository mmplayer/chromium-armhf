// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/dns_reloader.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD)

#include <resolv.h>

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_local_storage.h"
#include "net/base/network_change_notifier.h"

namespace {

// On Linux/BSD, changes to /etc/resolv.conf can go unnoticed thus resulting
// in DNS queries failing either because nameservers are unknown on startup
// or because nameserver info has changed as a result of e.g. connecting to
// a new network. Some distributions patch glibc to stat /etc/resolv.conf
// to try to automatically detect such changes but these patches are not
// universal and even patched systems such as Jaunty appear to need calls
// to res_ninit to reload the nameserver information in different threads.
//
// To fix this, on systems with FilePathWatcher support, we use
// NetworkChangeNotifier::DNSObserver to monitor /etc/resolv.conf to
// enable us to respond to DNS changes and reload the resolver state.
//
// OpenBSD does not have thread-safe res_ninit/res_nclose so we can't do
// the same trick there and most *BSD's don't yet have support for
// FilePathWatcher (but perhaps the new kqueue mac code just needs to be
// ported to *BSD to support that).

class DnsReloader : public net::NetworkChangeNotifier::DNSObserver {
 public:
  struct ReloadState {
    int resolver_generation;
  };

  // NetworkChangeNotifier::OnDNSChanged methods:
  virtual void OnDNSChanged() OVERRIDE {
    DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_IO);
    base::AutoLock l(lock_);
    resolver_generation_++;
  }

  void MaybeReload() {
    ReloadState* reload_state = static_cast<ReloadState*>(tls_index_.Get());
    base::AutoLock l(lock_);

    if (!reload_state) {
      reload_state = new ReloadState();
      reload_state->resolver_generation = resolver_generation_;
      res_ninit(&_res);
      tls_index_.Set(reload_state);
    } else if (reload_state->resolver_generation != resolver_generation_) {
      reload_state->resolver_generation = resolver_generation_;
      // It is safe to call res_nclose here since we know res_ninit will have
      // been called above.
      res_nclose(&_res);
      res_ninit(&_res);
    }
  }

  // Free the allocated state.
  static void SlotReturnFunction(void* data) {
    ReloadState* reload_state = static_cast<ReloadState*>(data);
    if (reload_state)
      res_nclose(&_res);
    delete reload_state;
  }

 private:
  DnsReloader() : resolver_generation_(0) {
    tls_index_.Initialize(SlotReturnFunction);
    net::NetworkChangeNotifier::AddDNSObserver(this);
  }

  ~DnsReloader() {
    NOTREACHED();  // LeakyLazyInstance is not destructed.
  }

  base::Lock lock_;  // Protects resolver_generation_.
  int resolver_generation_;
  friend struct base::DefaultLazyInstanceTraits<DnsReloader>;

  // We use thread local storage to identify which ReloadState to interact with.
  static base::ThreadLocalStorage::Slot tls_index_ ;

  DISALLOW_COPY_AND_ASSIGN(DnsReloader);
};

// A TLS slot to the ReloadState for the current thread.
// static
base::ThreadLocalStorage::Slot DnsReloader::tls_index_(
    base::LINKER_INITIALIZED);

base::LazyInstance<DnsReloader,
                   base::LeakyLazyInstanceTraits<DnsReloader> >
    g_dns_reloader(base::LINKER_INITIALIZED);

}  // namespace

namespace net {

void EnsureDnsReloaderInit() {
  DnsReloader* t ALLOW_UNUSED = g_dns_reloader.Pointer();
}

void DnsReloaderMaybeReload() {
  // This routine can be called by any of the DNS worker threads.
  DnsReloader* dns_reloader = g_dns_reloader.Pointer();
  dns_reloader->MaybeReload();
}

}  // namespace net

#endif  // defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD)
