// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/mac/keystone_glue.h"

#include <sys/param.h>
#include <sys/mount.h>

#include <vector>

#include "base/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/mac/scoped_nsexception_enabler.h"
#include "base/memory/ref_counted.h"
#include "base/sys_string_conversions.h"
#include "base/task.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/mac/authorization_util.h"
#import "chrome/browser/mac/keystone_registration.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_version_info.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

namespace ksr = keystone_registration;

// Constants for the brand file (uses an external file so it can survive
// updates to Chrome.)

#if defined(GOOGLE_CHROME_BUILD)
#define kBrandFileName @"Google Chrome Brand.plist";
#elif defined(CHROMIUM_BUILD)
#define kBrandFileName @"Chromium Brand.plist";
#else
#error Unknown branding
#endif

// These directories are hardcoded in Keystone promotion preflight and the
// Keystone install script, so NSSearchPathForDirectoriesInDomains isn't used
// since the scripts couldn't use anything like that.
NSString* kBrandUserFile = @"~/Library/Google/" kBrandFileName;
NSString* kBrandSystemFile = @"/Library/Google/" kBrandFileName;

NSString* UserBrandFilePath() {
  return [kBrandUserFile stringByStandardizingPath];
}
NSString* SystemBrandFilePath() {
  return [kBrandSystemFile stringByStandardizingPath];
}

// Adaptor for scheduling an Objective-C method call on a |WorkerPool|
// thread.
class PerformBridge : public base::RefCountedThreadSafe<PerformBridge> {
 public:

  // Call |sel| on |target| with |arg| in a WorkerPool thread.
  // |target| and |arg| are retained, |arg| may be |nil|.
  static void PostPerform(id target, SEL sel, id arg) {
    DCHECK(target);
    DCHECK(sel);

    scoped_refptr<PerformBridge> op = new PerformBridge(target, sel, arg);
    base::WorkerPool::PostTask(
        FROM_HERE, NewRunnableMethod(op.get(), &PerformBridge::Run), true);
  }

  // Convenience for the no-argument case.
  static void PostPerform(id target, SEL sel) {
    PostPerform(target, sel, nil);
  }

 private:
  // Allow RefCountedThreadSafe<> to delete.
  friend class base::RefCountedThreadSafe<PerformBridge>;

  PerformBridge(id target, SEL sel, id arg)
      : target_([target retain]),
        sel_(sel),
        arg_([arg retain]) {
  }

  ~PerformBridge() {}

  // Happens on a WorkerPool thread.
  void Run() {
    base::mac::ScopedNSAutoreleasePool pool;
    [target_ performSelector:sel_ withObject:arg_];
  }

  scoped_nsobject<id> target_;
  SEL sel_;
  scoped_nsobject<id> arg_;
};

}  // namespace

@interface KeystoneGlue (Private)

// Returns the path to the application's Info.plist file.  This returns the
// outer application bundle's Info.plist, not the framework's Info.plist.
- (NSString*)appInfoPlistPath;

// Returns a dictionary containing parameters to be used for a KSRegistration
// -registerWithParameters: or -promoteWithParameters:authorization: call.
- (NSDictionary*)keystoneParameters;

// Called when Keystone registration completes.
- (void)registrationComplete:(NSNotification*)notification;

// Called periodically to announce activity by pinging the Keystone server.
- (void)markActive:(NSTimer*)timer;

// Called when an update check or update installation is complete.  Posts the
// kAutoupdateStatusNotification notification to the default notification
// center.
- (void)updateStatus:(AutoupdateStatus)status version:(NSString*)version;

// Returns the version of the currently-installed application on disk.
- (NSString*)currentlyInstalledVersion;

// These three methods are used to determine the version of the application
// currently installed on disk, compare that to the currently-running version,
// decide whether any updates have been installed, and call
// -updateStatus:version:.
//
// In order to check the version on disk, the installed application's
// Info.plist dictionary must be read; in order to see changes as updates are
// applied, the dictionary must be read each time, bypassing any caches such
// as the one that NSBundle might be maintaining.  Reading files can be a
// blocking operation, and blocking operations are to be avoided on the main
// thread.  I'm not quite sure what jank means, but I bet that a blocked main
// thread would cause some of it.
//
// -determineUpdateStatusAsync is called on the main thread to initiate the
// operation.  It performs initial set-up work that must be done on the main
// thread and arranges for -determineUpdateStatus to be called on a work queue
// thread managed by WorkerPool.
// -determineUpdateStatus then reads the Info.plist, gets the version from the
// CFBundleShortVersionString key, and performs
// -determineUpdateStatusForVersion: on the main thread.
// -determineUpdateStatusForVersion: does the actual comparison of the version
// on disk with the running version and calls -updateStatus:version: with the
// results of its analysis.
- (void)determineUpdateStatusAsync;
- (void)determineUpdateStatus;
- (void)determineUpdateStatusForVersion:(NSString*)version;

