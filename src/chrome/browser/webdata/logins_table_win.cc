// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/logins_table.h"

#include "base/logging.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/ie7_password.h"
#include "sql/statement.h"

bool LoginsTable::AddIE7Login(const IE7PasswordInfo& info) {
  sql::Statement s(db_->GetUniqueStatement(
      "INSERT OR REPLACE INTO ie7_logins "
      "(url_hash, password_value, date_created) "
      "VALUES (?,?,?)"));
  if (!s) {
    NOTREACHED() << db_->GetErrorMessage();
    return false;
  }

  s.BindString(0, WideToUTF8(info.url_hash));
  s.BindBlob(1, &info.encrypted_data.front(),
             static_cast<int>(info.encrypted_data.size()));
  s.BindInt64(2, info.date_created.ToTimeT());
  if (!s.Run()) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool LoginsTable::RemoveIE7Login(const IE7PasswordInfo& info) {
  // Remove a login by UNIQUE-constrained fields.
  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM ie7_logins WHERE url_hash = ?"));
  if (!s) {
    NOTREACHED() << db_->GetErrorMessage();
    return false;
  }
  s.BindString(0, WideToUTF8(info.url_hash));

  if (!s.Run()) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool LoginsTable::GetIE7Login(const IE7PasswordInfo& info,
                              IE7PasswordInfo* result) {
  DCHECK(result);
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT password_value, date_created FROM ie7_logins "
      "WHERE url_hash == ? "));
  if (!s) {
    NOTREACHED() << db_->GetErrorMessage();
    return false;
  }

  s.BindString(0, WideToUTF8(info.url_hash));
  if (s.Step()) {
    s.ColumnBlobAsVector(0, &result->encrypted_data);
    result->date_created = base::Time::FromTimeT(s.ColumnInt64(1));
    result->url_hash = info.url_hash;
  }
  return s.Succeeded();
}
