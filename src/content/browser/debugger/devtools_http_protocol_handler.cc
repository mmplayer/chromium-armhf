// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/debugger/devtools_http_protocol_handler.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/browser_thread.h"
#include "content/browser/debugger/devtools_client_host.h"
#include "content/browser/debugger/devtools_manager.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/common/devtools_messages.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/base/io_buffer.h"
#include "net/server/http_server_request_info.h"
#include "net/url_request/url_request_context.h"

const int kBufferSize = 16 * 1024;

namespace {

// An internal implementation of DevToolsClientHost that delegates
// messages sent for DevToolsClient to a DebuggerShell instance.
class DevToolsClientHostImpl : public DevToolsClientHost {
 public:
  DevToolsClientHostImpl(
      net::HttpServer* server,
      int connection_id)
      : server_(server),
        connection_id_(connection_id) {
  }
  ~DevToolsClientHostImpl() {}

  // DevToolsClientHost interface
  virtual void InspectedTabClosing() {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        NewRunnableMethod(server_,
                          &net::HttpServer::Close,
                          connection_id_));
  }

  virtual void SendMessageToClient(const IPC::Message& msg) {
    IPC_BEGIN_MESSAGE_MAP(DevToolsClientHostImpl, msg)
      IPC_MESSAGE_HANDLER(DevToolsClientMsg_DispatchOnInspectorFrontend,
                          OnDispatchOnInspectorFrontend);
      IPC_MESSAGE_UNHANDLED_ERROR()
    IPC_END_MESSAGE_MAP()
  }

  virtual void TabReplaced(TabContents* new_tab) {
  }

  void NotifyCloseListener() {
    DevToolsClientHost::NotifyCloseListener();
  }
 private:
  // Message handling routines
  void OnDispatchOnInspectorFrontend(const std::string& data) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        NewRunnableMethod(server_,
                          &net::HttpServer::SendOverWebSocket,
                          connection_id_,
                          data));
  }

  virtual void FrameNavigating(const std::string& url) {}
  net::HttpServer* server_;
  int connection_id_;
};

static int next_id = 1;

class TabContentsIDHelper : public TabContentsObserver {
 public:

  static int GetID(TabContents* tab) {
    TabContentsToIdMap::iterator it = tabcontents_to_id_.find(tab);
    if (it != tabcontents_to_id_.end())
      return it->second;
    TabContentsIDHelper* wrapper = new TabContentsIDHelper(tab);
    return wrapper->id_;
  }

  static TabContents* GetTabContents(int id) {
    IdToTabContentsMap::iterator it = id_to_tabcontents_.find(id);
    if (it != id_to_tabcontents_.end())
      return it->second;
    return NULL;
  }

 private:
  explicit TabContentsIDHelper(TabContents* tab)
      : TabContentsObserver(tab),
        id_(next_id++) {
    id_to_tabcontents_[id_] = tab;
    tabcontents_to_id_[tab] = id_;
  }

  virtual ~TabContentsIDHelper() {}

  virtual void TabContentsDestroyed(TabContents* tab) {
    id_to_tabcontents_.erase(id_);
    tabcontents_to_id_.erase(tab);
    delete this;
  }

  int id_;
  typedef std::map<int, TabContents*> IdToTabContentsMap;
  static IdToTabContentsMap id_to_tabcontents_;
  typedef std::map<TabContents*, int> TabContentsToIdMap;
  static TabContentsToIdMap tabcontents_to_id_;
};

TabContentsIDHelper::IdToTabContentsMap TabContentsIDHelper::id_to_tabcontents_;
TabContentsIDHelper::TabContentsToIdMap TabContentsIDHelper::tabcontents_to_id_;

}  // namespace

// static
scoped_refptr<DevToolsHttpProtocolHandler> DevToolsHttpProtocolHandler::Start(
    const std::string& ip,
    int port,
    const std::string& frontend_url,
    Delegate* delegate) {
  scoped_refptr<DevToolsHttpProtocolHandler> http_handler =
      new DevToolsHttpProtocolHandler(ip, port, frontend_url, delegate);
  http_handler->Start();
  return http_handler;
}