// Returns YES if registration_ is definitely on a user ticket.  If definitely
// on a system ticket, or uncertain of ticket type (due to an older version
// of Keystone being used), returns NO.
- (BOOL)isUserTicket;

// Returns YES if Keystone is definitely installed at the system level,
// determined by the presence of an executable ksadmin program at the expected
// system location.
- (BOOL)isSystemKeystone;

// Returns YES if on a system ticket but system Keystone is not present.
// Returns NO otherwise. The "doomed" condition will result in the
// registration framework appearing to have registered Chrome, but no updates
// ever actually taking place.
- (BOOL)isSystemTicketDoomed;

// Called when ticket promotion completes.
- (void)promotionComplete:(NSNotification*)notification;

// Changes the application's ownership and permissions so that all files are
// owned by root:wheel and all files and directories are writable only by
// root, but readable and executable as needed by everyone.
// -changePermissionsForPromotionAsync is called on the main thread by
// -promotionComplete.  That routine calls
// -changePermissionsForPromotionWithTool: on a work queue thread.  When done,
// -changePermissionsForPromotionComplete is called on the main thread.
- (void)changePermissionsForPromotionAsync;
- (void)changePermissionsForPromotionWithTool:(NSString*)toolPath;
- (void)changePermissionsForPromotionComplete;

// Returns the brand file path to use for Keystone.
- (NSString*)brandFilePath;

@end  // @interface KeystoneGlue (Private)

NSString* const kAutoupdateStatusNotification = @"AutoupdateStatusNotification";
NSString* const kAutoupdateStatusStatus = @"status";
NSString* const kAutoupdateStatusVersion = @"version";

namespace {

NSString* const kChannelKey = @"KSChannelID";
NSString* const kBrandKey = @"KSBrandID";
NSString* const kVersionKey = @"KSVersion";

}  // namespace

@implementation KeystoneGlue

+ (id)defaultKeystoneGlue {
  static bool sTriedCreatingDefaultKeystoneGlue = false;
  // TODO(jrg): use base::SingletonObjC<KeystoneGlue>
  static KeystoneGlue* sDefaultKeystoneGlue = nil;  // leaked

  if (!sTriedCreatingDefaultKeystoneGlue) {
    sTriedCreatingDefaultKeystoneGlue = true;

    sDefaultKeystoneGlue = [[KeystoneGlue alloc] init];
    [sDefaultKeystoneGlue loadParameters];
    if (![sDefaultKeystoneGlue loadKeystoneRegistration]) {
      [sDefaultKeystoneGlue release];
      sDefaultKeystoneGlue = nil;
    }
  }
  return sDefaultKeystoneGlue;
}

- (id)init {
  if ((self = [super init])) {
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];

    [center addObserver:self
               selector:@selector(registrationComplete:)
                   name:ksr::KSRegistrationDidCompleteNotification
                 object:nil];

    [center addObserver:self
               selector:@selector(promotionComplete:)
                   name:ksr::KSRegistrationPromotionDidCompleteNotification
                 object:nil];

    [center addObserver:self
               selector:@selector(checkForUpdateComplete:)
                   name:ksr::KSRegistrationCheckForUpdateNotification
                 object:nil];

    [center addObserver:self
               selector:@selector(installUpdateComplete:)
                   name:ksr::KSRegistrationStartUpdateNotification
                 object:nil];
  }

  return self;
}

