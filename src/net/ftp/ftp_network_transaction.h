// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_NETWORK_TRANSACTION_H_
#define NET_FTP_FTP_NETWORK_TRANSACTION_H_
#pragma once

#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "net/base/address_list.h"
#include "net/base/host_resolver.h"
#include "net/base/net_log.h"
#include "net/base/single_request_host_resolver.h"
#include "net/ftp/ftp_ctrl_response_buffer.h"
#include "net/ftp/ftp_response_info.h"
#include "net/ftp/ftp_transaction.h"

namespace net {

class ClientSocketFactory;
class FtpNetworkSession;
class StreamSocket;

class NET_EXPORT_PRIVATE FtpNetworkTransaction : public FtpTransaction {
 public:
  FtpNetworkTransaction(FtpNetworkSession* session,
                        ClientSocketFactory* socket_factory);
  virtual ~FtpNetworkTransaction();

  virtual int Stop(int error);
  virtual int RestartIgnoringLastError(OldCompletionCallback* callback);

  // FtpTransaction methods:
  virtual int Start(const FtpRequestInfo* request_info,
                    OldCompletionCallback* callback,
                    const BoundNetLog& net_log) OVERRIDE;
  virtual int RestartWithAuth(const string16& username,
                              const string16& password,
                              OldCompletionCallback* callback) OVERRIDE;
  virtual int Read(IOBuffer* buf, int buf_len, OldCompletionCallback* callback)
      OVERRIDE;
  virtual const FtpResponseInfo* GetResponseInfo() const OVERRIDE;
  virtual LoadState GetLoadState() const OVERRIDE;
  virtual uint64 GetUploadProgress() const OVERRIDE;

 private:
  enum Command {
    COMMAND_NONE,
    COMMAND_USER,
    COMMAND_PASS,
    COMMAND_SYST,
    COMMAND_TYPE,
    COMMAND_EPSV,
    COMMAND_PASV,
    COMMAND_PWD,
    COMMAND_SIZE,
    COMMAND_RETR,
    COMMAND_CWD,
    COMMAND_LIST,
    COMMAND_QUIT,
  };

  // Major categories of remote system types, as returned by SYST command.
  enum SystemType {
    SYSTEM_TYPE_UNKNOWN,
    SYSTEM_TYPE_UNIX,
    SYSTEM_TYPE_WINDOWS,
    SYSTEM_TYPE_OS2,
    SYSTEM_TYPE_VMS,
  };

  // Data representation type, see RFC 959 section 3.1.1. Data Types.
  // We only support the two most popular data types.
  enum DataType {
    DATA_TYPE_ASCII,
    DATA_TYPE_IMAGE,
  };

  // In FTP we need to issue different commands depending on whether a resource
  // is a file or directory. If we don't know that, we're going to autodetect
  // it.
  enum ResourceType {
    RESOURCE_TYPE_UNKNOWN,
    RESOURCE_TYPE_FILE,
    RESOURCE_TYPE_DIRECTORY,
  };

  enum State {
    // Control connection states:
    STATE_CTRL_RESOLVE_HOST,
    STATE_CTRL_RESOLVE_HOST_COMPLETE,
    STATE_CTRL_CONNECT,
    STATE_CTRL_CONNECT_COMPLETE,
    STATE_CTRL_READ,
    STATE_CTRL_READ_COMPLETE,
    STATE_CTRL_WRITE,
    STATE_CTRL_WRITE_COMPLETE,
    STATE_CTRL_WRITE_USER,
    STATE_CTRL_WRITE_PASS,
    STATE_CTRL_WRITE_SYST,
    STATE_CTRL_WRITE_TYPE,
    STATE_CTRL_WRITE_EPSV,
    STATE_CTRL_WRITE_PASV,
    STATE_CTRL_WRITE_PWD,
    STATE_CTRL_WRITE_RETR,
    STATE_CTRL_WRITE_SIZE,
    STATE_CTRL_WRITE_CWD,
    STATE_CTRL_WRITE_LIST,
    STATE_CTRL_WRITE_QUIT,
    // Data connection states:
    STATE_DATA_CONNECT,
    STATE_DATA_CONNECT_COMPLETE,
    STATE_DATA_READ,
    STATE_DATA_READ_COMPLETE,
    STATE_NONE
  };

  // Resets the members of the transaction so it can be restarted.
  void ResetStateForRestart();

  void DoCallback(int result);
  void OnIOComplete(int result);

