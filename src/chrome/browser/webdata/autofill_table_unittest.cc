// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "chrome/browser/webdata/autofill_table.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/guid.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/form_field.h"

using base::Time;
using base::TimeDelta;
using webkit_glue::FormField;

// So we can compare AutofillKeys with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const AutofillKey& key) {
  return os << UTF16ToASCII(key.name()) << ", " << UTF16ToASCII(key.value());
}

// So we can compare AutofillChanges with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const AutofillChange& change) {
  switch (change.type()) {
    case AutofillChange::ADD: {
      os << "ADD";
      break;
    }
    case AutofillChange::UPDATE: {
      os << "UPDATE";
      break;
    }
    case AutofillChange::REMOVE: {
      os << "REMOVE";
      break;
    }
  }
  return os << " " << change.key();
}

namespace {

bool CompareAutofillEntries(const AutofillEntry& a, const AutofillEntry& b) {
  std::set<Time> timestamps1(a.timestamps().begin(), a.timestamps().end());
  std::set<Time> timestamps2(b.timestamps().begin(), b.timestamps().end());

  int compVal = a.key().name().compare(b.key().name());
  if (compVal != 0) {
    return compVal < 0;
  }

  compVal = a.key().value().compare(b.key().value());
  if (compVal != 0) {
    return compVal < 0;
  }

  if (timestamps1.size() != timestamps2.size()) {
    return timestamps1.size() < timestamps2.size();
  }

  std::set<Time>::iterator it;
  for (it = timestamps1.begin(); it != timestamps1.end(); it++) {
    timestamps2.erase(*it);
  }

  return !timestamps2.empty();
}

}  // anonymous namespace

class AutofillTableTest : public testing::Test {
 public:
  AutofillTableTest() {}
  virtual ~AutofillTableTest() {}

 protected:
  typedef std::vector<AutofillChange> AutofillChangeList;
  typedef std::set<AutofillEntry,
    bool (*)(const AutofillEntry&, const AutofillEntry&)> AutofillEntrySet;
  typedef std::set<AutofillEntry, bool (*)(const AutofillEntry&,
    const AutofillEntry&)>::iterator AutofillEntrySetIterator;

  virtual void SetUp() {
#if defined(OS_MACOSX)
    Encryptor::UseMockKeychain(true);
#endif
    PathService::Get(chrome::DIR_TEST_DATA, &file_);
    const std::string test_db = "TestWebDatabase" +
        base::Int64ToString(Time::Now().ToTimeT()) +
        ".db";
    file_ = file_.AppendASCII(test_db);
    file_util::Delete(file_, false);
  }

  virtual void TearDown() {
    file_util::Delete(file_, false);
  }

  static AutofillEntry MakeAutofillEntry(const char* name,
                                         const char* value,
                                         time_t timestamp0,
                                         time_t timestamp1) {
    std::vector<Time> timestamps;
    if (timestamp0 >= 0)
      timestamps.push_back(Time::FromTimeT(timestamp0));
    if (timestamp1 >= 0)
      timestamps.push_back(Time::FromTimeT(timestamp1));
    return AutofillEntry(
        AutofillKey(ASCIIToUTF16(name), ASCIIToUTF16(value)), timestamps);
  }

  FilePath file_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillTableTest);
};

TEST_F(AutofillTableTest, Autofill) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  Time t1 = Time::Now();

  // Simulate the submission of a handful of entries in a field called "Name",
  // some more often than others.
  AutofillChangeList changes;
  FormField field;
  field.name = ASCIIToUTF16("Name");
  field.value = ASCIIToUTF16("Superman");
  EXPECT_TRUE(db.GetAutofillTable()->AddFormFieldValue(field, &changes));
  std::vector<string16> v;
  for (int i = 0; i < 5; i++) {
    field.value = ASCIIToUTF16("Clark Kent");
    EXPECT_TRUE(db.GetAutofillTable()->AddFormFieldValue(field, &changes));
  }
  for (int i = 0; i < 3; i++) {
    field.value = ASCIIToUTF16("Clark Sutter");
    EXPECT_TRUE(db.GetAutofillTable()->AddFormFieldValue(field, &changes));
  }
  for (int i = 0; i < 2; i++) {
    field.name = ASCIIToUTF16("Favorite Color");
    field.value = ASCIIToUTF16("Green");
    EXPECT_TRUE(db.GetAutofillTable()->AddFormFieldValue(field, &changes));
  }

  int count = 0;
  int64 pair_id = 0;

  // We have added the name Clark Kent 5 times, so count should be 5 and pair_id
  // should be somthing non-zero.
  field.name = ASCIIToUTF16("Name");
  field.value = ASCIIToUTF16("Clark Kent");
  EXPECT_TRUE(db.GetAutofillTable()->GetIDAndCountOfFormElement(field, &pair_id,
                                                                &count));
  EXPECT_EQ(5, count);
  EXPECT_NE(0, pair_id);

  // Storing in the data base should be case sensitive, so there should be no
  // database entry for clark kent lowercase.
  field.value = ASCIIToUTF16("clark kent");
  EXPECT_TRUE(db.GetAutofillTable()->GetIDAndCountOfFormElement(field, &pair_id,
                                                                &count));
  EXPECT_EQ(0, count);

  field.name = ASCIIToUTF16("Favorite Color");
  field.value = ASCIIToUTF16("Green");
  EXPECT_TRUE(db.GetAutofillTable()->GetIDAndCountOfFormElement(field, &pair_id,
                                                                &count));
  EXPECT_EQ(2, count);

  // This is meant to get a list of suggestions for Name.  The empty prefix
  // in the second argument means it should return all suggestions for a name
  // no matter what they start with.  The order that the names occur in the list
  // should be decreasing order by count.
  EXPECT_TRUE(db.GetAutofillTable()->GetFormValuesForElementName(
      ASCIIToUTF16("Name"), string16(), &v, 6));
  EXPECT_EQ(3U, v.size());
  if (v.size() == 3) {
    EXPECT_EQ(ASCIIToUTF16("Clark Kent"), v[0]);
    EXPECT_EQ(ASCIIToUTF16("Clark Sutter"), v[1]);
    EXPECT_EQ(ASCIIToUTF16("Superman"), v[2]);
  }

  // If we query again limiting the list size to 1, we should only get the most
  // frequent entry.
  EXPECT_TRUE(db.GetAutofillTable()->GetFormValuesForElementName(
      ASCIIToUTF16("Name"), string16(), &v, 1));
  EXPECT_EQ(1U, v.size());
  if (v.size() == 1) {
    EXPECT_EQ(ASCIIToUTF16("Clark Kent"), v[0]);
  }

  // Querying for suggestions given a prefix is case-insensitive, so the prefix
  // "cLa" shoud get suggestions for both Clarks.
  EXPECT_TRUE(db.GetAutofillTable()->GetFormValuesForElementName(
      ASCIIToUTF16("Name"), ASCIIToUTF16("cLa"), &v, 6));
  EXPECT_EQ(2U, v.size());
  if (v.size() == 2) {
    EXPECT_EQ(ASCIIToUTF16("Clark Kent"), v[0]);
    EXPECT_EQ(ASCIIToUTF16("Clark Sutter"), v[1]);
  }

  // Removing all elements since the beginning of this function should remove
  // everything from the database.
  changes.clear();
  EXPECT_TRUE(db.GetAutofillTable()->RemoveFormElementsAddedBetween(
      t1, Time(), &changes));

  const AutofillChange expected_changes[] = {
    AutofillChange(AutofillChange::REMOVE,
                   AutofillKey(ASCIIToUTF16("Name"),
                               ASCIIToUTF16("Superman"))),
    AutofillChange(AutofillChange::REMOVE,
                   AutofillKey(ASCIIToUTF16("Name"),
                               ASCIIToUTF16("Clark Kent"))),
    AutofillChange(AutofillChange::REMOVE,
                   AutofillKey(ASCIIToUTF16("Name"),
                               ASCIIToUTF16("Clark Sutter"))),
    AutofillChange(AutofillChange::REMOVE,
                   AutofillKey(ASCIIToUTF16("Favorite Color"),
                               ASCIIToUTF16("Green"))),
  };
  EXPECT_EQ(arraysize(expected_changes), changes.size());
  for (size_t i = 0; i < arraysize(expected_changes); i++) {
    EXPECT_EQ(expected_changes[i], changes[i]);
  }

  field.name = ASCIIToUTF16("Name");
  field.value = ASCIIToUTF16("Clark Kent");
  EXPECT_TRUE(db.GetAutofillTable()->GetIDAndCountOfFormElement(field, &pair_id,
                                                                &count));
  EXPECT_EQ(0, count);

  EXPECT_TRUE(db.GetAutofillTable()->GetFormValuesForElementName(
      ASCIIToUTF16("Name"), string16(), &v, 6));
  EXPECT_EQ(0U, v.size());

  // Now add some values with empty strings.
  const string16 kValue = ASCIIToUTF16("  toto   ");
  field.name = ASCIIToUTF16("blank");
  field.value = string16();
  EXPECT_TRUE(db.GetAutofillTable()->AddFormFieldValue(field, &changes));
  field.name = ASCIIToUTF16("blank");
  field.value = ASCIIToUTF16(" ");
  EXPECT_TRUE(db.GetAutofillTable()->AddFormFieldValue(field, &changes));
  field.name = ASCIIToUTF16("blank");
  field.value = ASCIIToUTF16("      ");
  EXPECT_TRUE(db.GetAutofillTable()->AddFormFieldValue(field, &changes));
  field.name = ASCIIToUTF16("blank");
  field.value = kValue;
  EXPECT_TRUE(db.GetAutofillTable()->AddFormFieldValue(field, &changes));

  // They should be stored normally as the DB layer does not check for empty
  // values.
  v.clear();
  EXPECT_TRUE(db.GetAutofillTable()->GetFormValuesForElementName(
      ASCIIToUTF16("blank"), string16(), &v, 10));
  EXPECT_EQ(4U, v.size());

  // Now we'll check that ClearAutofillEmptyValueElements() works as expected.
  db.GetAutofillTable()->ClearAutofillEmptyValueElements();

  v.clear();
  EXPECT_TRUE(db.GetAutofillTable()->GetFormValuesForElementName(
      ASCIIToUTF16("blank"), string16(), &v, 10));
  ASSERT_EQ(1U, v.size());

  EXPECT_EQ(kValue, v[0]);
}