DevToolsHttpProtocolHandler::~DevToolsHttpProtocolHandler() {
  // Stop() must be called prior to this being called
  DCHECK(server_.get() == NULL);
}

void DevToolsHttpProtocolHandler::Start() {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &DevToolsHttpProtocolHandler::Init));
}

void DevToolsHttpProtocolHandler::Stop() {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &DevToolsHttpProtocolHandler::Teardown));
}

void DevToolsHttpProtocolHandler::OnHttpRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  if (info.path == "/json") {
    // Pages discovery json request.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        NewRunnableMethod(this,
                          &DevToolsHttpProtocolHandler::OnJsonRequestUI,
                          connection_id,
                          info));
    return;
  }

  // Proxy static files from chrome-devtools://devtools/*.
  net::URLRequestContext* request_context = delegate_->GetURLRequestContext();
  if (!request_context) {
    server_->Send404(connection_id);
    return;
  }

  if (info.path == "" || info.path == "/") {
    std::string response = delegate_->GetDiscoveryPageHTML();
    server_->Send200(connection_id, response, "text/html; charset=UTF-8");
    return;
  }

  net::URLRequest* request;

  if (info.path.find("/devtools/") == 0) {
    request = new net::URLRequest(GURL("chrome-devtools:/" + info.path), this);
  } else if (info.path.find("/thumb/") == 0) {
    request = new net::URLRequest(GURL("chrome:/" + info.path), this);
  } else {
    server_->Send404(connection_id);
    return;
  }

  Bind(request, connection_id);
  request->set_context(request_context);
  request->Start();
}

void DevToolsHttpProtocolHandler::OnWebSocketRequest(
    int connection_id,
    const net::HttpServerRequestInfo& request) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableMethod(
          this,
          &DevToolsHttpProtocolHandler::OnWebSocketRequestUI,
          connection_id,
          request));
}

void DevToolsHttpProtocolHandler::OnWebSocketMessage(
    int connection_id,
    const std::string& data) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableMethod(
          this,
          &DevToolsHttpProtocolHandler::OnWebSocketMessageUI,
          connection_id,
          data));
}

void DevToolsHttpProtocolHandler::OnClose(int connection_id) {
  ConnectionToRequestsMap::iterator it =
      connection_to_requests_io_.find(connection_id);
  if (it != connection_to_requests_io_.end()) {
    // Dispose delegating socket.
    for (std::set<net::URLRequest*>::iterator it2 = it->second.begin();
         it2 != it->second.end(); ++it2) {
      net::URLRequest* request = *it2;
      request->Cancel();
      request_to_connection_io_.erase(request);
      request_to_buffer_io_.erase(request);
      delete request;
    }
    connection_to_requests_io_.erase(connection_id);
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableMethod(
          this,
          &DevToolsHttpProtocolHandler::OnCloseUI,
          connection_id));
}

struct PageInfo
{
  int id;
  std::string url;
  bool attached;
  std::string title;
  std::string thumbnail_url;
  std::string favicon_url;
};
typedef std::vector<PageInfo> PageList;

static PageList GeneratePageList(
    DevToolsHttpProtocolHandler::Delegate* delegate,
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  typedef DevToolsHttpProtocolHandler::InspectableTabs Tabs;
  Tabs inspectable_tabs = delegate->GetInspectableTabs();

  PageList page_list;
  for (Tabs::iterator it = inspectable_tabs.begin();
       it != inspectable_tabs.end(); ++it) {

    TabContents* tab_contents = *it;
    NavigationController& controller = tab_contents->controller();

    NavigationEntry* entry = controller.GetActiveEntry();
    if (entry == NULL || !entry->url().is_valid())
      continue;

    DevToolsClientHost* client_host = DevToolsManager::GetInstance()->
        GetDevToolsClientHostFor(tab_contents->render_view_host());
    PageInfo page_info;
    page_info.id = TabContentsIDHelper::GetID(tab_contents);
    page_info.attached = client_host != NULL;
    page_info.url = entry->url().spec();
    page_info.title = UTF16ToUTF8(net::EscapeForHTML(entry->title()));
    page_info.thumbnail_url = "/thumb/" + entry->url().spec();
    page_info.favicon_url = entry->favicon().url().spec();
    page_list.push_back(page_info);
  }
  return page_list;
}

