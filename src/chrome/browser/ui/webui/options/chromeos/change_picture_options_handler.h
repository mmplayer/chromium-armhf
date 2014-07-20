// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CHANGE_PICTURE_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CHANGE_PICTURE_OPTIONS_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/profile_image_downloader.h"
#include "chrome/browser/chromeos/options/take_photo_dialog.h"
#include "chrome/browser/ui/shell_dialogs.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "content/common/notification_registrar.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace chromeos {

// ChromeOS user image options page UI handler.
class ChangePictureOptionsHandler : public OptionsPageUIHandler,
                                    public SelectFileDialog::Listener,
                                    public TakePhotoDialog::Delegate,
                                    public ProfileImageDownloader::Delegate {
 public:
  ChangePictureOptionsHandler();
  virtual ~ChangePictureOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void Initialize() OVERRIDE;
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Sends list of available default images to the page.
  void SendAvailableImages();

  // Sends current selection to the page.
  void SendSelectedImage();

  // Starts download of user profile image.
  void DownloadProfileImage();

  // Starts camera presence check.
  void CheckCameraPresence();

  // Updates UI with camera presence state.
  void SetCameraPresent(bool present);

  // Opens a file selection dialog to choose user image from file.
  void HandleChooseFile(const base::ListValue* args);

  // Opens the camera capture dialog.
  void HandleTakePhoto(const base::ListValue* args);

  // Gets the list of available user images and sends it to the page.
  void HandleGetAvailableImages(const base::ListValue* args);

  // Gets URL of the currently selected image.
  void HandleGetSelectedImage(const base::ListValue* args);

  // Selects one of the available images as user's.
  void HandleSelectImage(const base::ListValue* args);

  // SelectFileDialog::Delegate implementation.
  virtual void FileSelected(
      const FilePath& path,
      int index, void* params) OVERRIDE;

  // TakePhotoDialog::Delegate implementation.
  virtual void OnPhotoAccepted(const SkBitmap& photo) OVERRIDE;

  // ProfileImageDownloader::Delegate implementation.
  virtual void OnDownloadSuccess(const SkBitmap& image) OVERRIDE;
  virtual void OnDownloadFailure() OVERRIDE;
  virtual void OnDownloadDefaultImage() OVERRIDE;

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Called when the camera presence check has been completed.
  void OnCameraPresenceCheckDone();

  // Returns handle to browser window or NULL if it can't be found.
  gfx::NativeWindow GetBrowserWindow() const;

  // Records previous image index and content so that user can switch
  // back to it.
  void RecordPreviousImage(int image_index, const SkBitmap& image);

  scoped_refptr<SelectFileDialog> select_file_dialog_;

  SkBitmap previous_image_;
  int previous_image_index_;
  std::string previous_image_data_url_;

  SkBitmap profile_image_;
  std::string profile_image_data_url_;

  // Downloads user profile image when it's not set as current image.
  scoped_ptr<ProfileImageDownloader> profile_image_downloader_;

  NotificationRegistrar registrar_;

  base::WeakPtrFactory<ChangePictureOptionsHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChangePictureOptionsHandler);
};

} // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CHANGE_PICTURE_OPTIONS_HANDLER_H_