TEST_F(AutofillTableTest, Autofill_RemoveBetweenChanges) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  TimeDelta one_day(TimeDelta::FromDays(1));
  Time t1 = Time::Now();
  Time t2 = t1 + one_day;

  AutofillChangeList changes;
  FormField field;
  field.name = ASCIIToUTF16("Name");
  field.value = ASCIIToUTF16("Superman");
  EXPECT_TRUE(
      db.GetAutofillTable()->AddFormFieldValueTime(field, &changes, t1));
  EXPECT_TRUE(
      db.GetAutofillTable()->AddFormFieldValueTime(field, &changes, t2));

  changes.clear();
  EXPECT_TRUE(db.GetAutofillTable()->RemoveFormElementsAddedBetween(
      t1, t2, &changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutofillChange(AutofillChange::UPDATE,
                           AutofillKey(ASCIIToUTF16("Name"),
                                       ASCIIToUTF16("Superman"))),
            changes[0]);
  changes.clear();

  EXPECT_TRUE(db.GetAutofillTable()->RemoveFormElementsAddedBetween(
      t2, t2 + one_day, &changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutofillChange(AutofillChange::REMOVE,
                           AutofillKey(ASCIIToUTF16("Name"),
                                       ASCIIToUTF16("Superman"))),
            changes[0]);
}

TEST_F(AutofillTableTest, Autofill_AddChanges) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  TimeDelta one_day(TimeDelta::FromDays(1));
  Time t1 = Time::Now();
  Time t2 = t1 + one_day;

  AutofillChangeList changes;
  FormField field;
  field.name = ASCIIToUTF16("Name");
  field.value = ASCIIToUTF16("Superman");
  EXPECT_TRUE(
      db.GetAutofillTable()->AddFormFieldValueTime(field, &changes, t1));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutofillChange(AutofillChange::ADD,
                           AutofillKey(ASCIIToUTF16("Name"),
                                       ASCIIToUTF16("Superman"))),
            changes[0]);

  changes.clear();
  EXPECT_TRUE(
      db.GetAutofillTable()->AddFormFieldValueTime(field, &changes, t2));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutofillChange(AutofillChange::UPDATE,
                           AutofillKey(ASCIIToUTF16("Name"),
                                       ASCIIToUTF16("Superman"))),
            changes[0]);
}

TEST_F(AutofillTableTest, Autofill_UpdateOneWithOneTimestamp) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  AutofillEntry entry(MakeAutofillEntry("foo", "bar", 1, -1));
  std::vector<AutofillEntry> entries;
  entries.push_back(entry);
  ASSERT_TRUE(db.GetAutofillTable()->UpdateAutofillEntries(entries));

  FormField field;
  field.name = ASCIIToUTF16("foo");
  field.value = ASCIIToUTF16("bar");
  int64 pair_id;
  int count;
  ASSERT_TRUE(db.GetAutofillTable()->GetIDAndCountOfFormElement(
      field, &pair_id, &count));
  EXPECT_LE(0, pair_id);
  EXPECT_EQ(1, count);

  std::vector<AutofillEntry> all_entries;
  ASSERT_TRUE(db.GetAutofillTable()->GetAllAutofillEntries(&all_entries));
  ASSERT_EQ(1U, all_entries.size());
  EXPECT_TRUE(entry == all_entries[0]);
}

TEST_F(AutofillTableTest, Autofill_UpdateOneWithTwoTimestamps) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  AutofillEntry entry(MakeAutofillEntry("foo", "bar", 1, 2));
  std::vector<AutofillEntry> entries;
  entries.push_back(entry);
  ASSERT_TRUE(db.GetAutofillTable()->UpdateAutofillEntries(entries));

  FormField field;
  field.name = ASCIIToUTF16("foo");
  field.value = ASCIIToUTF16("bar");
  int64 pair_id;
  int count;
  ASSERT_TRUE(db.GetAutofillTable()->GetIDAndCountOfFormElement(
      field, &pair_id, &count));
  EXPECT_LE(0, pair_id);
  EXPECT_EQ(2, count);

  std::vector<AutofillEntry> all_entries;
  ASSERT_TRUE(db.GetAutofillTable()->GetAllAutofillEntries(&all_entries));
  ASSERT_EQ(1U, all_entries.size());
  EXPECT_TRUE(entry == all_entries[0]);
}

TEST_F(AutofillTableTest, Autofill_GetAutofillTimestamps) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  AutofillEntry entry(MakeAutofillEntry("foo", "bar", 1, 2));
  std::vector<AutofillEntry> entries;
  entries.push_back(entry);
  ASSERT_TRUE(db.GetAutofillTable()->UpdateAutofillEntries(entries));

  std::vector<Time> timestamps;
  ASSERT_TRUE(db.GetAutofillTable()->GetAutofillTimestamps(ASCIIToUTF16("foo"),
                                                           ASCIIToUTF16("bar"),
                                                           &timestamps));
  ASSERT_EQ(2U, timestamps.size());
  EXPECT_TRUE(Time::FromTimeT(1) == timestamps[0]);
  EXPECT_TRUE(Time::FromTimeT(2) == timestamps[1]);
}

