// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/keyword_table.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/search_engines/template_url.h"
#include "crypto/hmac.h"
#include "googleurl/src/gurl.h"
#include "sql/statement.h"

using base::Time;

namespace {

// ID of the url column in keywords.
const int kUrlIdPosition = 18;

// Keys used in the meta table.
const char kDefaultSearchProviderKey[] = "Default Search Provider ID";
const char kBuiltinKeywordVersion[] = "Builtin Keyword Version";

// Key for signing the default search provider backup.
// TOOD(avayvod): Add key for Google Chrome to internal repository.
const char kDefaultSearchProviderBackupSigningKey[] =
    "Please, don't change default search engine!";

// Meta table key to store backup value for the default search provider.
const char kDefaultSearchProviderBackupKey[] =
    "Default Search Provider ID Backup";

// Meta table key to store backup value signature for the default search
// provider.
const char kDefaultSearchProviderBackupSignatureKey[] =
    "Default Search Provider ID Backup Signature";

// Histogram name to report protection errors for the default search
// provider.
const char kProtectorHistogramDefaultSearchProvider[] =
    "Protector.DefaultSearchProvider";

// Counter names for protector check results.
const char kProtectorBackupInvalidCounter[] =
    "Protector.BackupInvalidCounter";
const char kProtectorValueChangedCounter[] =
    "Protector.ValueChangedCounter";
const char kProtectorValueValidCounter[] =
    "Protector.ValueValidCounter";

// Protector histogram values. TODO(avayvod): Move to protector.h/cc
enum ProtectorError {
  kProtectorErrorBackupInvalid,
  kProtectorErrorValueChanged,
  kProtectorErrorValueValid,

  // This is for convenience only, must always be the last.
  kProtectorErrorCount
};

void BindURLToStatement(const TemplateURL& url, sql::Statement* s) {
  s->BindString(0, UTF16ToUTF8(url.short_name()));
  s->BindString(1, UTF16ToUTF8(url.keyword()));
  GURL favicon_url = url.GetFaviconURL();
  if (!favicon_url.is_valid()) {
    s->BindString(2, std::string());
  } else {
    s->BindString(2, history::HistoryDatabase::GURLToDatabaseURL(
        url.GetFaviconURL()));
  }
  s->BindString(3, url.url() ? url.url()->url() : std::string());
  s->BindInt(4, url.safe_for_autoreplace() ? 1 : 0);
  if (!url.originating_url().is_valid()) {
    s->BindString(5, std::string());
  } else {
    s->BindString(5, history::HistoryDatabase::GURLToDatabaseURL(
        url.originating_url()));
  }
  s->BindInt64(6, url.date_created().ToTimeT());
  s->BindInt(7, url.usage_count());
  s->BindString(8, JoinString(url.input_encodings(), ';'));
  s->BindInt(9, url.show_in_default_list() ? 1 : 0);
  s->BindString(10, url.suggestions_url() ? url.suggestions_url()->url() :
                std::string());
  s->BindInt(11, url.prepopulate_id());
  s->BindInt(12, url.autogenerate_keyword() ? 1 : 0);
  s->BindInt(13, url.logo_id());
  s->BindBool(14, url.created_by_policy());
  s->BindString(15, url.instant_url() ? url.instant_url()->url() :
                std::string());
  s->BindInt64(16, url.last_modified().ToTimeT());
  s->BindString(17, url.sync_guid());
}

// Signs search provider id and returns its signature.
std::string GetSearchProviderIDSignature(int64 id) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  if (!hmac.Init(kDefaultSearchProviderBackupSigningKey)) {
    LOG(WARNING) << "Failed to initialize HMAC algorithm for signing";
    return std::string();
  }

  std::vector<unsigned char> digest(hmac.DigestLength());
  std::string id_to_sign(base::Int64ToString(id));
  if (!hmac.Sign(id_to_sign, &digest[0], digest.size())) {
    LOG(WARNING) << "Failed to sign setting";
    return std::string();
  }

  return std::string(&digest[0], &digest[0] + digest.size());
}