void DevToolsHttpProtocolHandler::OnJsonRequestUI(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  PageList page_list = GeneratePageList(delegate_.get(),
                                        connection_id, info);
  ListValue json_pages_list;
  std::string host = info.headers["Host"];
  for (PageList::iterator i = page_list.begin();
       i != page_list.end(); ++i) {

    DictionaryValue* page_info = new DictionaryValue;
    json_pages_list.Append(page_info);
    page_info->SetString("title", i->title);
    page_info->SetString("url", i->url);
    page_info->SetString("thumbnailUrl", i->thumbnail_url);
    page_info->SetString("faviconUrl", i->favicon_url);
    if (!i->attached) {
      page_info->SetString("webSocketDebuggerUrl",
                           base::StringPrintf("ws://%s/devtools/page/%d",
                                              host.c_str(),
                                              i->id));
      page_info->SetString("devtoolsFrontendUrl",
                           base::StringPrintf("%s?host=%s&page=%d",
                                              overridden_frontend_url_.c_str(),
                                              host.c_str(),
                                              i->id));
    }
  }

  std::string response;
  base::JSONWriter::Write(&json_pages_list, true, &response);
  Send200(connection_id, response, "application/json; charset=UTF-8");
}

void DevToolsHttpProtocolHandler::OnWebSocketRequestUI(
    int connection_id,
    const net::HttpServerRequestInfo& request) {
  std::string prefix = "/devtools/page/";
  size_t pos = request.path.find(prefix);
  if (pos != 0) {
    Send404(connection_id);
    return;
  }
  std::string page_id = request.path.substr(prefix.length());
  int id = 0;
  if (!base::StringToInt(page_id, &id)) {
    Send500(connection_id, "Invalid page id: " + page_id);
    return;
  }

  TabContents* tab_contents = TabContentsIDHelper::GetTabContents(id);
  if (tab_contents == NULL) {
    Send500(connection_id, "No such page id: " + page_id);
    return;
  }

  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->GetDevToolsClientHostFor(tab_contents->render_view_host())) {
    Send500(connection_id, "Page with given id is being inspected: " + page_id);
    return;
  }

  DevToolsClientHostImpl* client_host =
      new DevToolsClientHostImpl(server_, connection_id);
  connection_to_client_host_ui_[connection_id] = client_host;

  manager->RegisterDevToolsClientHostFor(
      tab_contents->render_view_host(),
      client_host);
  manager->ForwardToDevToolsAgent(
      client_host,
      DevToolsAgentMsg_FrontendLoaded(MSG_ROUTING_NONE));

  AcceptWebSocket(connection_id, request);
}

void DevToolsHttpProtocolHandler::OnWebSocketMessageUI(
    int connection_id,
    const std::string& data) {
  ConnectionToClientHostMap::iterator it =
      connection_to_client_host_ui_.find(connection_id);
  if (it == connection_to_client_host_ui_.end())
    return;

  DevToolsManager* manager = DevToolsManager::GetInstance();
  manager->ForwardToDevToolsAgent(
      it->second,
      DevToolsAgentMsg_DispatchOnInspectorBackend(MSG_ROUTING_NONE, data));
}

void DevToolsHttpProtocolHandler::OnCloseUI(int connection_id) {
  ConnectionToClientHostMap::iterator it =
      connection_to_client_host_ui_.find(connection_id);
  if (it != connection_to_client_host_ui_.end()) {
    DevToolsClientHostImpl* client_host =
        static_cast<DevToolsClientHostImpl*>(it->second);
    client_host->NotifyCloseListener();
    delete client_host;
    connection_to_client_host_ui_.erase(connection_id);
  }
}