TEST_F(AutofillTableTest, Autofill_UpdateTwo) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  AutofillEntry entry0(MakeAutofillEntry("foo", "bar0", 1, -1));
  AutofillEntry entry1(MakeAutofillEntry("foo", "bar1", 2, 3));
  std::vector<AutofillEntry> entries;
  entries.push_back(entry0);
  entries.push_back(entry1);
  ASSERT_TRUE(db.GetAutofillTable()->UpdateAutofillEntries(entries));

  FormField field0;
  field0.name = ASCIIToUTF16("foo");
  field0.value = ASCIIToUTF16("bar0");
  int64 pair_id;
  int count;
  ASSERT_TRUE(db.GetAutofillTable()->GetIDAndCountOfFormElement(
      field0, &pair_id, &count));
  EXPECT_LE(0, pair_id);
  EXPECT_EQ(1, count);

  FormField field1;
  field1.name = ASCIIToUTF16("foo");
  field1.value = ASCIIToUTF16("bar1");
  ASSERT_TRUE(db.GetAutofillTable()->GetIDAndCountOfFormElement(
      field1, &pair_id, &count));
  EXPECT_LE(0, pair_id);
  EXPECT_EQ(2, count);
}

TEST_F(AutofillTableTest, Autofill_UpdateReplace) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  AutofillChangeList changes;
  // Add a form field.  This will be replaced.
  FormField field;
  field.name = ASCIIToUTF16("Name");
  field.value = ASCIIToUTF16("Superman");
  EXPECT_TRUE(db.GetAutofillTable()->AddFormFieldValue(field, &changes));

  AutofillEntry entry(MakeAutofillEntry("Name", "Superman", 1, 2));
  std::vector<AutofillEntry> entries;
  entries.push_back(entry);
  ASSERT_TRUE(db.GetAutofillTable()->UpdateAutofillEntries(entries));

  std::vector<AutofillEntry> all_entries;
  ASSERT_TRUE(db.GetAutofillTable()->GetAllAutofillEntries(&all_entries));
  ASSERT_EQ(1U, all_entries.size());
  EXPECT_TRUE(entry == all_entries[0]);
}

TEST_F(AutofillTableTest, Autofill_UpdateDontReplace) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  Time t = Time::Now();
  AutofillEntry existing(
      MakeAutofillEntry("Name", "Superman", t.ToTimeT(), -1));

  AutofillChangeList changes;
  // Add a form field.  This will NOT be replaced.
  FormField field;
  field.name = existing.key().name();
  field.value = existing.key().value();
  EXPECT_TRUE(db.GetAutofillTable()->AddFormFieldValueTime(field, &changes, t));
  AutofillEntry entry(MakeAutofillEntry("Name", "Clark Kent", 1, 2));
  std::vector<AutofillEntry> entries;
  entries.push_back(entry);
  ASSERT_TRUE(db.GetAutofillTable()->UpdateAutofillEntries(entries));

  std::vector<AutofillEntry> all_entries;
  ASSERT_TRUE(db.GetAutofillTable()->GetAllAutofillEntries(&all_entries));
  ASSERT_EQ(2U, all_entries.size());
  AutofillEntrySet expected_entries(all_entries.begin(),
                                    all_entries.end(),
                                    CompareAutofillEntries);
  EXPECT_EQ(1U, expected_entries.count(existing));
  EXPECT_EQ(1U, expected_entries.count(entry));
}

TEST_F(AutofillTableTest, Autofill_AddFormFieldValues) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  Time t = Time::Now();

  // Add multiple values for "firstname" and "lastname" names.  Test that only
  // first value of each gets added. Related to security issue:
  // http://crbug.com/51727.
  std::vector<FormField> elements;
  FormField field;
  field.name = ASCIIToUTF16("firstname");
  field.value = ASCIIToUTF16("Joe");
  elements.push_back(field);

  field.name = ASCIIToUTF16("firstname");
  field.value = ASCIIToUTF16("Jane");
  elements.push_back(field);

  field.name = ASCIIToUTF16("lastname");
  field.value = ASCIIToUTF16("Smith");
  elements.push_back(field);

  field.name = ASCIIToUTF16("lastname");
  field.value = ASCIIToUTF16("Jones");
  elements.push_back(field);

  std::vector<AutofillChange> changes;
  db.GetAutofillTable()->AddFormFieldValuesTime(elements, &changes, t);

  ASSERT_EQ(2U, changes.size());
  EXPECT_EQ(changes[0], AutofillChange(AutofillChange::ADD,
                                       AutofillKey(ASCIIToUTF16("firstname"),
                                       ASCIIToUTF16("Joe"))));
  EXPECT_EQ(changes[1], AutofillChange(AutofillChange::ADD,
                                       AutofillKey(ASCIIToUTF16("lastname"),
                                       ASCIIToUTF16("Smith"))));

  std::vector<AutofillEntry> all_entries;
  ASSERT_TRUE(db.GetAutofillTable()->GetAllAutofillEntries(&all_entries));
  ASSERT_EQ(2U, all_entries.size());
}

