// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell.h"

#include <windows.h>
#include <commctrl.h>

#include "base/message_loop.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "base/win/resource_util.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/shell/resource.h"
#include "googleurl/src/gurl.h"
#include "grit/webkit_resources.h"
#include "grit/webkit_chromium_resources.h"
#include "ipc/ipc_message.h"
#include "net/base/net_module.h"
#include "ui/base/win/hwnd_util.h"

namespace {

const wchar_t kWindowTitle[] = L"Content Shell";
const wchar_t kWindowClass[] = L"CONTENT_SHELL";

const int kButtonWidth = 72;
const int kURLBarHeight = 24;

const int kMaxURLLength = 1024;

static base::StringPiece GetRawDataResource(HMODULE module, int resource_id) {
  void* data_ptr;
  size_t data_size;
  return base::win::GetDataResourceFromModule(module, resource_id, &data_ptr,
                                              &data_size)
      ? base::StringPiece(static_cast<char*>(data_ptr), data_size)
      : base::StringPiece();
}

}  // namespace

namespace content {

HINSTANCE Shell::instance_handle_;

void Shell::PlatformInitialize() {
  instance_handle_ = ::GetModuleHandle(NULL);

  INITCOMMONCONTROLSEX InitCtrlEx;
  InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
  InitCtrlEx.dwICC  = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&InitCtrlEx);
  RegisterWindowClass();
}

base::StringPiece Shell::PlatformResourceProvider(int key) {
  return GetRawDataResource(::GetModuleHandle(NULL), key);
}

void Shell::PlatformExit() {
}

void Shell::PlatformCleanUp() {
  // When the window is destroyed, tell the Edit field to forget about us,
  // otherwise we will crash.
  ui::SetWindowProc(url_edit_view_, default_edit_wnd_proc_);
  ui::SetWindowUserData(url_edit_view_, NULL);
}

void Shell::PlatformEnableUIControl(UIControl control, bool is_enabled) {
  int id;
  switch (control) {
    case BACK_BUTTON:
      id = IDC_NAV_BACK;
      break;
    case FORWARD_BUTTON:
      id = IDC_NAV_FORWARD;
      break;
    case STOP_BUTTON:
      id = IDC_NAV_STOP;
      break;
    default:
      NOTREACHED() << "Unknown UI control";
      return;
  }
  EnableWindow(GetDlgItem(window_, id), is_enabled);
}

void Shell::PlatformSetAddressBarURL(const GURL& url) {
  std::wstring url_string = UTF8ToWide(url.spec());
  SendMessage(url_edit_view_, WM_SETTEXT, 0,
              reinterpret_cast<LPARAM>(url_string.c_str()));
}

void Shell::PlatformCreateWindow(int width, int height) {
  window_ = CreateWindow(kWindowClass, kWindowTitle,
                         WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                         CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
                         NULL, NULL, instance_handle_, NULL);
  ui::SetWindowUserData(window_, this);

  HWND hwnd;
  int x = 0;

  hwnd = CreateWindow(L"BUTTON", L"Back",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON ,
                      x, 0, kButtonWidth, kURLBarHeight,
                      window_, (HMENU) IDC_NAV_BACK, instance_handle_, 0);
  x += kButtonWidth;

  hwnd = CreateWindow(L"BUTTON", L"Forward",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON ,
                      x, 0, kButtonWidth, kURLBarHeight,
                      window_, (HMENU) IDC_NAV_FORWARD, instance_handle_, 0);
  x += kButtonWidth;

  hwnd = CreateWindow(L"BUTTON", L"Reload",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON ,
                      x, 0, kButtonWidth, kURLBarHeight,
                      window_, (HMENU) IDC_NAV_RELOAD, instance_handle_, 0);
  x += kButtonWidth;

  hwnd = CreateWindow(L"BUTTON", L"Stop",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON ,
                      x, 0, kButtonWidth, kURLBarHeight,
                      window_, (HMENU) IDC_NAV_STOP, instance_handle_, 0);
  x += kButtonWidth;

  // This control is positioned by PlatformResizeSubViews.
  url_edit_view_ = CreateWindow(L"EDIT", 0,
                                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT |
                                ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                                x, 0, 0, 0, window_, 0, instance_handle_, 0);

  default_edit_wnd_proc_ = ui::SetWindowProc(url_edit_view_,
                                             Shell::EditWndProc);
  ui::SetWindowUserData(url_edit_view_, this);

  ShowWindow(window_, SW_SHOW);

  PlatformSizeTo(width, height);
}

