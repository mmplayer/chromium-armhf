// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_warning_set.h"

#include "chrome/browser/extensions/extension_global_error_badge.h"
#include "chrome/browser/ui/global_error_service.h"
#include "chrome/browser/ui/global_error_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/common/notification_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

// This class is used to represent warnings if extensions misbehave.
class ExtensionWarning {
 public:
  // Default constructor for storing ExtensionServiceWarning in STL containers
  // do not use.
  ExtensionWarning();

  // Constructs a warning of type |type| for extension |extension_id|. This
  // could be for example the fact that an extension conflicted with others.
  ExtensionWarning(ExtensionWarningSet::WarningType type,
                   const std::string& extension_id);

  ~ExtensionWarning();

  // Returns the specific warning type.
  ExtensionWarningSet::WarningType warning_type() const { return type_; }

  // Returns the id of the extension for which this warning is valid.
  const std::string& extension_id() const { return extension_id_; }

 private:
  ExtensionWarningSet::WarningType type_;
  std::string extension_id_;

  // Allow implicit copy and assign operator.
};

ExtensionWarning::ExtensionWarning() : type_(ExtensionWarningSet::kInvalid) {
}

ExtensionWarning::ExtensionWarning(
    ExtensionWarningSet::WarningType type,
    const std::string& extension_id)
    : type_(type), extension_id_(extension_id) {
}

ExtensionWarning::~ExtensionWarning() {
}

bool operator<(const ExtensionWarning& a, const ExtensionWarning& b) {
  if (a.warning_type() == b.warning_type())
    return a.extension_id() < b.extension_id();
  return a.warning_type() < b.warning_type();
}

// Static
string16 ExtensionWarningSet::GetLocalizedWarning(
    ExtensionWarningSet::WarningType warning_type) {
  switch (warning_type) {
    case ExtensionWarningSet::kInvalid:
    case ExtensionWarningSet::kMaxWarningType:
      NOTREACHED();
      return string16();
    case ExtensionWarningSet::kNetworkDelay:
      return l10n_util::GetStringFUTF16(
          IDS_EXTENSION_WARNINGS_NETWORK_DELAY,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  }
  NOTREACHED();  // Switch statement has no default branch.
  return string16();
}

ExtensionWarningSet::ExtensionWarningSet(Profile* profile)
    : extension_global_error_badge_(NULL), profile_(profile) {
}

ExtensionWarningSet::~ExtensionWarningSet() {
}

void ExtensionWarningSet::SetWarning(ExtensionWarningSet::WarningType type,
                                     const std::string& extension_id) {
  ExtensionWarning warning(type, extension_id);
  bool inserted = warnings_.insert(warning).second;
  if (inserted) {
    NotifyWarningsChanged();
    UpdateWarningBadge();
  }
}

void ExtensionWarningSet::ClearWarnings(
    const std::set<ExtensionWarningSet::WarningType>& types) {
  bool deleted_anything = false;
  for (const_iterator i = warnings_.begin(); i != warnings_.end();) {
    if (types.find(i->warning_type()) != types.end()) {
      deleted_anything = true;
      warnings_.erase(i++);
    } else {
      ++i;
    }
  }

  if (deleted_anything) {
    NotifyWarningsChanged();
    UpdateWarningBadge();
  }
}

void ExtensionWarningSet::GetWarningsAffectingExtension(
    const std::string& extension_id,
    std::set<ExtensionWarningSet::WarningType>* result) const {
  result->clear();
  for (const_iterator i = warnings_.begin(); i != warnings_.end(); ++i) {
    if (i->extension_id() == extension_id)
      result->insert(i->warning_type());
  }
}

void ExtensionWarningSet::SuppressBadgeForCurrentWarnings() {
  badge_suppressions_.insert(warnings_.begin(), warnings_.end());
  UpdateWarningBadge();
}

void ExtensionWarningSet::NotifyWarningsChanged() {
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_WARNING_CHANGED,
      Source<Profile>(profile_),
      NotificationService::NoDetails());
}

void ExtensionWarningSet::ActivateBadge() {
  DCHECK(!extension_global_error_badge_);
  DCHECK(profile_);
  extension_global_error_badge_ = new ExtensionGlobalErrorBadge;
  GlobalErrorServiceFactory::GetForProfile(profile_)->AddGlobalError(
      extension_global_error_badge_);
}

void ExtensionWarningSet::DeactivateBadge() {
  DCHECK(extension_global_error_badge_);
  DCHECK(profile_);
  GlobalErrorServiceFactory::GetForProfile(profile_)->RemoveGlobalError(
      extension_global_error_badge_);
  delete extension_global_error_badge_;
  extension_global_error_badge_ = NULL;
}

void ExtensionWarningSet::UpdateWarningBadge() {
  // We need a badge if a warning exists that has not been suppressed.
  bool need_warning_badge = false;
  for (const_iterator i = warnings_.begin(); i != warnings_.end(); ++i) {
    if (badge_suppressions_.find(*i) == badge_suppressions_.end()) {
      need_warning_badge = true;
      break;
    }
  }

  // Activate or hide the warning badge in case the current state is incorrect.
  if (extension_global_error_badge_ && !need_warning_badge)
    DeactivateBadge();
  else if (!extension_global_error_badge_ && need_warning_badge)
    ActivateBadge();
}