TEST_F(AutofillTableTest, AutofillProfile) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  // Add a 'Home' profile.
  AutofillProfile home_profile;
  home_profile.SetInfo(NAME_FIRST, ASCIIToUTF16("John"));
  home_profile.SetInfo(NAME_MIDDLE, ASCIIToUTF16("Q."));
  home_profile.SetInfo(NAME_LAST, ASCIIToUTF16("Smith"));
  home_profile.SetInfo(EMAIL_ADDRESS, ASCIIToUTF16("js@smith.xyz"));
  home_profile.SetInfo(COMPANY_NAME, ASCIIToUTF16("Google"));
  home_profile.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1234 Apple Way"));
  home_profile.SetInfo(ADDRESS_HOME_LINE2, ASCIIToUTF16("unit 5"));
  home_profile.SetInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("Los Angeles"));
  home_profile.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("CA"));
  home_profile.SetInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("90025"));
  home_profile.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));
  home_profile.SetInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("18181234567"));

  Time pre_creation_time = Time::Now();
  EXPECT_TRUE(db.GetAutofillTable()->AddAutofillProfile(home_profile));
  Time post_creation_time = Time::Now();

  // Get the 'Home' profile.
  AutofillProfile* db_profile;
  ASSERT_TRUE(db.GetAutofillTable()->GetAutofillProfile(
      home_profile.guid(), &db_profile));
  EXPECT_EQ(home_profile, *db_profile);
  sql::Statement s_home(db.GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified "
      "FROM autofill_profiles WHERE guid=?"));
  s_home.BindString(0, home_profile.guid());
  ASSERT_TRUE(s_home);
  ASSERT_TRUE(s_home.Step());
  EXPECT_GE(s_home.ColumnInt64(0), pre_creation_time.ToTimeT());
  EXPECT_LE(s_home.ColumnInt64(0), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_home.Step());
  delete db_profile;

  // Add a 'Billing' profile.
  AutofillProfile billing_profile = home_profile;
  billing_profile.set_guid(guid::GenerateGUID());
  billing_profile.SetInfo(ADDRESS_HOME_LINE1,
                          ASCIIToUTF16("5678 Bottom Street"));
  billing_profile.SetInfo(ADDRESS_HOME_LINE2, ASCIIToUTF16("suite 3"));

  pre_creation_time = Time::Now();
  EXPECT_TRUE(db.GetAutofillTable()->AddAutofillProfile(billing_profile));
  post_creation_time = Time::Now();

  // Get the 'Billing' profile.
  ASSERT_TRUE(db.GetAutofillTable()->GetAutofillProfile(
      billing_profile.guid(), &db_profile));
  EXPECT_EQ(billing_profile, *db_profile);
  sql::Statement s_billing(db.GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles WHERE guid=?"));
  s_billing.BindString(0, billing_profile.guid());
  ASSERT_TRUE(s_billing);
  ASSERT_TRUE(s_billing.Step());
  EXPECT_GE(s_billing.ColumnInt64(0), pre_creation_time.ToTimeT());
  EXPECT_LE(s_billing.ColumnInt64(0), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_billing.Step());
  delete db_profile;

  // Update the 'Billing' profile, name only.
  billing_profile.SetInfo(NAME_FIRST, ASCIIToUTF16("Jane"));
  Time pre_modification_time = Time::Now();
  EXPECT_TRUE(db.GetAutofillTable()->UpdateAutofillProfileMulti(
      billing_profile));
  Time post_modification_time = Time::Now();
  ASSERT_TRUE(db.GetAutofillTable()->GetAutofillProfile(
      billing_profile.guid(), &db_profile));
  EXPECT_EQ(billing_profile, *db_profile);
  sql::Statement s_billing_updated(db.GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles WHERE guid=?"));
  s_billing_updated.BindString(0, billing_profile.guid());
  ASSERT_TRUE(s_billing_updated);
  ASSERT_TRUE(s_billing_updated.Step());
  EXPECT_GE(s_billing_updated.ColumnInt64(0),
            pre_modification_time.ToTimeT());
  EXPECT_LE(s_billing_updated.ColumnInt64(0),
            post_modification_time.ToTimeT());
  EXPECT_FALSE(s_billing_updated.Step());
  delete db_profile;

  // Update the 'Billing' profile.
  billing_profile.SetInfo(NAME_FIRST, ASCIIToUTF16("Janice"));
  billing_profile.SetInfo(NAME_MIDDLE, ASCIIToUTF16("C."));
  billing_profile.SetInfo(NAME_FIRST, ASCIIToUTF16("Joplin"));
  billing_profile.SetInfo(EMAIL_ADDRESS, ASCIIToUTF16("jane@singer.com"));
  billing_profile.SetInfo(COMPANY_NAME, ASCIIToUTF16("Indy"));
  billing_profile.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("Open Road"));
  billing_profile.SetInfo(ADDRESS_HOME_LINE2, ASCIIToUTF16("Route 66"));
  billing_profile.SetInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("NFA"));
  billing_profile.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("NY"));
  billing_profile.SetInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("10011"));
  billing_profile.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("United States"));
  billing_profile.SetInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("18181230000"));
  Time pre_modification_time_2 = Time::Now();
  EXPECT_TRUE(db.GetAutofillTable()->UpdateAutofillProfileMulti(
      billing_profile));
  Time post_modification_time_2 = Time::Now();
  ASSERT_TRUE(db.GetAutofillTable()->GetAutofillProfile(
      billing_profile.guid(), &db_profile));
  EXPECT_EQ(billing_profile, *db_profile);
  sql::Statement s_billing_updated_2(db.GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles WHERE guid=?"));
  s_billing_updated_2.BindString(0, billing_profile.guid());
  ASSERT_TRUE(s_billing_updated_2);
  ASSERT_TRUE(s_billing_updated_2.Step());
  EXPECT_GE(s_billing_updated_2.ColumnInt64(0),
            pre_modification_time_2.ToTimeT());
  EXPECT_LE(s_billing_updated_2.ColumnInt64(0),
            post_modification_time_2.ToTimeT());
  EXPECT_FALSE(s_billing_updated_2.Step());
  delete db_profile;

  // Remove the 'Billing' profile.
  EXPECT_TRUE(db.GetAutofillTable()->RemoveAutofillProfile(
      billing_profile.guid()));
  EXPECT_FALSE(db.GetAutofillTable()->GetAutofillProfile(
      billing_profile.guid(), &db_profile));
}

TEST_F(AutofillTableTest, AutofillProfileMultiValueNames) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  AutofillProfile p;
  const string16 kJohnDoe(ASCIIToUTF16("John Doe"));
  const string16 kJohnPDoe(ASCIIToUTF16("John P. Doe"));
  std::vector<string16> set_values;
  set_values.push_back(kJohnDoe);
  set_values.push_back(kJohnPDoe);
  p.SetMultiInfo(NAME_FULL, set_values);

  EXPECT_TRUE(db.GetAutofillTable()->AddAutofillProfile(p));

  AutofillProfile* db_profile;
  ASSERT_TRUE(db.GetAutofillTable()->GetAutofillProfile(p.guid(), &db_profile));
  EXPECT_EQ(p, *db_profile);
  EXPECT_EQ(0, p.CompareMulti(*db_profile));
  delete db_profile;

  // Update the values.
  const string16 kNoOne(ASCIIToUTF16("No One"));
  set_values[1] = kNoOne;
  p.SetMultiInfo(NAME_FULL, set_values);
  EXPECT_TRUE(db.GetAutofillTable()->UpdateAutofillProfileMulti(p));
  ASSERT_TRUE(db.GetAutofillTable()->GetAutofillProfile(p.guid(), &db_profile));
  EXPECT_EQ(p, *db_profile);
  EXPECT_EQ(0, p.CompareMulti(*db_profile));
  delete db_profile;

  // Delete values.
  set_values.clear();
  p.SetMultiInfo(NAME_FULL, set_values);
  EXPECT_TRUE(db.GetAutofillTable()->UpdateAutofillProfileMulti(p));
  ASSERT_TRUE(db.GetAutofillTable()->GetAutofillProfile(p.guid(), &db_profile));
  EXPECT_EQ(p, *db_profile);
  EXPECT_EQ(0, p.CompareMulti(*db_profile));
  EXPECT_EQ(string16(), db_profile->GetInfo(NAME_FULL));
  delete db_profile;
}

TEST_F(AutofillTableTest, AutofillProfileSingleValue) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  AutofillProfile p;
  const string16 kJohnDoe(ASCIIToUTF16("John Doe"));
  const string16 kJohnPDoe(ASCIIToUTF16("John P. Doe"));
  std::vector<string16> set_values;
  set_values.push_back(kJohnDoe);
  set_values.push_back(kJohnPDoe);
  p.SetMultiInfo(NAME_FULL, set_values);

  EXPECT_TRUE(db.GetAutofillTable()->AddAutofillProfile(p));

  AutofillProfile* db_profile;
  ASSERT_TRUE(db.GetAutofillTable()->GetAutofillProfile(p.guid(), &db_profile));
  EXPECT_EQ(p, *db_profile);
  EXPECT_EQ(0, p.CompareMulti(*db_profile));
  delete db_profile;

  // Update the values.  This update is the "single value" update, it should
  // not perturb the multi-values following the zeroth entry.  This simulates
  // the Sync use-case until Sync can be changed to be multi-value aware.
  const string16 kNoOne(ASCIIToUTF16("No One"));
  set_values.resize(1);
  set_values[0] = kNoOne;
  p.SetMultiInfo(NAME_FULL, set_values);
  EXPECT_TRUE(db.GetAutofillTable()->UpdateAutofillProfile(p));
  ASSERT_TRUE(db.GetAutofillTable()->GetAutofillProfile(p.guid(), &db_profile));
  EXPECT_EQ(p, *db_profile);
  EXPECT_NE(0, p.CompareMulti(*db_profile));
  db_profile->GetMultiInfo(NAME_FULL, &set_values);
  ASSERT_EQ(2UL, set_values.size());
  EXPECT_EQ(kNoOne, set_values[0]);
  EXPECT_EQ(kJohnPDoe, set_values[1]);
  delete db_profile;
}

