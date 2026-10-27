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
#include "playlist/playlistsequence.h"
#include "itunesbutton.h"

class QEvent;
class QSlider;
class QSplitter;
class QWidget;
class QMainWindow;
class Ui_MainWindow;
class Player;
class PlaylistManager;
class StyleSheetLoader;
class QBoxLayout;
class QLabel;
class Song;
class ITunesLcd;
class ITunesSourceList;

// Rearranges the main window into a 2004 era jukebox layout: a brushed metal window, transport controls and a backlit status display at the top, a source list on the left and a status bar at the bottom.
// The source list picks what is shown in the middle, either one of the sidebar views or the songs of a playlist.
// Lyrics and song information can be shown on the right, and the bottom bar holds the buttons for everything around the song list.
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

  // Whether the artwork of the song playing is shown below the source list.
  bool artwork_visible() const { return artwork_visible_; }

 Q_SIGNALS:
  void OpenOutputSettings();

 protected:
  bool eventFilter(QObject *obj, QEvent *e) override;

 private:
  QWidget *CreateTopBar();
  void RearrangeStatusBar();
  void RearrangeSidebar();
  void CreateContextPane();
  void RestoreSettings();
  ITunesButton *AddBottomButton(const ITunesButton::Glyph glyph, const QString &tooltip, QBoxLayout *layout);

  static QPixmap BrushedMetalTexture();
  static QPixmap SpeakerPixmap(const bool loud);

 private Q_SLOTS:
  void TabModeChanged(const FancyTabWidget::Mode mode);
  void ShowSource(const bool playlist);
  void BrowseClicked(const bool checked);
  void SetContextVisible(const bool visible);
  void SetArtworkVisible(const bool visible);
  void ShuffleClicked();
  void RepeatClicked();
  void ShuffleModeChanged(const PlaylistSequence::ShuffleMode mode);
  void RepeatModeChanged(const PlaylistSequence::RepeatMode mode);
  void SongChanged(const Song &song);

 private:
  QMainWindow *mainwindow_;
  Ui_MainWindow *ui_;
  SharedPtr<Player> player_;
  SharedPtr<PlaylistManager> playlist_manager_;
  StyleSheetLoader *css_loader_;

  QWidget *central_widget_;
  QWidget *volume_;
  QWidget *search_column_;
  FancyTabWidget *tabs_;
  QSplitter *splitter_;
  ITunesSourceList *source_list_;
  QWidget *context_pane_;
  QSlider *volume_slider_;
  ITunesButton *play_button_;
  ITunesButton *browse_button_;
  ITunesButton *shuffle_button_;
  ITunesButton *repeat_button_;
  ITunesButton *artwork_button_;
  ITunesButton *context_button_;
  ITunesLcd *lcd_;
  QLabel *format_label_;
  QLabel *hires_label_;

  QPixmap metal_;
  FancyTabWidget::Mode original_tab_mode_;
  bool artwork_visible_;
};

#endif  // ITUNESTHEME_H