// Checks if signature for search provider id is correct and returns the
// result.
bool IsSearchProviderIDValid(int64 id, const std::string& signature) {
  std::string id_to_verify(base::Int64ToString(id));
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  if (!hmac.Init(kDefaultSearchProviderBackupSigningKey)) {
    LOG(WARNING) << "Failed to initialize HMAC algorithm for verification.";
    return false;
  }
  return hmac.Verify(id_to_verify, signature);
}

}  // anonymous namespace

KeywordTable::~KeywordTable() {}

bool KeywordTable::Init() {
  if (!db_->DoesTableExist("keywords")) {
    if (!db_->Execute("CREATE TABLE keywords ("
                      "id INTEGER PRIMARY KEY,"
                      "short_name VARCHAR NOT NULL,"
                      "keyword VARCHAR NOT NULL,"
                      "favicon_url VARCHAR NOT NULL,"
                      "url VARCHAR NOT NULL,"
                      "show_in_default_list INTEGER,"
                      "safe_for_autoreplace INTEGER,"
                      "originating_url VARCHAR,"
                      "date_created INTEGER DEFAULT 0,"
                      "usage_count INTEGER DEFAULT 0,"
                      "input_encodings VARCHAR,"
                      "suggest_url VARCHAR,"
                      "prepopulate_id INTEGER DEFAULT 0,"
                      "autogenerate_keyword INTEGER DEFAULT 0,"
                      "logo_id INTEGER DEFAULT 0,"
                      "created_by_policy INTEGER DEFAULT 0,"
                      "instant_url VARCHAR,"
                      "last_modified INTEGER DEFAULT 0,"
                      "sync_guid VARCHAR)")) {
      NOTREACHED();
      return false;
    }
    // Initialize default search engine provider for new profile to have it
    // signed properly. TemplateURLService treats 0 as not existing id and
    // resets the value to the actual default search provider id.
    SetDefaultSearchProviderID(0);
  }
  return true;
}

bool KeywordTable::IsSyncable() {
  return true;
}