TEST_F(AutofillTableTest, AutofillProfileMultiValueEmails) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  AutofillProfile p;
  const string16 kJohnDoe(ASCIIToUTF16("john@doe.com"));
  const string16 kJohnPDoe(ASCIIToUTF16("john_p@doe.com"));
  std::vector<string16> set_values;
  set_values.push_back(kJohnDoe);
  set_values.push_back(kJohnPDoe);
  p.SetMultiInfo(EMAIL_ADDRESS, set_values);

  EXPECT_TRUE(db.GetAutofillTable()->AddAutofillProfile(p));

  AutofillProfile* db_profile;
  ASSERT_TRUE(db.GetAutofillTable()->GetAutofillProfile(p.guid(), &db_profile));
  EXPECT_EQ(p, *db_profile);
  EXPECT_EQ(0, p.CompareMulti(*db_profile));
  delete db_profile;

  // Update the values.
  const string16 kNoOne(ASCIIToUTF16("no@one.com"));
  set_values[1] = kNoOne;
  p.SetMultiInfo(EMAIL_ADDRESS, set_values);
  EXPECT_TRUE(db.GetAutofillTable()->UpdateAutofillProfileMulti(p));
  ASSERT_TRUE(db.GetAutofillTable()->GetAutofillProfile(p.guid(), &db_profile));
  EXPECT_EQ(p, *db_profile);
  EXPECT_EQ(0, p.CompareMulti(*db_profile));
  delete db_profile;

  // Delete values.
  set_values.clear();
  p.SetMultiInfo(EMAIL_ADDRESS, set_values);
  EXPECT_TRUE(db.GetAutofillTable()->UpdateAutofillProfileMulti(p));
  ASSERT_TRUE(db.GetAutofillTable()->GetAutofillProfile(p.guid(), &db_profile));
  EXPECT_EQ(p, *db_profile);
  EXPECT_EQ(0, p.CompareMulti(*db_profile));
  EXPECT_EQ(string16(), db_profile->GetInfo(EMAIL_ADDRESS));
  delete db_profile;
}

TEST_F(AutofillTableTest, AutofillProfileMultiValuePhone) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  AutofillProfile p;
  const string16 kJohnDoe(ASCIIToUTF16("4151112222"));
  const string16 kJohnPDoe(ASCIIToUTF16("4151113333"));
  std::vector<string16> set_values;
  set_values.push_back(kJohnDoe);
  set_values.push_back(kJohnPDoe);
  p.SetMultiInfo(PHONE_HOME_WHOLE_NUMBER, set_values);

  EXPECT_TRUE(db.GetAutofillTable()->AddAutofillProfile(p));

  AutofillProfile* db_profile;
  ASSERT_TRUE(db.GetAutofillTable()->GetAutofillProfile(p.guid(), &db_profile));
  EXPECT_EQ(p, *db_profile);
  EXPECT_EQ(0, p.CompareMulti(*db_profile));
  delete db_profile;

  // Update the values.
  const string16 kNoOne(ASCIIToUTF16("4151110000"));
  set_values[1] = kNoOne;
  p.SetMultiInfo(PHONE_HOME_WHOLE_NUMBER, set_values);
  EXPECT_TRUE(db.GetAutofillTable()->UpdateAutofillProfileMulti(p));
  ASSERT_TRUE(db.GetAutofillTable()->GetAutofillProfile(p.guid(), &db_profile));
  EXPECT_EQ(p, *db_profile);
  EXPECT_EQ(0, p.CompareMulti(*db_profile));
  delete db_profile;

  // Delete values.
  set_values.clear();
  p.SetMultiInfo(PHONE_HOME_WHOLE_NUMBER, set_values);
  EXPECT_TRUE(db.GetAutofillTable()->UpdateAutofillProfileMulti(p));
  ASSERT_TRUE(db.GetAutofillTable()->GetAutofillProfile(p.guid(), &db_profile));
  EXPECT_EQ(p, *db_profile);
  EXPECT_EQ(0, p.CompareMulti(*db_profile));
  EXPECT_EQ(string16(), db_profile->GetInfo(EMAIL_ADDRESS));
  delete db_profile;
}

TEST_F(AutofillTableTest, AutofillProfileTrash) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  std::vector<std::string> guids;
  db.GetAutofillTable()->GetAutofillProfilesInTrash(&guids);
  EXPECT_TRUE(guids.empty());

  ASSERT_TRUE(db.GetAutofillTable()->AddAutofillGUIDToTrash(
      "00000000-0000-0000-0000-000000000000"));
  ASSERT_TRUE(db.GetAutofillTable()->AddAutofillGUIDToTrash(
      "00000000-0000-0000-0000-000000000001"));
  ASSERT_TRUE(db.GetAutofillTable()->GetAutofillProfilesInTrash(&guids));
  EXPECT_EQ(2UL, guids.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000000", guids[0]);
  EXPECT_EQ("00000000-0000-0000-0000-000000000001", guids[1]);

  ASSERT_TRUE(db.GetAutofillTable()->EmptyAutofillProfilesTrash());
  ASSERT_TRUE(db.GetAutofillTable()->GetAutofillProfilesInTrash(&guids));
  EXPECT_TRUE(guids.empty());
}

TEST_F(AutofillTableTest, AutofillProfileTrashInteraction) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  std::vector<std::string> guids;
  db.GetAutofillTable()->GetAutofillProfilesInTrash(&guids);
  EXPECT_TRUE(guids.empty());

  AutofillProfile profile;
  profile.SetInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profile.SetInfo(NAME_MIDDLE, ASCIIToUTF16("Q."));
  profile.SetInfo(NAME_LAST, ASCIIToUTF16("Smith"));
  profile.SetInfo(EMAIL_ADDRESS,ASCIIToUTF16("js@smith.xyz"));
  profile.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1 Main St"));
  profile.SetInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("Los Angeles"));
  profile.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("CA"));
  profile.SetInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("90025"));
  profile.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));

  // Mark this profile as in the trash.  This stops |AddAutofillProfile| from
  // adding it.
  EXPECT_TRUE(db.GetAutofillTable()->AddAutofillGUIDToTrash(profile.guid()));
  EXPECT_TRUE(db.GetAutofillTable()->AddAutofillProfile(profile));
  AutofillProfile* added_profile = NULL;
  EXPECT_FALSE(db.GetAutofillTable()->GetAutofillProfile(
      profile.guid(), &added_profile));
  EXPECT_EQ(static_cast<AutofillProfile*>(NULL), added_profile);

  // Add the profile for real this time.
  EXPECT_TRUE(db.GetAutofillTable()->EmptyAutofillProfilesTrash());
  EXPECT_TRUE(db.GetAutofillTable()->GetAutofillProfilesInTrash(&guids));
  EXPECT_TRUE(guids.empty());
  EXPECT_TRUE(db.GetAutofillTable()->AddAutofillProfile(profile));
  EXPECT_TRUE(db.GetAutofillTable()->GetAutofillProfile(profile.guid(),
                                                        &added_profile));
  ASSERT_NE(static_cast<AutofillProfile*>(NULL), added_profile);
  delete added_profile;

  // Mark this profile as in the trash.  This stops |UpdateAutofillProfileMulti|
  // from updating it.  In normal operation a profile should not be both in the
  // trash and in the profiles table simultaneously.
  EXPECT_TRUE(db.GetAutofillTable()->AddAutofillGUIDToTrash(profile.guid()));
  profile.SetInfo(NAME_FIRST, ASCIIToUTF16("Jane"));
  EXPECT_TRUE(db.GetAutofillTable()->UpdateAutofillProfileMulti(profile));
  AutofillProfile* updated_profile = NULL;
  EXPECT_TRUE(db.GetAutofillTable()->GetAutofillProfile(
      profile.guid(), &updated_profile));
  ASSERT_NE(static_cast<AutofillProfile*>(NULL), added_profile);
  EXPECT_EQ(ASCIIToUTF16("John"), updated_profile->GetInfo(NAME_FIRST));
  delete updated_profile;

  // Try to delete the trashed profile.  This stops |RemoveAutofillProfile| from
  // deleting it.  In normal operation deletion is done by migration step, and
  // removal from trash is done by |WebDataService|.  |RemoveAutofillProfile|
  // does remove the item from the trash if it is found however, so that if
  // other clients remove it (via Sync say) then it is gone and doesn't need to
  // be processed further by |WebDataService|.
  EXPECT_TRUE(db.GetAutofillTable()->RemoveAutofillProfile(profile.guid()));
  AutofillProfile* removed_profile = NULL;
  EXPECT_TRUE(db.GetAutofillTable()->GetAutofillProfile(profile.guid(),
                                                        &removed_profile));
  EXPECT_FALSE(db.GetAutofillTable()->IsAutofillGUIDInTrash(profile.guid()));
  ASSERT_NE(static_cast<AutofillProfile*>(NULL), removed_profile);
  delete removed_profile;

  // Check that emptying the trash now allows removal to occur.
  EXPECT_TRUE(db.GetAutofillTable()->EmptyAutofillProfilesTrash());
  EXPECT_TRUE(db.GetAutofillTable()->RemoveAutofillProfile(profile.guid()));
  removed_profile = NULL;
  EXPECT_FALSE(db.GetAutofillTable()->GetAutofillProfile(profile.guid(),
                                                         &removed_profile));
  EXPECT_EQ(static_cast<AutofillProfile*>(NULL), removed_profile);
}

