/*
 * Strawberry Music Player
 * Copyright 2026, Barry Brown
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef ITUNESTHEME_H
#define ITUNESTHEME_H

#include <QObject>
#include <QPixmap>

#include "includes/shared_ptr.h"
#include "widgets/fancytabwidget.h"

class QEvent;
class QSlider;
class QSplitter;
class QWidget;
class QMainWindow;
class Ui_MainWindow;
class Player;
class PlaylistManager;
class StyleSheetLoader;
class ITunesButton;
class ITunesLcd;

// Rearranges the main window into a 2004 era jukebox layout: a brushed metal window, transport controls and a backlit status display at the top, a source list on the left and a status bar at the bottom.
// The widgets created by mainwindow.ui are moved around at runtime rather than changing the .ui file, so the stock layout is untouched when the theme is disabled.
class ITunesTheme : public QObject {
  Q_OBJECT

 public:
  explicit ITunesTheme(QMainWindow *mainwindow, Ui_MainWindow *ui, SharedPtr<Player> player, SharedPtr<PlaylistManager> playlist_manager, StyleSheetLoader *css_loader);

  static bool IsEnabled();

  // Call after the main window has restored its own settings.
  void Apply();

  // Call after the main window has saved its own settings.
  void SaveSettings();

 protected:
  bool eventFilter(QObject *obj, QEvent *e) override;

 private:
  QWidget *CreateTopBar();
  void RearrangeStatusBar();
  void RearrangeSidebar();
  void SetBrowseVisible(const bool visible);

  static QPixmap BrushedMetalTexture();
  static QPixmap SpeakerPixmap(const bool loud);

 private Q_SLOTS:
  void TabModeChanged(const FancyTabWidget::Mode mode);

 private:
  QMainWindow *mainwindow_;
  Ui_MainWindow *ui_;
  SharedPtr<Player> player_;
  SharedPtr<PlaylistManager> playlist_manager_;
  StyleSheetLoader *css_loader_;

  QWidget *central_widget_;
  QWidget *volume_;
  FancyTabWidget *tabs_;
  QSplitter *splitter_;
  QSplitter *source_splitter_;
  QSlider *volume_slider_;
  ITunesButton *play_button_;
  ITunesButton *browse_button_;
  ITunesLcd *lcd_;

  QPixmap metal_;
  FancyTabWidget::Mode original_tab_mode_;
  int browse_width_;
};

#endif  // ITUNESTHEME_H
