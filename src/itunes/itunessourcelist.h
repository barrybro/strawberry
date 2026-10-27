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

#ifndef ITUNESSOURCELIST_H
#define ITUNESSOURCELIST_H

#include <QTreeWidget>
#include <QString>
#include <QStringList>

#include "includes/shared_ptr.h"

class QContextMenuEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QDragLeaveEvent;
class QDropEvent;
class QKeyEvent;
class QPaintEvent;
class FancyTabWidget;
class Playlist;
class PlaylistManager;

// The "Source" list on the left, the only way to move around in the iTunes-style layout.
// It lists the sidebar views (Library, Files, Radio, streaming services, devices...) and all playlists, in sections.
// Selecting a view shows it in place of the song list, selecting a playlist shows its songs.
// Playlists are created, renamed and deleted here, and songs can be dropped on them.
class ITunesSourceList : public QTreeWidget {
  Q_OBJECT

 public:
  explicit ITunesSourceList(FancyTabWidget *tabs, SharedPtr<PlaylistManager> playlist_manager, QWidget *parent = nullptr);

  // The selected source, the name of a sidebar view or "playlist:<id>", used to restore the selection on the next start.
  QString CurrentSource() const;
  void SetCurrentSource(const QString &source);

  bool IsPlaylistSelected() const;
  void SelectLibrary();
  void SelectCurrentPlaylist();

  QStringList hidden_views() const { return hidden_views_; }
  void SetHiddenViews(const QStringList &hidden_views);

  // Views which are shown elsewhere in the layout, they are not listed and can't be added back.
  void SetExcludedViews(const QStringList &excluded_views);

 public Q_SLOTS:
  void NewPlaylist();

 Q_SIGNALS:
  // Emitted when the selection moves between a sidebar view and a playlist.
  void SourceChanged(const bool playlist);

 protected:
  void contextMenuEvent(QContextMenuEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
  void dragEnterEvent(QDragEnterEvent *e) override;
  void dragMoveEvent(QDragMoveEvent *e) override;
  void dragLeaveEvent(QDragLeaveEvent *e) override;
  void dropEvent(QDropEvent *e) override;
  void paintEvent(QPaintEvent *e) override;

 private:
  enum class Kind {
    Header,
    View,
    Playlist
  };

  enum class Section {
    Library,
    Streaming,
    Devices,
    Playlists
  };

  static Section SectionForView(const QString &name);
  void AddHeader(const QString &text);

  static QString PlaylistSource(const int id);
  QTreeWidgetItem *ItemForSource(const QString &source) const;
  static int PlaylistId(const QTreeWidgetItem *item);
  void RenamePlaylist(QTreeWidgetItem *item);
  void DeletePlaylist(QTreeWidgetItem *item);
  // Highlights the playlist songs will be dropped on, or the whole list when they will go to a new playlist.
  void SetDropTarget(QTreeWidgetItem *item, const bool new_playlist);

 private Q_SLOTS:
  void Rebuild();
  void ItemSelected(QTreeWidgetItem *current);
  void ItemChanged(QTreeWidgetItem *item);
  void TabChanged(const int idx);
  void CurrentPlaylistChanged(Playlist *playlist);

 private:
  FancyTabWidget *tabs_;
  SharedPtr<PlaylistManager> playlist_manager_;
  QStringList hidden_views_;
  QStringList excluded_views_;
  QString current_source_;
  QTreeWidgetItem *drop_item_;
  bool drop_new_playlist_;
  bool rebuilding_;
};

#endif  // ITUNESSOURCELIST_H