bool KeywordTable::AddKeyword(const TemplateURL& url) {
  DCHECK(url.id());
  // Be sure to change kUrlIdPosition if you add columns
  sql::Statement s(db_->GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO keywords "
      "(short_name, keyword, favicon_url, url, safe_for_autoreplace, "
      "originating_url, date_created, usage_count, input_encodings, "
      "show_in_default_list, suggest_url, prepopulate_id, "
      "autogenerate_keyword, logo_id, created_by_policy, instant_url, "
      "last_modified, sync_guid, id) VALUES "
      "(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  BindURLToStatement(url, &s);
  s.BindInt64(kUrlIdPosition, url.id());
  if (!s.Run()) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool KeywordTable::RemoveKeyword(TemplateURLID id) {
  DCHECK(id);
  sql::Statement s(
      db_->GetUniqueStatement("DELETE FROM keywords WHERE id = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindInt64(0, id);
  return s.Run();
}

bool KeywordTable::GetKeywords(std::vector<TemplateURL*>* urls) {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT id, short_name, keyword, favicon_url, url, "
      "safe_for_autoreplace, originating_url, date_created, "
      "usage_count, input_encodings, show_in_default_list, "
      "suggest_url, prepopulate_id, autogenerate_keyword, logo_id, "
      "created_by_policy, instant_url, last_modified, sync_guid "
      "FROM keywords ORDER BY id ASC"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  while (s.Step()) {
    TemplateURL* template_url = new TemplateURL();
    template_url->set_id(s.ColumnInt64(0));

    std::string tmp;
    tmp = s.ColumnString(1);
    DCHECK(!tmp.empty());
    template_url->set_short_name(UTF8ToUTF16(tmp));

    template_url->set_keyword(UTF8ToUTF16(s.ColumnString(2)));

    tmp = s.ColumnString(3);
    if (!tmp.empty())
      template_url->SetFaviconURL(GURL(tmp));

    template_url->SetURL(s.ColumnString(4), 0, 0);

    template_url->set_safe_for_autoreplace(s.ColumnInt(5) == 1);

    tmp = s.ColumnString(6);
    if (!tmp.empty())
      template_url->set_originating_url(GURL(tmp));

    template_url->set_date_created(Time::FromTimeT(s.ColumnInt64(7)));

    template_url->set_usage_count(s.ColumnInt(8));

    std::vector<std::string> encodings;
    base::SplitString(s.ColumnString(9), ';', &encodings);
    template_url->set_input_encodings(encodings);

    template_url->set_show_in_default_list(s.ColumnInt(10) == 1);

    template_url->SetSuggestionsURL(s.ColumnString(11), 0, 0);

    template_url->SetPrepopulateId(s.ColumnInt(12));

    template_url->set_autogenerate_keyword(s.ColumnInt(13) == 1);

    template_url->set_logo_id(s.ColumnInt(14));

    template_url->set_created_by_policy(s.ColumnBool(15));

    template_url->SetInstantURL(s.ColumnString(16), 0, 0);

    template_url->set_last_modified(Time::FromTimeT(s.ColumnInt64(17)));

    // If the persisted sync_guid was empty, we ignore it and allow the TURL to
    // keep its generated GUID.
    if (!s.ColumnString(18).empty())
      template_url->set_sync_guid(s.ColumnString(18));

    urls->push_back(template_url);
  }
  return s.Succeeded();
}

bool KeywordTable::UpdateKeyword(const TemplateURL& url) {
  DCHECK(url.id());
  // Be sure to change kUrlIdPosition if you add columns
  sql::Statement s(db_->GetUniqueStatement(
      "UPDATE keywords "
      "SET short_name=?, keyword=?, favicon_url=?, url=?, "
      "safe_for_autoreplace=?, originating_url=?, date_created=?, "
      "usage_count=?, input_encodings=?, show_in_default_list=?, "
      "suggest_url=?, prepopulate_id=?, autogenerate_keyword=?, "
      "logo_id=?, created_by_policy=?, instant_url=?, last_modified=?, "
      "sync_guid=? WHERE id=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  BindURLToStatement(url, &s);
  s.BindInt64(kUrlIdPosition, url.id());
  return s.Run();
}

bool KeywordTable::SetDefaultSearchProviderID(int64 id) {
  return meta_table_->SetValue(kDefaultSearchProviderKey, id) &&
      SetDefaultSearchProviderBackupID(id);
}

int64 KeywordTable::GetDefaultSearchProviderID() {
  int64 value = 0;
  meta_table_->GetValue(kDefaultSearchProviderKey, &value);
  int64 backup_value = 0;
  meta_table_->GetValue(kDefaultSearchProviderBackupKey, &backup_value);
  std::string backup_signature;
  meta_table_->GetValue(
      kDefaultSearchProviderBackupSignatureKey, &backup_signature);
  // For now only track how often signature or backup is lost or value is
  // changed. Don't change the value since UI is not working yet.
  if (!IsSearchProviderIDValid(backup_value, backup_signature)) {
    UMA_HISTOGRAM_ENUMERATION(kProtectorHistogramDefaultSearchProvider,
                              kProtectorErrorBackupInvalid,
                              kProtectorErrorCount);
    SIMPLE_STATS_COUNTER(kProtectorBackupInvalidCounter);
    SetDefaultSearchProviderBackupID(value);
  } else if (value != backup_value) {
    UMA_HISTOGRAM_ENUMERATION(kProtectorHistogramDefaultSearchProvider,
                              kProtectorErrorValueChanged,
                              kProtectorErrorCount);
    SIMPLE_STATS_COUNTER(kProtectorValueChangedCounter);
    SetDefaultSearchProviderBackupID(value);
  }
  UMA_HISTOGRAM_ENUMERATION(
      kProtectorHistogramDefaultSearchProvider,
      kProtectorErrorValueValid,
      kProtectorErrorCount);
  SIMPLE_STATS_COUNTER(kProtectorValueValidCounter);
  return value;
}

bool KeywordTable::SetBuiltinKeywordVersion(int version) {
  return meta_table_->SetValue(kBuiltinKeywordVersion, version);
}

int KeywordTable::GetBuiltinKeywordVersion() {
  int version = 0;
  if (!meta_table_->GetValue(kBuiltinKeywordVersion, &version))
    return 0;
  return version;
}

bool KeywordTable::MigrateToVersion21AutoGenerateKeywordColumn() {
  return db_->Execute("ALTER TABLE keywords ADD COLUMN autogenerate_keyword "
                      "INTEGER DEFAULT 0");
}

bool KeywordTable::MigrateToVersion25AddLogoIDColumn() {
  return db_->Execute(
      "ALTER TABLE keywords ADD COLUMN logo_id INTEGER DEFAULT 0");
}

bool KeywordTable::MigrateToVersion26AddCreatedByPolicyColumn() {
  return db_->Execute("ALTER TABLE keywords ADD COLUMN created_by_policy "
                      "INTEGER DEFAULT 0");
}

bool KeywordTable::MigrateToVersion28SupportsInstantColumn() {
  return db_->Execute("ALTER TABLE keywords ADD COLUMN supports_instant "
                      "INTEGER DEFAULT 0");
}

bool KeywordTable::MigrateToVersion29InstantUrlToSupportsInstant() {
  if (!db_->Execute("ALTER TABLE keywords ADD COLUMN instant_url VARCHAR"))
    return false;

  if (!db_->Execute("CREATE TABLE keywords_temp ("
                    "id INTEGER PRIMARY KEY,"
                    "short_name VARCHAR NOT NULL,"
                    "keyword VARCHAR NOT NULL,"
                    "favicon_url VARCHAR NOT NULL,"
                    "url VARCHAR NOT NULL,"
                    "show_in_default_list INTEGER,"
                    "safe_for_autoreplace INTEGER,"
                    "originating_url VARCHAR,"
                    "date_created INTEGER DEFAULT 0,"
                    "usage_count INTEGER DEFAULT 0,"
                    "input_encodings VARCHAR,"
                    "suggest_url VARCHAR,"
                    "prepopulate_id INTEGER DEFAULT 0,"
                    "autogenerate_keyword INTEGER DEFAULT 0,"
                    "logo_id INTEGER DEFAULT 0,"
                    "created_by_policy INTEGER DEFAULT 0,"
                    "instant_url VARCHAR)")) {
    return false;
  }

  if (!db_->Execute(
      "INSERT INTO keywords_temp "
      "SELECT id, short_name, keyword, favicon_url, url, "
      "show_in_default_list, safe_for_autoreplace, originating_url, "
      "date_created, usage_count, input_encodings, suggest_url, "
      "prepopulate_id, autogenerate_keyword, logo_id, created_by_policy, "
      "instant_url FROM keywords")) {
    return false;
  }

  if (!db_->Execute("DROP TABLE keywords"))
    return false;

  if (!db_->Execute("ALTER TABLE keywords_temp RENAME TO keywords"))
    return false;

  return true;
}

bool KeywordTable::MigrateToVersion38AddLastModifiedColumn() {
  return db_->Execute(
      "ALTER TABLE keywords ADD COLUMN last_modified INTEGER DEFAULT 0");
}

bool KeywordTable::MigrateToVersion39AddSyncGUIDColumn() {
  return db_->Execute(
      "ALTER TABLE keywords ADD COLUMN sync_guid VARCHAR");
}

bool KeywordTable::MigrateToVersion40AddDefaultSearchEngineBackup() {
  int64 value = 0;
  if (!meta_table_->GetValue(kDefaultSearchProviderKey, &value)) {
    // Set default search provider ID and its backup.
    return SetDefaultSearchProviderID(0);
  }
  return SetDefaultSearchProviderBackupID(value);
}

bool KeywordTable::SetDefaultSearchProviderBackupID(int64 id) {
  return
      meta_table_->SetValue(kDefaultSearchProviderBackupKey, id) &&
      SetDefaultSearchProviderBackupIDSignature(id);
}

bool KeywordTable::SetDefaultSearchProviderBackupIDSignature(int64 id) {
  return meta_table_->SetValue(
      kDefaultSearchProviderBackupSignatureKey,
      GetSearchProviderIDSignature(id));
}