- (void)dealloc {
  [productID_ release];
  [appPath_ release];
  [url_ release];
  [version_ release];
  [channel_ release];
  [registration_ release];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (NSDictionary*)infoDictionary {
  // Use [NSBundle mainBundle] to get the application's own bundle identifier
  // and path, not the framework's.  For auto-update, the application is
  // what's significant here: it's used to locate the outermost part of the
  // application for the existence checker and other operations that need to
  // see the entire application bundle.
  return [[NSBundle mainBundle] infoDictionary];
}

- (void)loadParameters {
  NSBundle* appBundle = [NSBundle mainBundle];
  NSDictionary* infoDictionary = [self infoDictionary];

  NSString* productID = [infoDictionary objectForKey:@"KSProductID"];
  if (productID == nil) {
    productID = [appBundle bundleIdentifier];
  }

  NSString* appPath = [appBundle bundlePath];
  NSString* url = [infoDictionary objectForKey:@"KSUpdateURL"];
  NSString* version = [infoDictionary objectForKey:kVersionKey];

  if (!productID || !appPath || !url || !version) {
    // If parameters required for Keystone are missing, don't use it.
    return;
  }

  NSString* channel = [infoDictionary objectForKey:kChannelKey];
  // The stable channel has no tag.  If updating to stable, remove the
  // dev and beta tags since we've been "promoted".
  if (channel == nil)
    channel = ksr::KSRegistrationRemoveExistingTag;

  productID_ = [productID retain];
  appPath_ = [appPath retain];
  url_ = [url retain];
  version_ = [version retain];
  channel_ = [channel retain];
}

- (NSString*)brandFilePath {
  DCHECK(version_ != nil) << "-loadParameters must be called first";

  if (brandFileType_ == kBrandFileTypeNotDetermined) {

    NSFileManager* fm = [NSFileManager defaultManager];
    NSString* userBrandFile = UserBrandFilePath();
    NSString* systemBrandFile = SystemBrandFilePath();

    // Default to none.
    brandFileType_ = kBrandFileTypeNone;

    // Only the stable channel has a brand code.
    chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();

    if (channel == chrome::VersionInfo::CHANNEL_DEV ||
        channel == chrome::VersionInfo::CHANNEL_BETA) {

      // If on the dev or beta channel, this installation may have replaced
      // an older system-level installation. Check for a user brand file and
      // nuke it if present. Don't try to remove the system brand file, there
      // wouldn't be any permission to do so.
      //
      // Don't do this on the canary channel. The canary can run side-by-side
      // with another Google Chrome installation whose brand code, if any,
      // should remain intact.

      if ([fm fileExistsAtPath:userBrandFile]) {
        [fm removeItemAtPath:userBrandFile error:NULL];
      }

    } else if (channel == chrome::VersionInfo::CHANNEL_STABLE) {

      // If there is a system brand file, use it.
      if ([fm fileExistsAtPath:systemBrandFile]) {
        // System

        // Use the system file that is there.
        brandFileType_ = kBrandFileTypeSystem;

        // Clean up any old user level file.
        if ([fm fileExistsAtPath:userBrandFile]) {
          [fm removeItemAtPath:userBrandFile error:NULL];
        }

      } else {
        // User

        NSDictionary* infoDictionary = [self infoDictionary];
        NSString* appBundleBrandID = [infoDictionary objectForKey:kBrandKey];

        NSString* storedBrandID = nil;
        if ([fm fileExistsAtPath:userBrandFile]) {
          NSDictionary* storedBrandDict =
              [NSDictionary dictionaryWithContentsOfFile:userBrandFile];
          storedBrandID = [storedBrandDict objectForKey:kBrandKey];
        }

        if ((appBundleBrandID != nil) &&
            (![storedBrandID isEqualTo:appBundleBrandID])) {
          // App and store don't match, update store and use it.
          NSDictionary* storedBrandDict =
              [NSDictionary dictionaryWithObject:appBundleBrandID
                                          forKey:kBrandKey];
          // If Keystone hasn't been installed yet, the location the brand file
          // is written to won't exist, so manually create the directory.
          NSString *userBrandFileDirectory =
              [userBrandFile stringByDeletingLastPathComponent];
          if (![fm fileExistsAtPath:userBrandFileDirectory]) {
            if (![fm createDirectoryAtPath:userBrandFileDirectory
               withIntermediateDirectories:YES
                                attributes:nil
                                     error:NULL]) {
              LOG(ERROR) << "Failed to create the directory for the brand file";
            }
          }
          if ([storedBrandDict writeToFile:userBrandFile atomically:YES]) {
            brandFileType_ = kBrandFileTypeUser;
          }
        } else if (storedBrandID) {
          // Had stored brand, use it.
          brandFileType_ = kBrandFileTypeUser;
        }
      }
    }

  }

  NSString* result = nil;
  switch (brandFileType_) {
    case kBrandFileTypeUser:
      result = UserBrandFilePath();
      break;

    case kBrandFileTypeSystem:
      result = SystemBrandFilePath();
      break;

    case kBrandFileTypeNotDetermined:
      NOTIMPLEMENTED();
      // Fall through
    case kBrandFileTypeNone:
      // Clear the value.
      result = @"";
      break;

  }
  return result;
}

- (BOOL)loadKeystoneRegistration {
  if (!productID_ || !appPath_ || !url_ || !version_)
    return NO;

  // Load the KeystoneRegistration framework bundle if present.  It lives
  // inside the framework, so use base::mac::MainAppBundle();
  NSString* ksrPath =
      [[base::mac::MainAppBundle() privateFrameworksPath]
          stringByAppendingPathComponent:@"KeystoneRegistration.framework"];
  NSBundle* ksrBundle = [NSBundle bundleWithPath:ksrPath];
  [ksrBundle load];

  // Harness the KSRegistration class.
  Class ksrClass = [ksrBundle classNamed:@"KSRegistration"];
  KSRegistration* ksr = [ksrClass registrationWithProductID:productID_];
  if (!ksr)
    return NO;

  registration_ = [ksr retain];
  return YES;
}

- (NSString*)appInfoPlistPath {
  // NSBundle ought to have a way to access this path directly, but it
  // doesn't.
  return [[appPath_ stringByAppendingPathComponent:@"Contents"]
             stringByAppendingPathComponent:@"Info.plist"];
}

- (NSDictionary*)keystoneParameters {
  NSNumber* xcType = [NSNumber numberWithInt:ksr::kKSPathExistenceChecker];
  NSNumber* preserveTTToken = [NSNumber numberWithBool:YES];
  NSString* appInfoPlistPath = [self appInfoPlistPath];
  NSString* brandKey = kBrandKey;
  NSString* brandPath = [self brandFilePath];

  if ([brandPath length] == 0) {
    // Brand path and brand key must be cleared together or ksadmin seems
    // to throw an error.
    brandKey = @"";
  }

  return [NSDictionary dictionaryWithObjectsAndKeys:
             version_, ksr::KSRegistrationVersionKey,
             appInfoPlistPath, ksr::KSRegistrationVersionPathKey,
             kVersionKey, ksr::KSRegistrationVersionKeyKey,
             xcType, ksr::KSRegistrationExistenceCheckerTypeKey,
             appPath_, ksr::KSRegistrationExistenceCheckerStringKey,
             url_, ksr::KSRegistrationServerURLStringKey,
             preserveTTToken, ksr::KSRegistrationPreserveTrustedTesterTokenKey,
             channel_, ksr::KSRegistrationTagKey,
             appInfoPlistPath, ksr::KSRegistrationTagPathKey,
             kChannelKey, ksr::KSRegistrationTagKeyKey,
             brandPath, ksr::KSRegistrationBrandPathKey,
             brandKey, ksr::KSRegistrationBrandKeyKey,
             nil];
}

- (void)registerWithKeystone {
  [self updateStatus:kAutoupdateRegistering version:nil];

  NSDictionary* parameters = [self keystoneParameters];
  BOOL result;
  {
    // TODO(shess): Allows Keystone to throw an exception when
    // /usr/bin/python does not exist (really!).
    // http://crbug.com/86221 and http://crbug.com/87931
    base::mac::ScopedNSExceptionEnabler enabler;
    result = [registration_ registerWithParameters:parameters];
  }
  if (!result) {
    [self updateStatus:kAutoupdateRegisterFailed version:nil];
    return;
  }

  // Upon completion, ksr::KSRegistrationDidCompleteNotification will be
  // posted, and -registrationComplete: will be called.

  // Mark an active RIGHT NOW; don't wait an hour for the first one.
  [registration_ setActive];

  // Set up hourly activity pings.
  timer_ = [NSTimer scheduledTimerWithTimeInterval:60 * 60  // One hour
                                            target:self
                                          selector:@selector(markActive:)
                                          userInfo:registration_
                                           repeats:YES];
}

- (void)registrationComplete:(NSNotification*)notification {
  NSDictionary* userInfo = [notification userInfo];
  if ([[userInfo objectForKey:ksr::KSRegistrationStatusKey] boolValue]) {
    if ([self isSystemTicketDoomed]) {
      [self updateStatus:kAutoupdateNeedsPromotion version:nil];
    } else {
      [self updateStatus:kAutoupdateRegistered version:nil];
    }
  } else {
    // Dump registration_?
    [self updateStatus:kAutoupdateRegisterFailed version:nil];
  }
}

- (void)stopTimer {
  [timer_ invalidate];
}

- (void)markActive:(NSTimer*)timer {
  KSRegistration* ksr = [timer userInfo];
  [ksr setActive];
}

- (void)checkForUpdate {
  DCHECK(![self asyncOperationPending]);

  if (!registration_) {
    [self updateStatus:kAutoupdateCheckFailed version:nil];
    return;
  }

  [self updateStatus:kAutoupdateChecking version:nil];

  [registration_ checkForUpdate];

  // Upon completion, ksr::KSRegistrationCheckForUpdateNotification will be
  // posted, and -checkForUpdateComplete: will be called.
}

- (void)checkForUpdateComplete:(NSNotification*)notification {
  NSDictionary* userInfo = [notification userInfo];

  if ([[userInfo objectForKey:ksr::KSRegistrationUpdateCheckErrorKey]
          boolValue]) {
    [self updateStatus:kAutoupdateCheckFailed version:nil];
  } else if ([[userInfo objectForKey:ksr::KSRegistrationStatusKey] boolValue]) {
    // If an update is known to be available, go straight to
    // -updateStatus:version:.  It doesn't matter what's currently on disk.
    NSString* version = [userInfo objectForKey:ksr::KSRegistrationVersionKey];
    [self updateStatus:kAutoupdateAvailable version:version];
  } else {
    // If no updates are available, check what's on disk, because an update
    // may have already been installed.  This check happens on another thread,
    // and -updateStatus:version: will be called on the main thread when done.
    [self determineUpdateStatusAsync];
  }
}

- (void)installUpdate {
  DCHECK(![self asyncOperationPending]);

  if (!registration_) {
    [self updateStatus:kAutoupdateInstallFailed version:nil];
    return;
  }

  [self updateStatus:kAutoupdateInstalling version:nil];

  [registration_ startUpdate];

  // Upon completion, ksr::KSRegistrationStartUpdateNotification will be
  // posted, and -installUpdateComplete: will be called.
}

- (void)installUpdateComplete:(NSNotification*)notification {
  NSDictionary* userInfo = [notification userInfo];

  if (![[userInfo objectForKey:ksr::KSUpdateCheckSuccessfulKey] boolValue] ||
      ![[userInfo objectForKey:ksr::KSUpdateCheckSuccessfullyInstalledKey]
          intValue]) {
    [self updateStatus:kAutoupdateInstallFailed version:nil];
  } else {
    updateSuccessfullyInstalled_ = YES;

    // Nothing in the notification dictionary reports the version that was
    // installed.  Figure it out based on what's on disk.
    [self determineUpdateStatusAsync];
  }
}

- (NSString*)currentlyInstalledVersion {
  NSString* appInfoPlistPath = [self appInfoPlistPath];
  NSDictionary* infoPlist =
      [NSDictionary dictionaryWithContentsOfFile:appInfoPlistPath];
  return [infoPlist objectForKey:@"CFBundleShortVersionString"];
}

// Runs on the main thread.
- (void)determineUpdateStatusAsync {
  DCHECK([NSThread isMainThread]);

  PerformBridge::PostPerform(self, @selector(determineUpdateStatus));
}

// Runs on a thread managed by WorkerPool.
- (void)determineUpdateStatus {
  DCHECK(![NSThread isMainThread]);

  NSString* version = [self currentlyInstalledVersion];

  [self performSelectorOnMainThread:@selector(determineUpdateStatusForVersion:)
                         withObject:version
                      waitUntilDone:NO];
}

// Runs on the main thread.
- (void)determineUpdateStatusForVersion:(NSString*)version {
  DCHECK([NSThread isMainThread]);

  AutoupdateStatus status;
  if (updateSuccessfullyInstalled_) {
    // If an update was successfully installed and this object saw it happen,
    // then don't even bother comparing versions.
    status = kAutoupdateInstalled;
  } else {
    NSString* currentVersion =
        [NSString stringWithUTF8String:chrome::kChromeVersion];
    if (!version) {
      // If the version on disk could not be determined, assume that
      // whatever's running is current.
      version = currentVersion;
      status = kAutoupdateCurrent;
    } else if ([version isEqualToString:currentVersion]) {
      status = kAutoupdateCurrent;
    } else {
      // If the version on disk doesn't match what's currently running, an
      // update must have been applied in the background, without this app's
      // direct participation.  Leave updateSuccessfullyInstalled_ alone
      // because there's no direct knowledge of what actually happened.
      status = kAutoupdateInstalled;
    }
  }

  [self updateStatus:status version:version];
}

- (void)updateStatus:(AutoupdateStatus)status version:(NSString*)version {
  NSNumber* statusNumber = [NSNumber numberWithInt:status];
  NSMutableDictionary* dictionary =
      [NSMutableDictionary dictionaryWithObject:statusNumber
                                         forKey:kAutoupdateStatusStatus];
  if (version) {
    [dictionary setObject:version forKey:kAutoupdateStatusVersion];
  }

  NSNotification* notification =
      [NSNotification notificationWithName:kAutoupdateStatusNotification
                                    object:self
                                  userInfo:dictionary];
  recentNotification_.reset([notification retain]);

  [[NSNotificationCenter defaultCenter] postNotification:notification];
}

- (NSNotification*)recentNotification {
  return [[recentNotification_ retain] autorelease];
}

- (AutoupdateStatus)recentStatus {
  NSDictionary* dictionary = [recentNotification_ userInfo];
  return static_cast<AutoupdateStatus>(
      [[dictionary objectForKey:kAutoupdateStatusStatus] intValue]);
}

- (BOOL)asyncOperationPending {
  AutoupdateStatus status = [self recentStatus];
  return status == kAutoupdateRegistering ||
         status == kAutoupdateChecking ||
         status == kAutoupdateInstalling ||
         status == kAutoupdatePromoting;
}

- (BOOL)isUserTicket {
  return [registration_ ticketType] == ksr::kKSRegistrationUserTicket;
}

- (BOOL)isSystemKeystone {
  struct stat statbuf;
  if (stat("/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/"
           "Contents/MacOS/ksadmin",
           &statbuf) != 0) {
    return NO;
  }

  if (!(statbuf.st_mode & S_IXUSR)) {
    return NO;
  }

  return YES;
}

- (BOOL)isSystemTicketDoomed {
  BOOL isSystemTicket = ![self isUserTicket];
  return isSystemTicket && ![self isSystemKeystone];
}

- (BOOL)isOnReadOnlyFilesystem {
  const char* appPathC = [appPath_ fileSystemRepresentation];
  struct statfs statfsBuf;

  if (statfs(appPathC, &statfsBuf) != 0) {
    PLOG(ERROR) << "statfs";
    // Be optimistic about the filesystem's writability.
    return NO;
  }

  return (statfsBuf.f_flags & MNT_RDONLY) != 0;
}

- (BOOL)needsPromotion {
  // Don't promote when on a read-only filesystem.
  if ([self isOnReadOnlyFilesystem]) {
    return NO;
  }

  // Promotion is required when a system ticket is present but system Keystone
  // is not.
  if ([self isSystemTicketDoomed]) {
    return YES;
  }

  // If on a system ticket and system Keystone is present, promotion is not
  // required.
  if (![self isUserTicket]) {
    return NO;
  }

  // Check the outermost bundle directory, the main executable path, and the
  // framework directory.  It may be enough to just look at the outermost
  // bundle directory, but checking an interior file and directory can be
  // helpful in case permissions are set differently only on the outermost
  // directory.  An interior file and directory are both checked because some
  // file operations, such as Snow Leopard's Finder's copy operation when
  // authenticating, may actually result in different ownership being applied
  // to files and directories.
  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSString* executablePath = [[NSBundle mainBundle] executablePath];
  NSString* frameworkPath = [base::mac::MainAppBundle() bundlePath];
  return ![fileManager isWritableFileAtPath:appPath_] ||
         ![fileManager isWritableFileAtPath:executablePath] ||
         ![fileManager isWritableFileAtPath:frameworkPath];
}

- (BOOL)wantsPromotion {
  if ([self needsPromotion]) {
    return YES;
  }

  // These are the same unpromotable cases as in -needsPromotion.
  if ([self isOnReadOnlyFilesystem] || ![self isUserTicket]) {
    return NO;
  }

  return [appPath_ hasPrefix:@"/Applications/"];
}

- (void)promoteTicket {
  if ([self asyncOperationPending] || ![self wantsPromotion]) {
    // Because there are multiple ways of reaching promoteTicket that might
    // not lock each other out, it may be possible to arrive here while an
    // asynchronous operation is pending, or even after promotion has already
    // occurred.  Just quietly return without doing anything.
    return;
  }

  NSString* prompt = l10n_util::GetNSStringFWithFixup(
      IDS_PROMOTE_AUTHENTICATION_PROMPT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  ScopedAuthorizationRef authorization(
      authorization_util::AuthorizationCreateToRunAsRoot(
          base::mac::NSToCFCast(prompt)));
  if (!authorization.get()) {
    return;
  }

  [self promoteTicketWithAuthorization:authorization.release() synchronous:NO];
}

- (void)promoteTicketWithAuthorization:(AuthorizationRef)authorization_arg
                           synchronous:(BOOL)synchronous {
  ScopedAuthorizationRef authorization(authorization_arg);
  authorization_arg = NULL;

  if ([self asyncOperationPending]) {
    // Starting a synchronous operation while an asynchronous one is pending
    // could be trouble.
    return;
  }
  if (!synchronous && ![self wantsPromotion]) {
    // If operating synchronously, the call came from the installer, which
    // means that a system ticket is required.  Otherwise, only allow
    // promotion if it's wanted.
    return;
  }

  synchronousPromotion_ = synchronous;

  [self updateStatus:kAutoupdatePromoting version:nil];

  // TODO(mark): Remove when able!
  //
  // keystone_promote_preflight will copy the current brand information out to
  // the system level so all users can share the data as part of the ticket
  // promotion.
  //
  // It will also ensure that the Keystone system ticket store is in a usable
  // state for all users on the system.  Ideally, Keystone's installer or
  // another part of Keystone would handle this.  The underlying problem is
  // http://b/2285921, and it causes http://b/2289908, which this workaround
  // addresses.
  //
  // This is run synchronously, which isn't optimal, but
  // -[KSRegistration promoteWithParameters:authorization:] is currently
  // synchronous too, and this operation needs to happen before that one.
  //
  // TODO(mark): Make asynchronous.  That only makes sense if the promotion
  // operation itself is asynchronous too.  http://b/2290009.  Hopefully,
  // the Keystone promotion code will just be changed to do what preflight
  // now does, and then the preflight script can be removed instead.
  // However, preflight operation (and promotion) should only be asynchronous
  // if the synchronous parameter is NO.
  NSString* preflightPath =
      [base::mac::MainAppBundle() pathForResource:@"keystone_promote_preflight"
                                           ofType:@"sh"];
  const char* preflightPathC = [preflightPath fileSystemRepresentation];
  const char* userBrandFile = NULL;
  const char* systemBrandFile = NULL;
  if (brandFileType_ == kBrandFileTypeUser) {
    // Running with user level brand file, promote to the system level.
    userBrandFile = [UserBrandFilePath() fileSystemRepresentation];
    systemBrandFile = [SystemBrandFilePath() fileSystemRepresentation];
  }
  const char* arguments[] = {userBrandFile, systemBrandFile, NULL};

  int exit_status;
  OSStatus status = authorization_util::ExecuteWithPrivilegesAndWait(
      authorization,
      preflightPathC,
      kAuthorizationFlagDefaults,
      arguments,
      NULL,  // pipe
      &exit_status);
  if (status != errAuthorizationSuccess) {
    LOG(ERROR) << "AuthorizationExecuteWithPrivileges preflight: " << status;
    [self updateStatus:kAutoupdatePromoteFailed version:nil];
    return;
  }
  if (exit_status != 0) {
    LOG(ERROR) << "keystone_promote_preflight status " << exit_status;
    [self updateStatus:kAutoupdatePromoteFailed version:nil];
    return;
  }

  // Hang on to the AuthorizationRef so that it can be used once promotion is
  // complete.  Do this before asking Keystone to promote the ticket, because
  // -promotionComplete: may be called from inside the Keystone promotion
  // call.
  authorization_.swap(authorization);

  NSDictionary* parameters = [self keystoneParameters];

  // If the brand file is user level, update parameters to point to the new
  // system level file during promotion.
  if (brandFileType_ == kBrandFileTypeUser) {
    NSMutableDictionary* temp_parameters =
        [[parameters mutableCopy] autorelease];
    [temp_parameters setObject:SystemBrandFilePath()
                        forKey:ksr::KSRegistrationBrandPathKey];
    parameters = temp_parameters;
  }

  if (![registration_ promoteWithParameters:parameters
                              authorization:authorization_]) {
    [self updateStatus:kAutoupdatePromoteFailed version:nil];
    authorization_.reset();
    return;
  }

  // Upon completion, ksr::KSRegistrationPromotionDidCompleteNotification will
  // be posted, and -promotionComplete: will be called.
}

- (void)promotionComplete:(NSNotification*)notification {
  NSDictionary* userInfo = [notification userInfo];
  if ([[userInfo objectForKey:ksr::KSRegistrationStatusKey] boolValue]) {
    if (synchronousPromotion_) {
      // Short-circuit: if performing a synchronous promotion, the promotion
      // came from the installer, which already set the permissions properly.
      // Rather than run a duplicate permission-changing operation, jump
      // straight to "done."
      [self changePermissionsForPromotionComplete];
    } else {
      [self changePermissionsForPromotionAsync];
    }
  } else {
    authorization_.reset();
    [self updateStatus:kAutoupdatePromoteFailed version:nil];
  }
}

- (void)changePermissionsForPromotionAsync {
  // NSBundle is not documented as being thread-safe.  Do NSBundle operations
  // on the main thread before jumping over to a WorkerPool-managed
  // thread to run the tool.
  DCHECK([NSThread isMainThread]);

  SEL selector = @selector(changePermissionsForPromotionWithTool:);
  NSString* toolPath =
      [base::mac::MainAppBundle() pathForResource:@"keystone_promote_postflight"
                                           ofType:@"sh"];

  PerformBridge::PostPerform(self, selector, toolPath);
}

- (void)changePermissionsForPromotionWithTool:(NSString*)toolPath {
  const char* toolPathC = [toolPath fileSystemRepresentation];

  const char* appPathC = [appPath_ fileSystemRepresentation];
  const char* arguments[] = {appPathC, NULL};

  int exit_status;
  OSStatus status = authorization_util::ExecuteWithPrivilegesAndWait(
      authorization_,
      toolPathC,
      kAuthorizationFlagDefaults,
      arguments,
      NULL,  // pipe
      &exit_status);
  if (status != errAuthorizationSuccess) {
    LOG(ERROR) << "AuthorizationExecuteWithPrivileges postflight: " << status;
  } else if (exit_status != 0) {
    LOG(ERROR) << "keystone_promote_postflight status " << exit_status;
  }

  SEL selector = @selector(changePermissionsForPromotionComplete);
  [self performSelectorOnMainThread:selector
                         withObject:nil
                      waitUntilDone:NO];
}

- (void)changePermissionsForPromotionComplete {
  authorization_.reset();

  [self updateStatus:kAutoupdatePromoted version:nil];
}

- (void)setAppPath:(NSString*)appPath {
  if (appPath != appPath_) {
    [appPath_ release];
    appPath_ = [appPath copy];
  }
}

@end  // @implementation KeystoneGlue

namespace keystone_glue {

std::string BrandCode() {
  KeystoneGlue* keystoneGlue = [KeystoneGlue defaultKeystoneGlue];
  NSString* brand_path = [keystoneGlue brandFilePath];

  if (![brand_path length])
    return std::string();

  std::string brand_code;
  file_util::ReadFileToString(FilePath([brand_path fileSystemRepresentation]),
                              &brand_code);

  return brand_code;
}

bool KeystoneEnabled() {
  return [KeystoneGlue defaultKeystoneGlue] != nil;
}

string16 CurrentlyInstalledVersion() {
  KeystoneGlue* keystoneGlue = [KeystoneGlue defaultKeystoneGlue];
  NSString* version = [keystoneGlue currentlyInstalledVersion];
  return base::SysNSStringToUTF16(version);
}

}  // namespace keystone_glue
