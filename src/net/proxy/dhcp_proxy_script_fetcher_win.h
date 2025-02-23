// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_DHCP_PROXY_SCRIPT_FETCHER_WIN_H_
#define NET_PROXY_DHCP_PROXY_SCRIPT_FETCHER_WIN_H_
#pragma once

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop_proxy.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"
#include "base/timer.h"
#include "net/proxy/dhcp_proxy_script_fetcher.h"

namespace net {

class DhcpProxyScriptAdapterFetcher;
class URLRequestContext;

// Windows-specific implementation.
class NET_EXPORT_PRIVATE DhcpProxyScriptFetcherWin
    : public DhcpProxyScriptFetcher,
      public base::SupportsWeakPtr<DhcpProxyScriptFetcherWin>,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  // Creates a DhcpProxyScriptFetcherWin that issues requests through
  // |url_request_context|. |url_request_context| must remain valid for
  // the lifetime of DhcpProxyScriptFetcherWin.
  explicit DhcpProxyScriptFetcherWin(URLRequestContext* url_request_context);
  virtual ~DhcpProxyScriptFetcherWin();

  // DhcpProxyScriptFetcher implementation.
  int Fetch(string16* utf16_text, OldCompletionCallback* callback) OVERRIDE;
  void Cancel() OVERRIDE;
  const GURL& GetPacURL() const OVERRIDE;
  std::string GetFetcherName() const OVERRIDE;

  // Sets |adapter_names| to contain the name of each network adapter on
  // this machine that has DHCP enabled and is not a loop-back adapter. Returns
  // false on error.
  static bool GetCandidateAdapterNames(std::set<std::string>* adapter_names);

 protected:
  int num_pending_fetchers() const;

  URLRequestContext* url_request_context() const;

  // This inner class encapsulate work done on a worker pool thread.
  // The class calls GetCandidateAdapterNames, which can take a couple of
  // hundred milliseconds.
  class NET_EXPORT_PRIVATE AdapterQuery
      : public base::RefCountedThreadSafe<AdapterQuery> {
   public:
    AdapterQuery();
    virtual ~AdapterQuery();

    // This is the method that runs on the worker pool thread.
    void GetCandidateAdapterNames();

    // This set is valid after GetCandidateAdapterNames has
    // been run. Its lifetime is scoped by this object.
    const std::set<std::string>& adapter_names() const;

   protected:
    // Virtual method introduced to allow unit testing.
    virtual bool ImplGetCandidateAdapterNames(
        std::set<std::string>* adapter_names);

   private:
    // This is constructed on the originating thread, then used on the
    // worker thread, then used again on the originating thread only when
    // the task has completed on the worker thread. No locking required.
    std::set<std::string> adapter_names_;

    DISALLOW_COPY_AND_ASSIGN(AdapterQuery);
  };

  // Virtual methods introduced to allow unit testing.
  virtual DhcpProxyScriptAdapterFetcher* ImplCreateAdapterFetcher();
  virtual AdapterQuery* ImplCreateAdapterQuery();
  virtual int ImplGetMaxWaitMs();
  virtual void ImplOnGetCandidateAdapterNamesDone() {}

 private:
  // Event/state transition handlers
  void CancelImpl();
  void OnGetCandidateAdapterNamesDone(scoped_refptr<AdapterQuery> query);
  void OnFetcherDone(int result);
  void OnWaitTimer();
  void TransitionToDone();

  // This is the outer state machine for fetching PAC configuration from
  // DHCP.  It relies for sub-states on the state machine of the
  // DhcpProxyScriptAdapterFetcher class.
  //
  // The goal of the implementation is to the following work in parallel
  // for all network adapters that are using DHCP:
  // a) Try to get the PAC URL configured in DHCP;
  // b) If one is configured, try to fetch the PAC URL.
  // c) Once this is done for all adapters, or a timeout has passed after
  //    it has completed for the fastest adapter, return the PAC file
  //    available for the most preferred network adapter, if any.
  //
  // The state machine goes from START->WAIT_ADAPTERS when it starts a
  // worker thread to get the list of adapters with DHCP enabled.
  // It then goes from WAIT_ADAPTERS->NO_RESULTS when it creates
  // and starts an DhcpProxyScriptAdapterFetcher for each adapter.  It goes
  // from NO_RESULTS->SOME_RESULTS when it gets the first result; at this
  // point a wait timer is started.  It goes from SOME_RESULTS->DONE in
  // two cases: All results are known, or the wait timer expired.  A call
  // to Cancel() will also go straight to DONE from any state.  Any
  // way the DONE state is entered, we will at that point cancel any
  // outstanding work and return the best known PAC script or the empty
  // string.
  //
  // The state machine is reset for each Fetch(), a call to which is
  // only valid in states START and DONE, as only one Fetch() is
  // allowed to be outstanding at any given time.
  enum State {
    STATE_START,
    STATE_WAIT_ADAPTERS,
    STATE_NO_RESULTS,
    STATE_SOME_RESULTS,
    STATE_DONE,
  };

  // Current state of this state machine.
  State state_;

  // Vector, in Windows' network adapter preference order, of
  // DhcpProxyScriptAdapterFetcher objects that are or were attempting
  // to fetch a PAC file based on DHCP configuration.
  typedef ScopedVector<DhcpProxyScriptAdapterFetcher> FetcherVector;
  FetcherVector fetchers_;

  // Callback invoked when any fetcher completes.
  OldCompletionCallbackImpl<DhcpProxyScriptFetcherWin> fetcher_callback_;

  // Number of fetchers we are waiting for.
  int num_pending_fetchers_;

  // Lets our client know we're done. Not valid in states START or DONE.
  OldCompletionCallback* client_callback_;

  // Pointer to string we will write results to. Not valid in states
  // START and DONE.
  string16* destination_string_;

  // PAC URL retrieved from DHCP, if any. Valid only in state STATE_DONE.
  GURL pac_url_;

  base::OneShotTimer<DhcpProxyScriptFetcherWin> wait_timer_;

  scoped_refptr<URLRequestContext> url_request_context_;

  // NULL or the AdapterQuery currently in flight.
  scoped_refptr<AdapterQuery> last_query_;

  // Time |Fetch()| was last called, 0 if never.
  base::TimeTicks fetch_start_time_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DhcpProxyScriptFetcherWin);
};

}  // namespace net

#endif  // NET_PROXY_DHCP_PROXY_SCRIPT_FETCHER_WIN_H_
