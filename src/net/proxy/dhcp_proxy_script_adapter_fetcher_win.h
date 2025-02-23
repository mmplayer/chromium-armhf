// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_DHCP_SCRIPT_ADAPTER_FETCHER_WIN_H_
#define NET_PROXY_DHCP_SCRIPT_ADAPTER_FETCHER_WIN_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "base/threading/non_thread_safe.h"
#include "base/timer.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "googleurl/src/gurl.h"

namespace base {
class MessageLoopProxy;
}

namespace net {

class ProxyScriptFetcher;
class URLRequestContext;

// For a given adapter, this class takes care of first doing a DHCP lookup
// to get the PAC URL, then if there is one, trying to fetch it.
class NET_EXPORT_PRIVATE DhcpProxyScriptAdapterFetcher
    : public base::SupportsWeakPtr<DhcpProxyScriptAdapterFetcher>,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  // |url_request_context| must outlive DhcpProxyScriptAdapterFetcher.
  explicit DhcpProxyScriptAdapterFetcher(
      URLRequestContext* url_request_context);
  virtual ~DhcpProxyScriptAdapterFetcher();

  // Starts a fetch.  On completion (but not cancellation), |callback|
  // will be invoked with the network error indicating success or failure
  // of fetching a DHCP-configured PAC file on this adapter.
  //
  // On completion, results can be obtained via |GetPacScript()|, |GetPacURL()|.
  //
  // You may only call Fetch() once on a given instance of
  // DhcpProxyScriptAdapterFetcher.
  virtual void Fetch(const std::string& adapter_name,
                     OldCompletionCallback* callback);

  // Cancels the fetch on this adapter.
  virtual void Cancel();

  // Returns true if in the FINISH state (not CANCEL).
  virtual bool DidFinish() const;

  // Returns the network error indicating the result of the fetch. Will
  // return IO_PENDING until the fetch is complete or cancelled. This is
  // the same network error passed to the |callback| provided to |Fetch()|.
  virtual int GetResult() const;

  // Returns the contents of the PAC file retrieved.  Only valid if
  // |IsComplete()| is true.  Returns the empty string if |GetResult()|
  // returns anything other than OK.
  virtual string16 GetPacScript() const;

  // Returns the PAC URL retrieved from DHCP.  Only guaranteed to be
  // valid if |IsComplete()| is true.  Returns an empty URL if no URL was
  // configured in DHCP.  May return a valid URL even if |result()| does
  // not return OK (this would indicate that we found a URL configured in
  // DHCP but failed to download it).
  virtual GURL GetPacURL() const;

  // Returns the PAC URL configured in DHCP for the given |adapter_name|, or
  // the empty string if none is configured.
  //
  // This function executes synchronously due to limitations of the Windows
  // DHCP client API.
  static std::string GetPacURLFromDhcp(const std::string& adapter_name);

 protected:
  // This is the state machine for fetching from a given adapter.
  //
  // The state machine goes from START->WAIT_DHCP when it starts
  // a worker thread to fetch the PAC URL from DHCP.
  //
  // In state WAIT_DHCP, if the DHCP query finishes and has no URL, it
  // moves to state FINISH.  If there is a URL, it starts a
  // ProxyScriptFetcher to fetch it and moves to state WAIT_URL.
  //
  // It goes from WAIT_URL->FINISH when the ProxyScriptFetcher completes.
  //
  // In state FINISH, completion is indicated to the outer class, with
  // the results of the fetch if a PAC script was successfully fetched.
  //
  // In state WAIT_DHCP, our timeout occurring can push us to FINISH.
  //
  // In any state except FINISH, a call to Cancel() will move to state
  // CANCEL and cause all outstanding work to be cancelled or its
  // results ignored when available.
  enum State {
    STATE_START,
    STATE_WAIT_DHCP,
    STATE_WAIT_URL,
    STATE_FINISH,
    STATE_CANCEL,
  };

  State state() const;

  // This inner class encapsulates work done on a worker pool thread.
  // By using a separate object, we can keep the main object completely
  // thread safe and let it be non-refcounted.
  class NET_EXPORT_PRIVATE DhcpQuery
      : public base::RefCountedThreadSafe<DhcpQuery> {
   public:
    DhcpQuery();
    virtual ~DhcpQuery();

    // This method should run on a worker pool thread, via PostTaskAndReply.
    // After it has run, the |url()| method on this object will return the
    // URL retrieved.
    void GetPacURLForAdapter(const std::string& adapter_name);

    // Returns the URL retrieved for the given adapter, once the task has run.
    const std::string& url() const;

   protected:
    // Virtual method introduced to allow unit testing.
    virtual std::string ImplGetPacURLFromDhcp(const std::string& adapter_name);

   private:
    // The URL retrieved for the given adapter.
    std::string url_;

    DISALLOW_COPY_AND_ASSIGN(DhcpQuery);
  };

  // Virtual methods introduced to allow unit testing.
  virtual ProxyScriptFetcher* ImplCreateScriptFetcher();
  virtual DhcpQuery* ImplCreateDhcpQuery();
  virtual base::TimeDelta ImplGetTimeout() const;

 private:
  // Event/state transition handlers
  void OnDhcpQueryDone(scoped_refptr<DhcpQuery> dhcp_query);
  void OnTimeout();
  void OnFetcherDone(int result);
  void TransitionToFinish();

  // Current state of this state machine.
  State state_;

  // A network error indicating result of operation.
  int result_;

  // Empty string or the PAC script downloaded.
  string16 pac_script_;

  // Empty URL or the PAC URL configured in DHCP.
  GURL pac_url_;

  // Callback to let our client know we're done. Invalid in states
  // START, FINISH and CANCEL.
  OldCompletionCallback* callback_;

  // Fetcher to retrieve PAC files once URL is known.
  scoped_ptr<ProxyScriptFetcher> script_fetcher_;

  // Callback from the script fetcher.
  OldCompletionCallbackImpl<DhcpProxyScriptAdapterFetcher>
      script_fetcher_callback_;

  // Implements a timeout on the call to the Win32 DHCP API.
  base::OneShotTimer<DhcpProxyScriptAdapterFetcher> wait_timer_;

  scoped_refptr<URLRequestContext> url_request_context_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DhcpProxyScriptAdapterFetcher);
};

}  // namespace net

#endif  // NET_PROXY_DHCP_SCRIPT_ADAPTER_FETCHER_WIN_H_