void Shell::PlatformSizeTo(int width, int height) {
  RECT rc, rw;
  GetClientRect(window_, &rc);
  GetWindowRect(window_, &rw);

  int client_width = rc.right - rc.left;
  int window_width = rw.right - rw.left;
  window_width = (window_width - client_width) + width;

  int client_height = rc.bottom - rc.top;
  int window_height = rw.bottom - rw.top;
  window_height = (window_height - client_height) + height;

  // Add space for the url bar.
  window_height += kURLBarHeight;

  SetWindowPos(window_, NULL, 0, 0, window_width, window_height,
               SWP_NOMOVE | SWP_NOZORDER);
}

void Shell::PlatformResizeSubViews() {
  RECT rc;
  GetClientRect(window_, &rc);

  int x = kButtonWidth * 4;
  MoveWindow(url_edit_view_, x, 0, rc.right - x, kURLBarHeight, TRUE);

  MoveWindow(GetContentView(), 0, kURLBarHeight, rc.right,
             rc.bottom - kURLBarHeight, TRUE);
}

ATOM Shell::RegisterWindowClass() {
  WNDCLASSEX wcex = {
      sizeof(WNDCLASSEX),
      CS_HREDRAW | CS_VREDRAW,
      Shell::WndProc,
      0,
      0,
      instance_handle_,
      NULL,
      LoadCursor(NULL, IDC_ARROW),
      0,
      MAKEINTRESOURCE(IDC_CONTENTSHELL),
      kWindowClass,
      NULL,
    };
  return RegisterClassEx(&wcex);
}

LRESULT CALLBACK Shell::WndProc(HWND hwnd, UINT message, WPARAM wParam,
                                LPARAM lParam) {
  Shell* shell = static_cast<Shell*>(ui::GetWindowUserData(hwnd));

  switch (message) {
    case WM_COMMAND: {
      int id = LOWORD(wParam);
      switch (id) {
        case IDM_NEW_WINDOW:
          CreateNewWindow(
              shell->tab_contents()->browser_context(),
              GURL(), NULL, MSG_ROUTING_NONE, NULL);
          break;
        case IDM_CLOSE_WINDOW:
          DestroyWindow(hwnd);
          break;
        case IDM_EXIT:
          PlatformExit();
          break;
        case IDC_NAV_BACK:
          shell->GoBackOrForward(-1);
          break;
        case IDC_NAV_FORWARD:
          shell->GoBackOrForward(1);
          break;
        case IDC_NAV_RELOAD:
          shell->Reload();
          break;
        case IDC_NAV_STOP:
          shell->Stop();
          break;
      }
      break;
    }
    case WM_DESTROY: {
      delete shell;
      if (windows_.empty())
        MessageLoop::current()->Quit();
      return 0;
    }

    case WM_SIZE: {
      if (shell->GetContentView())
        shell->PlatformResizeSubViews();
      return 0;
    }
  }

  return DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK Shell::EditWndProc(HWND hwnd, UINT message,
                                    WPARAM wParam, LPARAM lParam) {
  Shell* shell = static_cast<Shell*>(ui::GetWindowUserData(hwnd));

  switch (message) {
    case WM_CHAR:
      if (wParam == VK_RETURN) {
        wchar_t str[kMaxURLLength + 1];  // Leave room for adding a NULL;
        *(str) = kMaxURLLength;
        LRESULT str_len = SendMessage(hwnd, EM_GETLINE, 0, (LPARAM)str);
        if (str_len > 0) {
          str[str_len] = 0;  // EM_GETLINE doesn't NULL terminate.
          GURL url(str);
          if (!url.has_scheme())
            url = GURL(std::wstring(L"http://") + std::wstring(str));
          shell->LoadURL(url);
        }

        return 0;
      }
  }

  return CallWindowProc(shell->default_edit_wnd_proc_, hwnd, message, wParam,
                        lParam);
}

}  // namespace content