  // Executes correct ProcessResponse + command_name function based on last
  // issued command. Returns error code.
  int ProcessCtrlResponse();

  int SendFtpCommand(const std::string& command, Command cmd);

  // Returns request path suitable to be included in an FTP command. If the path
  // will be used as a directory, |is_directory| should be true.
  std::string GetRequestPathForFtpCommand(bool is_directory) const;

  // See if the request URL contains a typecode and make us respect it.
  void DetectTypecode();

  // Runs the state transition loop.
  int DoLoop(int result);

  // Each of these methods corresponds to a State value.  Those with an input
  // argument receive the result from the previous state.  If a method returns
  // ERR_IO_PENDING, then the result from OnIOComplete will be passed to the
  // next state method as the result arg.
  int DoCtrlResolveHost();
  int DoCtrlResolveHostComplete(int result);
  int DoCtrlConnect();
  int DoCtrlConnectComplete(int result);
  int DoCtrlRead();
  int DoCtrlReadComplete(int result);
  int DoCtrlWrite();
  int DoCtrlWriteComplete(int result);
  int DoCtrlWriteUSER();
  int ProcessResponseUSER(const FtpCtrlResponse& response);
  int DoCtrlWritePASS();
  int ProcessResponsePASS(const FtpCtrlResponse& response);
  int DoCtrlWriteSYST();
  int ProcessResponseSYST(const FtpCtrlResponse& response);
  int DoCtrlWritePWD();
  int ProcessResponsePWD(const FtpCtrlResponse& response);
  int DoCtrlWriteTYPE();
  int ProcessResponseTYPE(const FtpCtrlResponse& response);
  int DoCtrlWriteEPSV();
  int ProcessResponseEPSV(const FtpCtrlResponse& response);
  int DoCtrlWritePASV();
  int ProcessResponsePASV(const FtpCtrlResponse& response);
  int DoCtrlWriteRETR();
  int ProcessResponseRETR(const FtpCtrlResponse& response);
  int DoCtrlWriteSIZE();
  int ProcessResponseSIZE(const FtpCtrlResponse& response);
  int DoCtrlWriteCWD();
  int ProcessResponseCWD(const FtpCtrlResponse& response);
  int ProcessResponseCWDNotADirectory();
  int DoCtrlWriteLIST();
  int ProcessResponseLIST(const FtpCtrlResponse& response);
  int DoCtrlWriteQUIT();
  int ProcessResponseQUIT(const FtpCtrlResponse& response);

  int DoDataConnect();
  int DoDataConnectComplete(int result);
  int DoDataRead();
  int DoDataReadComplete(int result);

  void RecordDataConnectionError(int result);

  Command command_sent_;

  OldCompletionCallbackImpl<FtpNetworkTransaction> io_callback_;
  OldCompletionCallback* user_callback_;

  scoped_refptr<FtpNetworkSession> session_;

  BoundNetLog net_log_;
  const FtpRequestInfo* request_;
  FtpResponseInfo response_;

  // Cancels the outstanding request on destruction.
  SingleRequestHostResolver resolver_;
  AddressList addresses_;

  // User buffer passed to the Read method for control socket.
  scoped_refptr<IOBuffer> read_ctrl_buf_;

  scoped_ptr<FtpCtrlResponseBuffer> ctrl_response_buffer_;

  scoped_refptr<IOBuffer> read_data_buf_;
  int read_data_buf_len_;

  // Buffer holding the command line to be written to the control socket.
  scoped_refptr<IOBufferWithSize> write_command_buf_;

  // Buffer passed to the Write method of control socket. It actually writes
  // to the write_command_buf_ at correct offset.
  scoped_refptr<DrainableIOBuffer> write_buf_;

  int last_error_;

  SystemType system_type_;

  // Data type to be used for the TYPE command.
  DataType data_type_;

  // Detected resource type (file or directory).
  ResourceType resource_type_;

  // Initially we favour EPSV over PASV for transfers but should any
  // EPSV fail, we fall back to PASV for the duration of connection.
  bool use_epsv_;

  string16 username_;
  string16 password_;

  // Current directory on the remote server, as returned by last PWD command,
  // with any trailing slash removed.
  std::string current_remote_directory_;

  int data_connection_port_;

  ClientSocketFactory* socket_factory_;

  scoped_ptr<StreamSocket> ctrl_socket_;
  scoped_ptr<StreamSocket> data_socket_;

  State next_state_;
};

}  // namespace net

#endif  // NET_FTP_FTP_NETWORK_TRANSACTION_H_
