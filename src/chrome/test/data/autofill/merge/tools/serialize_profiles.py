# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sqlite3
import sys

from autofill_merge_common import SerializeProfiles, ColumnNameToFieldType

def main():
  """Serializes the autofill_profiles table from the specified database."""

  if len(sys.argv) != 2:
    print "Usage: python serialize_profiles.py <path/to/database>"
    return

  database = sys.argv[1]
  if not os.path.isfile(database):
    print "Cannot read database at \"%s\"" % database
    return

  # Read the autofill_profile_names table.
  try:
    connection = sqlite3.connect(database, 0)
    cursor = connection.cursor()
    cursor.execute("SELECT * from autofill_profile_names;")
  except sqlite3.OperationalError:
    print ("Failed to read the autofill_profile_names table from \"%s\"" %
           database)
    raise

  # For backward-compatibility, the result of |cursor.description| is a list of
  # 7-tuples, in which the first item is the column name, and the remaining
  # items are 'None'.
  types = [ColumnNameToFieldType(item[0]) for item in cursor.description]
  profiles = {}
  for profile in cursor:
    guid = profile[0]
    profiles[guid] = zip(types, profile)

  # Read the autofill_profile_emails table.
  try:
    cursor.execute("SELECT * from autofill_profile_emails;")
  except sqlite3.OperationalError:
    print ("Failed to read the autofill_profile_emails table from \"%s\"" %
           database)
    raise

  types = [ColumnNameToFieldType(item[0]) for item in cursor.description]
  for profile in cursor:
    guid = profile[0]
    profiles[guid].extend(zip(types, profile))

  # Read the autofill_profiles table.
  try:
    cursor.execute("SELECT * from autofill_profiles;")
  except sqlite3.OperationalError:
    print "Failed to read the autofill_profiles table from \"%s\"" % database
    raise

  types = [ColumnNameToFieldType(item[0]) for item in cursor.description]
  for profile in cursor:
    guid = profile[0]
    profiles[guid].extend(zip(types, profile))

  # Read the autofill_profile_phones table.
  try:
    cursor.execute("SELECT * from autofill_profile_phones;")
  except sqlite3.OperationalError:
    print ("Failed to read the autofill_profile_phones table from \"%s\"" %
           database)
    raise

  for profile in cursor:
    guid = profile[0]
    profiles[guid].append(("PHONE_HOME_WHOLE_NUMBER", profile[2]))

  print SerializeProfiles(profiles.values())


if __name__ == '__main__':
  main()