TEST_F(AutofillTableTest, CreditCard) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  // Add a 'Work' credit card.
  CreditCard work_creditcard;
  work_creditcard.SetInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Jack Torrance"));
  work_creditcard.SetInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16("1234567890123456"));
  work_creditcard.SetInfo(CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("04"));
  work_creditcard.SetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, ASCIIToUTF16("2013"));

  Time pre_creation_time = Time::Now();
  EXPECT_TRUE(db.GetAutofillTable()->AddCreditCard(work_creditcard));
  Time post_creation_time = Time::Now();

  // Get the 'Work' credit card.
  CreditCard* db_creditcard;
  ASSERT_TRUE(db.GetAutofillTable()->GetCreditCard(work_creditcard.guid(),
                                                   &db_creditcard));
  EXPECT_EQ(work_creditcard, *db_creditcard);
  sql::Statement s_work(db.GetSQLConnection()->GetUniqueStatement(
      "SELECT guid, name_on_card, expiration_month, expiration_year, "
      "card_number_encrypted, date_modified "
      "FROM credit_cards WHERE guid=?"));
  s_work.BindString(0, work_creditcard.guid());
  ASSERT_TRUE(s_work);
  ASSERT_TRUE(s_work.Step());
  EXPECT_GE(s_work.ColumnInt64(5), pre_creation_time.ToTimeT());
  EXPECT_LE(s_work.ColumnInt64(5), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_work.Step());
  delete db_creditcard;

  // Add a 'Target' credit card.
  CreditCard target_creditcard;
  target_creditcard.SetInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Jack Torrance"));
  target_creditcard.SetInfo(CREDIT_CARD_NUMBER,
                            ASCIIToUTF16("1111222233334444"));
  target_creditcard.SetInfo(CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("06"));
  target_creditcard.SetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, ASCIIToUTF16("2012"));

  pre_creation_time = Time::Now();
  EXPECT_TRUE(db.GetAutofillTable()->AddCreditCard(target_creditcard));
  post_creation_time = Time::Now();
  ASSERT_TRUE(db.GetAutofillTable()->GetCreditCard(target_creditcard.guid(),
                                                   &db_creditcard));
  EXPECT_EQ(target_creditcard, *db_creditcard);
  sql::Statement s_target(db.GetSQLConnection()->GetUniqueStatement(
      "SELECT guid, name_on_card, expiration_month, expiration_year, "
      "card_number_encrypted, date_modified "
      "FROM credit_cards WHERE guid=?"));
  s_target.BindString(0, target_creditcard.guid());
  ASSERT_TRUE(s_target);
  ASSERT_TRUE(s_target.Step());
  EXPECT_GE(s_target.ColumnInt64(5), pre_creation_time.ToTimeT());
  EXPECT_LE(s_target.ColumnInt64(5), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_target.Step());
  delete db_creditcard;

  // Update the 'Target' credit card.
  target_creditcard.SetInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Charles Grady"));
  Time pre_modification_time = Time::Now();
  EXPECT_TRUE(db.GetAutofillTable()->UpdateCreditCard(target_creditcard));
  Time post_modification_time = Time::Now();
  ASSERT_TRUE(db.GetAutofillTable()->GetCreditCard(target_creditcard.guid(),
                                                   &db_creditcard));
  EXPECT_EQ(target_creditcard, *db_creditcard);
  sql::Statement s_target_updated(db.GetSQLConnection()->GetUniqueStatement(
      "SELECT guid, name_on_card, expiration_month, expiration_year, "
      "card_number_encrypted, date_modified "
      "FROM credit_cards WHERE guid=?"));
  s_target_updated.BindString(0, target_creditcard.guid());
  ASSERT_TRUE(s_target_updated);
  ASSERT_TRUE(s_target_updated.Step());
  EXPECT_GE(s_target_updated.ColumnInt64(5), pre_modification_time.ToTimeT());
  EXPECT_LE(s_target_updated.ColumnInt64(5), post_modification_time.ToTimeT());
  EXPECT_FALSE(s_target_updated.Step());
  delete db_creditcard;

  // Remove the 'Target' credit card.
  EXPECT_TRUE(db.GetAutofillTable()->RemoveCreditCard(
      target_creditcard.guid()));
  EXPECT_FALSE(db.GetAutofillTable()->GetCreditCard(target_creditcard.guid(),
                                                    &db_creditcard));
}