void DevToolsHttpProtocolHandler::OnResponseStarted(net::URLRequest* request) {
  RequestToSocketMap::iterator it = request_to_connection_io_.find(request);
  if (it == request_to_connection_io_.end())
    return;

  int connection_id = it->second;

  std::string content_type;
  request->GetMimeType(&content_type);

  if (request->status().is_success()) {
    server_->Send(connection_id,
                  base::StringPrintf("HTTP/1.1 200 OK\r\n"
                                     "Content-Type:%s\r\n"
                                     "Transfer-Encoding: chunked\r\n"
                                     "\r\n",
                                     content_type.c_str()));
  } else {
    server_->Send404(connection_id);
  }

  int bytes_read = 0;
  // Some servers may treat HEAD requests as GET requests.  To free up the
  // network connection as soon as possible, signal that the request has
  // completed immediately, without trying to read any data back (all we care
  // about is the response code and headers, which we already have).
  net::IOBuffer* buffer = request_to_buffer_io_[request].get();
  if (request->status().is_success())
    request->Read(buffer, kBufferSize, &bytes_read);
  OnReadCompleted(request, bytes_read);
}

void DevToolsHttpProtocolHandler::OnReadCompleted(net::URLRequest* request,
                                                  int bytes_read) {
  RequestToSocketMap::iterator it = request_to_connection_io_.find(request);
  if (it == request_to_connection_io_.end())
    return;

  int connection_id = it->second;

  net::IOBuffer* buffer = request_to_buffer_io_[request].get();
  do {
    if (!request->status().is_success() || bytes_read <= 0)
      break;
    std::string chunk_size = base::StringPrintf("%X\r\n", bytes_read);
    server_->Send(connection_id, chunk_size);
    server_->Send(connection_id, buffer->data(), bytes_read);
    server_->Send(connection_id, "\r\n");
  } while (request->Read(buffer, kBufferSize, &bytes_read));


  // See comments re: HEAD requests in OnResponseStarted().
  if (!request->status().is_io_pending()) {
    server_->Send(connection_id, "0\r\n\r\n");
    RequestCompleted(request);
  }
}

DevToolsHttpProtocolHandler::DevToolsHttpProtocolHandler(
    const std::string& ip,
    int port,
    const std::string& frontend_host,
    Delegate* delegate)
    : ip_(ip),
      port_(port),
      overridden_frontend_url_(frontend_host),
      delegate_(delegate) {
  if (overridden_frontend_url_.empty())
      overridden_frontend_url_ = "/devtools/devtools.html";
}

void DevToolsHttpProtocolHandler::Init() {
  server_ = new net::HttpServer(ip_, port_, this);
}

// Run on I/O thread
void DevToolsHttpProtocolHandler::Teardown() {
  server_ = NULL;
}

void DevToolsHttpProtocolHandler::Bind(net::URLRequest* request,
                                       int connection_id) {
  request_to_connection_io_[request] = connection_id;
  ConnectionToRequestsMap::iterator it =
      connection_to_requests_io_.find(connection_id);
  if (it == connection_to_requests_io_.end()) {
    std::pair<int, std::set<net::URLRequest*> > value(
        connection_id,
        std::set<net::URLRequest*>());
    it = connection_to_requests_io_.insert(value).first;
  }
  it->second.insert(request);
  request_to_buffer_io_[request] = new net::IOBuffer(kBufferSize);
}

void DevToolsHttpProtocolHandler::RequestCompleted(net::URLRequest* request) {
  RequestToSocketMap::iterator it = request_to_connection_io_.find(request);
  if (it == request_to_connection_io_.end())
    return;

  int connection_id = it->second;
  request_to_connection_io_.erase(request);
  ConnectionToRequestsMap::iterator it2 =
      connection_to_requests_io_.find(connection_id);
  it2->second.erase(request);
  request_to_buffer_io_.erase(request);
  delete request;
}

void DevToolsHttpProtocolHandler::Send200(int connection_id,
                                          const std::string& data,
                                          const std::string& mime_type) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(server_.get(),
                        &net::HttpServer::Send200,
                        connection_id,
                        data,
                        mime_type));
}

void DevToolsHttpProtocolHandler::Send404(int connection_id) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(server_.get(),
                        &net::HttpServer::Send404,
                        connection_id));
}

void DevToolsHttpProtocolHandler::Send500(int connection_id,
                                          const std::string& message) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(server_.get(),
                        &net::HttpServer::Send500,
                        connection_id,
                        message));
}

void DevToolsHttpProtocolHandler::AcceptWebSocket(
    int connection_id,
    const net::HttpServerRequestInfo& request) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(server_.get(),
                        &net::HttpServer::AcceptWebSocket,
                        connection_id,
                        request));
}