TEST_F(AutofillTableTest, UpdateAutofillProfile) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  // Add a profile to the db.
  AutofillProfile profile;
  profile.SetInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profile.SetInfo(NAME_MIDDLE, ASCIIToUTF16("Q."));
  profile.SetInfo(NAME_LAST, ASCIIToUTF16("Smith"));
  profile.SetInfo(EMAIL_ADDRESS, ASCIIToUTF16("js@example.com"));
  profile.SetInfo(COMPANY_NAME, ASCIIToUTF16("Google"));
  profile.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1234 Apple Way"));
  profile.SetInfo(ADDRESS_HOME_LINE2, ASCIIToUTF16("unit 5"));
  profile.SetInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("Los Angeles"));
  profile.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("CA"));
  profile.SetInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("90025"));
  profile.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));
  profile.SetInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("18181234567"));
  db.GetAutofillTable()->AddAutofillProfile(profile);

  // Set a mocked value for the profile's creation time.
  const time_t mock_creation_date = Time::Now().ToTimeT() - 13;
  sql::Statement s_mock_creation_date(db.GetSQLConnection()->GetUniqueStatement(
      "UPDATE autofill_profiles SET date_modified = ?"));
  ASSERT_TRUE(s_mock_creation_date);
  s_mock_creation_date.BindInt64(0, mock_creation_date);
  ASSERT_TRUE(s_mock_creation_date.Run());

  // Get the profile.
  AutofillProfile* tmp_profile;
  ASSERT_TRUE(db.GetAutofillTable()->GetAutofillProfile(profile.guid(),
                                                        &tmp_profile));
  scoped_ptr<AutofillProfile> db_profile(tmp_profile);
  EXPECT_EQ(profile, *db_profile);
  sql::Statement s_original(db.GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles"));
  ASSERT_TRUE(s_original);
  ASSERT_TRUE(s_original.Step());
  EXPECT_EQ(mock_creation_date, s_original.ColumnInt64(0));
  EXPECT_FALSE(s_original.Step());

  // Now, update the profile and save the update to the database.
  // The modification date should change to reflect the update.
  profile.SetInfo(EMAIL_ADDRESS, ASCIIToUTF16("js@smith.xyz"));
  db.GetAutofillTable()->UpdateAutofillProfileMulti(profile);

  // Get the profile.
  ASSERT_TRUE(db.GetAutofillTable()->GetAutofillProfile(profile.guid(),
                                                        &tmp_profile));
  db_profile.reset(tmp_profile);
  EXPECT_EQ(profile, *db_profile);
  sql::Statement s_updated(db.GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles"));
  ASSERT_TRUE(s_updated);
  ASSERT_TRUE(s_updated.Step());
  EXPECT_LT(mock_creation_date, s_updated.ColumnInt64(0));
  EXPECT_FALSE(s_updated.Step());

  // Set a mocked value for the profile's modification time.
  const time_t mock_modification_date = Time::Now().ToTimeT() - 7;
  sql::Statement s_mock_modification_date(
      db.GetSQLConnection()->GetUniqueStatement(
          "UPDATE autofill_profiles SET date_modified = ?"));
  ASSERT_TRUE(s_mock_modification_date);
  s_mock_modification_date.BindInt64(0, mock_modification_date);
  ASSERT_TRUE(s_mock_modification_date.Run());

  // Finally, call into |UpdateAutofillProfileMulti()| without changing the
  // profile.  The modification date should not change.
  db.GetAutofillTable()->UpdateAutofillProfileMulti(profile);

  // Get the profile.
  ASSERT_TRUE(db.GetAutofillTable()->GetAutofillProfile(profile.guid(),
                                                        &tmp_profile));
  db_profile.reset(tmp_profile);
  EXPECT_EQ(profile, *db_profile);
  sql::Statement s_unchanged(db.GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles"));
  ASSERT_TRUE(s_unchanged);
  ASSERT_TRUE(s_unchanged.Step());
  EXPECT_EQ(mock_modification_date, s_unchanged.ColumnInt64(0));
  EXPECT_FALSE(s_unchanged.Step());
}

TEST_F(AutofillTableTest, UpdateCreditCard) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  // Add a credit card to the db.
  CreditCard credit_card;
  credit_card.SetInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Jack Torrance"));
  credit_card.SetInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16("1234567890123456"));
  credit_card.SetInfo(CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("04"));
  credit_card.SetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, ASCIIToUTF16("2013"));
  db.GetAutofillTable()->AddCreditCard(credit_card);

  // Set a mocked value for the credit card's creation time.
  const time_t mock_creation_date = Time::Now().ToTimeT() - 13;
  sql::Statement s_mock_creation_date(db.GetSQLConnection()->GetUniqueStatement(
      "UPDATE credit_cards SET date_modified = ?"));
  ASSERT_TRUE(s_mock_creation_date);
  s_mock_creation_date.BindInt64(0, mock_creation_date);
  ASSERT_TRUE(s_mock_creation_date.Run());

  // Get the credit card.
  CreditCard* tmp_credit_card;
  ASSERT_TRUE(db.GetAutofillTable()->GetCreditCard(credit_card.guid(),
                                                   &tmp_credit_card));
  scoped_ptr<CreditCard> db_credit_card(tmp_credit_card);
  EXPECT_EQ(credit_card, *db_credit_card);
  sql::Statement s_original(db.GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_original);
  ASSERT_TRUE(s_original.Step());
  EXPECT_EQ(mock_creation_date, s_original.ColumnInt64(0));
  EXPECT_FALSE(s_original.Step());

  // Now, update the credit card and save the update to the database.
  // The modification date should change to reflect the update.
  credit_card.SetInfo(CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("01"));
  db.GetAutofillTable()->UpdateCreditCard(credit_card);

  // Get the credit card.
  ASSERT_TRUE(db.GetAutofillTable()->GetCreditCard(credit_card.guid(),
                                                   &tmp_credit_card));
  db_credit_card.reset(tmp_credit_card);
  EXPECT_EQ(credit_card, *db_credit_card);
  sql::Statement s_updated(db.GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_updated);
  ASSERT_TRUE(s_updated.Step());
  EXPECT_LT(mock_creation_date, s_updated.ColumnInt64(0));
  EXPECT_FALSE(s_updated.Step());

  // Set a mocked value for the credit card's modification time.
  const time_t mock_modification_date = Time::Now().ToTimeT() - 7;
  sql::Statement s_mock_modification_date(
      db.GetSQLConnection()->GetUniqueStatement(
          "UPDATE credit_cards SET date_modified = ?"));
  ASSERT_TRUE(s_mock_modification_date);
  s_mock_modification_date.BindInt64(0, mock_modification_date);
  ASSERT_TRUE(s_mock_modification_date.Run());

  // Finally, call into |UpdateCreditCard()| without changing the credit card.
  // The modification date should not change.
  db.GetAutofillTable()->UpdateCreditCard(credit_card);

  // Get the profile.
  ASSERT_TRUE(db.GetAutofillTable()->GetCreditCard(credit_card.guid(),
                                                   &tmp_credit_card));
  db_credit_card.reset(tmp_credit_card);
  EXPECT_EQ(credit_card, *db_credit_card);
  sql::Statement s_unchanged(db.GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_unchanged);
  ASSERT_TRUE(s_unchanged.Step());
  EXPECT_EQ(mock_modification_date, s_unchanged.ColumnInt64(0));
  EXPECT_FALSE(s_unchanged.Step());
}

TEST_F(AutofillTableTest, RemoveAutofillProfilesAndCreditCardsModifiedBetween) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  // Populate the autofill_profiles and credit_cards tables.
  ASSERT_TRUE(db.GetSQLConnection()->Execute(
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000000', 11);"
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000001', 21);"
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000002', 31);"
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000003', 41);"
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000004', 51);"
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000005', 61);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000006', 17);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000007', 27);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000008', 37);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000009', 47);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000010', 57);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000011', 67);"));

  // Remove all entries modified in the bounded time range [17,41).
  std::vector<std::string> profile_guids;
  std::vector<std::string> credit_card_guids;
  db.GetAutofillTable()->RemoveAutofillProfilesAndCreditCardsModifiedBetween(
      Time::FromTimeT(17), Time::FromTimeT(41),
      &profile_guids, &credit_card_guids);
  ASSERT_EQ(2UL, profile_guids.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000001", profile_guids[0]);
  EXPECT_EQ("00000000-0000-0000-0000-000000000002", profile_guids[1]);
  sql::Statement s_autofill_profiles_bounded(
      db.GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM autofill_profiles"));
  ASSERT_TRUE(s_autofill_profiles_bounded);
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(11, s_autofill_profiles_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(41, s_autofill_profiles_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(51, s_autofill_profiles_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(61, s_autofill_profiles_bounded.ColumnInt64(0));
  EXPECT_FALSE(s_autofill_profiles_bounded.Step());
  ASSERT_EQ(3UL, credit_card_guids.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000006", credit_card_guids[0]);
  EXPECT_EQ("00000000-0000-0000-0000-000000000007", credit_card_guids[1]);
  EXPECT_EQ("00000000-0000-0000-0000-000000000008", credit_card_guids[2]);
  sql::Statement s_credit_cards_bounded(
      db.GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_credit_cards_bounded);
  ASSERT_TRUE(s_credit_cards_bounded.Step());
  EXPECT_EQ(47, s_credit_cards_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_credit_cards_bounded.Step());
  EXPECT_EQ(57, s_credit_cards_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_credit_cards_bounded.Step());
  EXPECT_EQ(67, s_credit_cards_bounded.ColumnInt64(0));
  EXPECT_FALSE(s_credit_cards_bounded.Step());

  // Remove all entries modified on or after time 51 (unbounded range).
  db.GetAutofillTable()->RemoveAutofillProfilesAndCreditCardsModifiedBetween(
      Time::FromTimeT(51), Time(),
      &profile_guids, &credit_card_guids);
  ASSERT_EQ(2UL, profile_guids.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000004", profile_guids[0]);
  EXPECT_EQ("00000000-0000-0000-0000-000000000005", profile_guids[1]);
  sql::Statement s_autofill_profiles_unbounded(
      db.GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM autofill_profiles"));
  ASSERT_TRUE(s_autofill_profiles_unbounded);
  ASSERT_TRUE(s_autofill_profiles_unbounded.Step());
  EXPECT_EQ(11, s_autofill_profiles_unbounded.ColumnInt64(0));
  ASSERT_TRUE(s_autofill_profiles_unbounded.Step());
  EXPECT_EQ(41, s_autofill_profiles_unbounded.ColumnInt64(0));
  EXPECT_FALSE(s_autofill_profiles_unbounded.Step());
  ASSERT_EQ(2UL, credit_card_guids.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000010", credit_card_guids[0]);
  EXPECT_EQ("00000000-0000-0000-0000-000000000011", credit_card_guids[1]);
  sql::Statement s_credit_cards_unbounded(
      db.GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_credit_cards_unbounded);
  ASSERT_TRUE(s_credit_cards_unbounded.Step());
  EXPECT_EQ(47, s_credit_cards_unbounded.ColumnInt64(0));
  EXPECT_FALSE(s_credit_cards_unbounded.Step());

  // Remove all remaining entries.
  db.GetAutofillTable()->RemoveAutofillProfilesAndCreditCardsModifiedBetween(
      Time(), Time(),
      &profile_guids, &credit_card_guids);
  ASSERT_EQ(2UL, profile_guids.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000000", profile_guids[0]);
  EXPECT_EQ("00000000-0000-0000-0000-000000000003", profile_guids[1]);
  sql::Statement s_autofill_profiles_empty(
      db.GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM autofill_profiles"));
  ASSERT_TRUE(s_autofill_profiles_empty);
  EXPECT_FALSE(s_autofill_profiles_empty.Step());
  ASSERT_EQ(1UL, credit_card_guids.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000009", credit_card_guids[0]);
  sql::Statement s_credit_cards_empty(
      db.GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_credit_cards_empty);
  EXPECT_FALSE(s_credit_cards_empty.Step());
}

TEST_F(AutofillTableTest, Autofill_GetAllAutofillEntries_NoResults) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  std::vector<AutofillEntry> entries;
  ASSERT_TRUE(db.GetAutofillTable()->GetAllAutofillEntries(&entries));

  EXPECT_EQ(0U, entries.size());
}

TEST_F(AutofillTableTest, Autofill_GetAllAutofillEntries_OneResult) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  AutofillChangeList changes;
  std::map<std::string, std::vector<Time> > name_value_times_map;

  time_t start = 0;
  std::vector<Time> timestamps1;
  FormField field;
  field.name = ASCIIToUTF16("Name");
  field.value = ASCIIToUTF16("Superman");
  EXPECT_TRUE(
      db.GetAutofillTable()->AddFormFieldValueTime(field, &changes,
                                                   Time::FromTimeT(start)));
  timestamps1.push_back(Time::FromTimeT(start));
  std::string key1("NameSuperman");
  name_value_times_map.insert(std::pair<std::string,
    std::vector<Time> > (key1, timestamps1));

  AutofillEntrySet expected_entries(CompareAutofillEntries);
  AutofillKey ak1(ASCIIToUTF16("Name"), ASCIIToUTF16("Superman"));
  AutofillEntry ae1(ak1, timestamps1);

  expected_entries.insert(ae1);

  std::vector<AutofillEntry> entries;
  ASSERT_TRUE(db.GetAutofillTable()->GetAllAutofillEntries(&entries));
  AutofillEntrySet entry_set(entries.begin(), entries.end(),
    CompareAutofillEntries);

  // make sure the lists of entries match
  ASSERT_EQ(expected_entries.size(), entry_set.size());
  AutofillEntrySetIterator it;
  for (it = entry_set.begin(); it != entry_set.end(); it++) {
    expected_entries.erase(*it);
  }

  EXPECT_EQ(0U, expected_entries.size());
}

TEST_F(AutofillTableTest, Autofill_GetAllAutofillEntries_TwoDistinct) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  AutofillChangeList changes;
  std::map<std::string, std::vector<Time> > name_value_times_map;
  time_t start = 0;

  std::vector<Time> timestamps1;
  FormField field;
  field.name = ASCIIToUTF16("Name");
  field.value = ASCIIToUTF16("Superman");
  EXPECT_TRUE(
      db.GetAutofillTable()->AddFormFieldValueTime(field, &changes,
                                                   Time::FromTimeT(start)));
  timestamps1.push_back(Time::FromTimeT(start));
  std::string key1("NameSuperman");
  name_value_times_map.insert(std::pair<std::string,
    std::vector<Time> > (key1, timestamps1));

  start++;
  std::vector<Time> timestamps2;
  field.name = ASCIIToUTF16("Name");
  field.value = ASCIIToUTF16("Clark Kent");
  EXPECT_TRUE(
      db.GetAutofillTable()->AddFormFieldValueTime(field, &changes,
                                                   Time::FromTimeT(start)));
  timestamps2.push_back(Time::FromTimeT(start));
  std::string key2("NameClark Kent");
  name_value_times_map.insert(std::pair<std::string,
    std::vector<Time> > (key2, timestamps2));

  AutofillEntrySet expected_entries(CompareAutofillEntries);
  AutofillKey ak1(ASCIIToUTF16("Name"), ASCIIToUTF16("Superman"));
  AutofillKey ak2(ASCIIToUTF16("Name"), ASCIIToUTF16("Clark Kent"));
  AutofillEntry ae1(ak1, timestamps1);
  AutofillEntry ae2(ak2, timestamps2);

  expected_entries.insert(ae1);
  expected_entries.insert(ae2);

  std::vector<AutofillEntry> entries;
  ASSERT_TRUE(db.GetAutofillTable()->GetAllAutofillEntries(&entries));
  AutofillEntrySet entry_set(entries.begin(), entries.end(),
    CompareAutofillEntries);

  // make sure the lists of entries match
  ASSERT_EQ(expected_entries.size(), entry_set.size());
  AutofillEntrySetIterator it;
  for (it = entry_set.begin(); it != entry_set.end(); it++) {
    expected_entries.erase(*it);
  }

  EXPECT_EQ(0U, expected_entries.size());
}

TEST_F(AutofillTableTest, Autofill_GetAllAutofillEntries_TwoSame) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  AutofillChangeList changes;
  std::map<std::string, std::vector<Time> > name_value_times_map;

  time_t start = 0;
  std::vector<Time> timestamps;
  for (int i = 0; i < 2; i++) {
    FormField field;
    field.name = ASCIIToUTF16("Name");
    field.value = ASCIIToUTF16("Superman");
    EXPECT_TRUE(
        db.GetAutofillTable()->AddFormFieldValueTime(field, &changes,
                                                     Time::FromTimeT(start)));
    timestamps.push_back(Time::FromTimeT(start));
    start++;
  }

  std::string key("NameSuperman");
  name_value_times_map.insert(std::pair<std::string,
      std::vector<Time> > (key, timestamps));

  AutofillEntrySet expected_entries(CompareAutofillEntries);
  AutofillKey ak1(ASCIIToUTF16("Name"), ASCIIToUTF16("Superman"));
  AutofillEntry ae1(ak1, timestamps);

  expected_entries.insert(ae1);

  std::vector<AutofillEntry> entries;
  ASSERT_TRUE(db.GetAutofillTable()->GetAllAutofillEntries(&entries));
  AutofillEntrySet entry_set(entries.begin(), entries.end(),
    CompareAutofillEntries);

  // make sure the lists of entries match
  ASSERT_EQ(expected_entries.size(), entry_set.size());
  AutofillEntrySetIterator it;
  for (it = entry_set.begin(); it != entry_set.end(); it++) {
    expected_entries.erase(*it);
  }

  EXPECT_EQ(0U, expected_entries.size());
}
