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

#include <algorithm>

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QTabBar>
#include <QVariant>
#include <QList>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPen>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QPaintEvent>
#include <QFont>
#include <QFontMetrics>
#include <QColor>
#include <QSignalBlocker>
#include <QPair>

#include "core/iconloader.h"
#include "core/mimedata.h"
#include "core/settings.h"
#include "constants/playlistsettings.h"
#include "playlist/playlist.h"
#include "playlist/playlistbackend.h"
#include "playlist/playlistmanager.h"
#include "widgets/fancytabwidget.h"
#include "widgets/fancytabdata.h"
#include "itunessourcelist.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kRoleKind = Qt::UserRole;
constexpr int kRoleSource = Qt::UserRole + 1;
constexpr int kRolePage = Qt::UserRole + 2;
constexpr int kRoleName = Qt::UserRole + 3;
constexpr char kPlaylistSourcePrefix[] = "playlist:";

bool CanDrop(const QMimeData *data) {
  return data->hasUrls() || data->hasFormat(QString::fromLatin1(Playlist::kRowsMimetype)) || qobject_cast<const MimeData*>(data);
}

}  // namespace

ITunesSourceList::ITunesSourceList(FancyTabWidget *tabs, SharedPtr<PlaylistManager> playlist_manager, QWidget *parent)
    : QTreeWidget(parent),
      tabs_(tabs),
      playlist_manager_(playlist_manager),
      hidden_views_({u"playlists"_s}),
      drop_item_(nullptr),
      drop_new_playlist_(false),
      rebuilding_(false) {

  setObjectName(u"itunes_source_list"_s);
  setColumnCount(1);
  setHeaderHidden(true);
  setRootIsDecorated(false);
  setIndentation(0);
  setIconSize(QSize(16, 16));
  setFrameShape(QFrame::NoFrame);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
  setAcceptDrops(true);
  viewport()->setAcceptDrops(true);

  QObject::connect(this, &QTreeWidget::currentItemChanged, this, &ITunesSourceList::ItemSelected);
  QObject::connect(this, &QTreeWidget::itemChanged, this, &ITunesSourceList::ItemChanged);
  QObject::connect(tabs_, &FancyTabWidget::CurrentTabChanged, this, &ITunesSourceList::TabChanged);
  QObject::connect(&*playlist_manager_, &PlaylistManager::CurrentChanged, this, &ITunesSourceList::CurrentPlaylistChanged);

  // Rebuild once the change is complete: tab data is set after the tab is inserted, and the playlist manager is still in the middle of adding or closing the playlist.
  QObject::connect(tabs_, &FancyTabWidget::TabsChanged, this, &ITunesSourceList::Rebuild, Qt::QueuedConnection);
  QObject::connect(&*playlist_manager_, &PlaylistManager::PlaylistAdded, this, &ITunesSourceList::Rebuild, Qt::QueuedConnection);
  QObject::connect(&*playlist_manager_, &PlaylistManager::PlaylistClosed, this, &ITunesSourceList::Rebuild, Qt::QueuedConnection);
  QObject::connect(&*playlist_manager_, &PlaylistManager::PlaylistDeleted, this, &ITunesSourceList::Rebuild, Qt::QueuedConnection);
  QObject::connect(&*playlist_manager_, &PlaylistManager::PlaylistRenamed, this, &ITunesSourceList::Rebuild, Qt::QueuedConnection);
  QObject::connect(&*playlist_manager_, &PlaylistManager::PlaylistFavorited, this, &ITunesSourceList::Rebuild, Qt::QueuedConnection);

  Rebuild();

}

QString ITunesSourceList::PlaylistSource(const int id) {
  return QLatin1String(kPlaylistSourcePrefix) + QString::number(id);
}

int ITunesSourceList::PlaylistId(const QTreeWidgetItem *item) {

  if (!item || static_cast<Kind>(item->data(0, kRoleKind).toInt()) != Kind::Playlist) return -1;
  return item->data(0, kRoleSource).toString().mid(static_cast<qsizetype>(qstrlen(kPlaylistSourcePrefix))).toInt();

}

QTreeWidgetItem *ITunesSourceList::ItemForSource(const QString &source) const {

  if (source.isEmpty()) return nullptr;

  for (int i = 0; i < topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = topLevelItem(i);
    if (item->data(0, kRoleSource).toString() == source) return item;
  }

  return nullptr;

}

QString ITunesSourceList::CurrentSource() const { return current_source_; }

void ITunesSourceList::SetCurrentSource(const QString &source) {

  QTreeWidgetItem *item = ItemForSource(source);
  if (!item) item = ItemForSource(PlaylistSource(playlist_manager_->current_id()));
  for (int i = 0; !item && i < topLevelItemCount(); ++i) {
    if (topLevelItem(i)->flags() & Qt::ItemIsSelectable) item = topLevelItem(i);
  }
  if (!item) return;

  if (currentItem() == item) {
    ItemSelected(item);
  }
  else {
    setCurrentItem(item);
  }

}

bool ITunesSourceList::IsPlaylistSelected() const {
  return current_source_.startsWith(QLatin1String(kPlaylistSourcePrefix));
}

void ITunesSourceList::SelectLibrary() {
  SetCurrentSource(u"collection"_s);
}

void ITunesSourceList::SelectCurrentPlaylist() {
  SetCurrentSource(PlaylistSource(playlist_manager_->current_id()));
}

void ITunesSourceList::SetHiddenViews(const QStringList &hidden_views) {

  hidden_views_ = hidden_views;
  Rebuild();

}

void ITunesSourceList::SetExcludedViews(const QStringList &excluded_views) {

  excluded_views_ = excluded_views;
  Rebuild();

}

ITunesSourceList::Section ITunesSourceList::SectionForView(const QString &name) {

  if (name == "collection"_L1 || name == "files"_L1 || name == "radios"_L1) return Section::Library;
  if (name == "devices"_L1) return Section::Devices;
  if (name == "queue"_L1 || name == "smartplaylists"_L1 || name == "playlists"_L1) return Section::Playlists;
  // Everything else is a streaming service.
  return Section::Streaming;

}

void ITunesSourceList::AddHeader(const QString &text) {

  QTreeWidgetItem *item = new QTreeWidgetItem(this, QStringList() << text);
  QFont font = item->font(0);
  font.setBold(true);
  font.setPointSizeF(font.pointSizeF() * 0.85);
  item->setFont(0, font);
  item->setForeground(0, QColor(128, 128, 128));
  item->setData(0, kRoleKind, static_cast<int>(Kind::Header));
  // Not enabled, so the keyboard moves past it.
  item->setFlags(Qt::NoItemFlags);
  item->setSizeHint(0, QSize(0, QFontMetrics(font).height() + 14));
  item->setTextAlignment(0, Qt::AlignLeft | Qt::AlignBottom);

}

void ITunesSourceList::Rebuild() {

  rebuilding_ = true;
  drop_item_ = nullptr;
  clear();

  // The sidebar views by section, under the names a 2004 jukebox would have used where they fit.
  QList<QList<FancyTabData*>> sections(4);
  for (int i = 0; i < tabs_->count(); ++i) {
    FancyTabData *tab_data = tabs_->tabBar()->tabData(i).value<FancyTabData*>();
    if (!tab_data || hidden_views_.contains(tab_data->name()) || excluded_views_.contains(tab_data->name())) continue;
    sections[static_cast<int>(SectionForView(tab_data->name()))] << tab_data;
  }

  const auto add_views = [this](const QList<FancyTabData*> &views) {
    for (FancyTabData *tab_data : views) {
      QString label = tab_data->label();
      if (tab_data->name() == "collection"_L1) label = tr("Music");
      else if (tab_data->name() == "radios"_L1) label = tr("Radio");
      else if (tab_data->name() == "smartplaylists"_L1) label = tr("Smart Playlists");
      else if (tab_data->name() == "playlists"_L1) label = tr("Saved Playlists");
      else if (tab_data->name() == "queue"_L1) label = tr("Up Next");
      QTreeWidgetItem *item = new QTreeWidgetItem(this, QStringList() << label);
      item->setIcon(0, tab_data->icon());
      item->setData(0, kRoleKind, static_cast<int>(Kind::View));
      item->setData(0, kRoleSource, tab_data->name());
      item->setData(0, kRolePage, QVariant::fromValue<QWidget*>(tab_data->page()));
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    }
  };

  const QList<FancyTabData*> &library_views = sections[static_cast<int>(Section::Library)];
  const QList<FancyTabData*> &streaming_views = sections[static_cast<int>(Section::Streaming)];
  const QList<FancyTabData*> &device_views = sections[static_cast<int>(Section::Devices)];
  if (!library_views.isEmpty()) {
    AddHeader(tr("Library"));
    add_views(library_views);
  }
  if (!streaming_views.isEmpty()) {
    AddHeader(tr("Streaming"));
    add_views(streaming_views);
  }
  if (!device_views.isEmpty()) {
    AddHeader(tr("Devices"));
    add_views(device_views);
  }
  AddHeader(tr("Playlists"));
  add_views(sections[static_cast<int>(Section::Playlists)]);

  // All open playlists, and the favorite playlists which are closed, in alphabetical order.
  QList<QPair<int, QString>> playlists;
  const QList<int> playlist_ids = playlist_manager_->playlist_ids();
  for (const int id : playlist_ids) {
    playlists << qMakePair(id, playlist_manager_->playlist_name(id));
  }
  const PlaylistBackend::PlaylistList favorite_playlists = playlist_manager_->playlist_backend()->GetAllFavoritePlaylists();
  for (const PlaylistBackend::Playlist &playlist : favorite_playlists) {
    if (!playlist_manager_->IsPlaylistOpen(playlist.id)) {
      playlists << qMakePair(playlist.id, playlist.name);
    }
  }
  std::stable_sort(playlists.begin(), playlists.end(), [](const QPair<int, QString> &a, const QPair<int, QString> &b) { return a.second.localeAwareCompare(b.second) < 0; });

  const QIcon playlist_icon = IconLoader::Load(u"view-media-playlist"_s);
  for (const QPair<int, QString> &playlist : std::as_const(playlists)) {
    QTreeWidgetItem *item = new QTreeWidgetItem(this, QStringList() << playlist.second);
    item->setIcon(0, playlist_icon);
    item->setData(0, kRoleKind, static_cast<int>(Kind::Playlist));
    item->setData(0, kRoleSource, PlaylistSource(playlist.first));
    item->setData(0, kRoleName, playlist.second);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
  }

  rebuilding_ = false;

  // Keep the selection, or fall back to the current playlist if the selected source is gone.
  if (!current_source_.isEmpty()) {
    SetCurrentSource(current_source_);
  }

}

void ITunesSourceList::ItemSelected(QTreeWidgetItem *current) {

  if (rebuilding_ || !current) return;

  // Opening a playlist rebuilds the list, so take everything needed from the item first.
  const Kind kind = static_cast<Kind>(current->data(0, kRoleKind).toInt());
  if (kind == Kind::Header) return;
  current_source_ = current->data(0, kRoleSource).toString();

  if (kind == Kind::View) {
    const int idx = tabs_->indexOf(current->data(0, kRolePage).value<QWidget*>());
    if (idx >= 0 && idx != tabs_->currentIndex()) {
      tabs_->setCurrentIndex(idx);
    }
  }
  else {
    const int id = PlaylistId(current);
    if (!playlist_manager_->IsPlaylistOpen(id) || playlist_manager_->current_id() != id) {
      playlist_manager_->SetCurrentOrOpen(id);
    }
  }

  Q_EMIT SourceChanged(kind == Kind::Playlist);

}

void ITunesSourceList::TabChanged(const int idx) {

  // Follow the views being shown from elsewhere, for example "Show in collection" in the song list.
  if (rebuilding_) return;
  FancyTabData *tab_data = tabs_->tabBar()->tabData(idx).value<FancyTabData*>();
  if (!tab_data || tab_data->name() == current_source_) return;
  if (QTreeWidgetItem *item = ItemForSource(tab_data->name())) {
    setCurrentItem(item);
  }

}

void ITunesSourceList::CurrentPlaylistChanged(Playlist *playlist) {

  // Show playlists made current from elsewhere, for example a new playlist created from the menu.
  if (rebuilding_ || !playlist) return;
  const QString source = PlaylistSource(playlist->id());
  if (source == current_source_) return;

  if (QTreeWidgetItem *item = ItemForSource(source)) {
    setCurrentItem(item);
  }
  else {
    // A new playlist, its item is added by the next rebuild.
    current_source_ = source;
    Q_EMIT SourceChanged(true);
  }

}

void ITunesSourceList::NewPlaylist() {

  // Untitled playlists are numbered to tell them apart.
  QStringList names;
  for (int i = 0; i < topLevelItemCount(); ++i) {
    names << topLevelItem(i)->data(0, kRoleName).toString();
  }
  QString name = tr("untitled playlist");
  for (int i = 2; names.contains(name); ++i) {
    name = tr("untitled playlist %1").arg(i);
  }

  playlist_manager_->New(name);
  const QString source = PlaylistSource(playlist_manager_->current_id());

  // Let the new playlist be named right away, once it has been added by the queued rebuild.
  QMetaObject::invokeMethod(this, [this, source]() {
    if (QTreeWidgetItem *item = ItemForSource(source)) {
      setCurrentItem(item);
      scrollToItem(item);
      editItem(item);
    }
  }, Qt::QueuedConnection);

}

void ITunesSourceList::ItemChanged(QTreeWidgetItem *item) {

  if (rebuilding_) return;

  const int id = PlaylistId(item);
  if (id == -1) return;

  const QString old_name = item->data(0, kRoleName).toString();
  const QString new_name = item->text(0).trimmed();
  if (new_name.isEmpty() || new_name == old_name) {
    const QSignalBlocker blocker(this);
    item->setText(0, old_name);
    return;
  }

  if (!playlist_manager_->IsPlaylistOpen(id)) playlist_manager_->Open(id);
  playlist_manager_->Rename(id, new_name);

}

void ITunesSourceList::RenamePlaylist(QTreeWidgetItem *item) {

  setCurrentItem(item);
  editItem(item);

}

void ITunesSourceList::DeletePlaylist(QTreeWidgetItem *item) {

  const int id = PlaylistId(item);
  if (id == -1) return;

  const bool open = playlist_manager_->IsPlaylistOpen(id);
  // There is always at least one playlist to play from.
  if (open && playlist_manager_->playlist_ids().count() <= 1) return;

  Settings s;
  s.beginGroup(PlaylistSettings::kSettingsGroup);
  const bool ask_for_delete = s.value(PlaylistSettings::kWarnClosePlaylist, PlaylistSettings::kDefaultWarnClosePlaylist).toBool();
  s.endGroup();

  if (ask_for_delete && (!open || !playlist_manager_->playlist(id)->GetAllSongs().isEmpty())) {
    const QString name = item->data(0, kRoleName).toString();
    if (QMessageBox::question(this, tr("Delete playlist"), tr("Are you sure you want to delete the playlist \"%1\"?").arg(name), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
      return;
    }
  }

  if (open) {
    playlist_manager_->Delete(id);
  }
  else {
    // Unfavoriting a playlist which is not open deletes it.
    playlist_manager_->Favorite(id, false);
  }

}

void ITunesSourceList::contextMenuEvent(QContextMenuEvent *e) {

  QTreeWidgetItem *item = itemAt(viewport()->mapFrom(this, e->pos()));
  const int id = PlaylistId(item);

  QMenu menu(this);
  menu.addAction(IconLoader::Load(u"document-new"_s), tr("New Playlist"), this, &ITunesSourceList::NewPlaylist);

  if (id != -1) {
    menu.addSeparator();
    menu.addAction(IconLoader::Load(u"edit-rename"_s), tr("Rename"), this, [this, item]() { RenamePlaylist(item); });
    QAction *action_save = menu.addAction(IconLoader::Load(u"document-save"_s), tr("Save Playlist..."), this, [this, id]() {
      if (!playlist_manager_->IsPlaylistOpen(id)) playlist_manager_->Open(id);
      playlist_manager_->SaveWithUI(id, playlist_manager_->playlist_name(id));
    });
    action_save->setEnabled(playlist_manager_->IsPlaylistOpen(id));
    QAction *action_delete = menu.addAction(IconLoader::Load(u"edit-delete"_s), tr("Delete"), this, [this, item]() { DeletePlaylist(item); });
    action_delete->setEnabled(!playlist_manager_->IsPlaylistOpen(id) || playlist_manager_->playlist_ids().count() > 1);
  }

  // Views which are rarely needed are hidden by default, they can be brought back here.
  menu.addSeparator();
  QMenu *views_menu = menu.addMenu(tr("Show in Source List"));
  for (int i = 0; i < tabs_->count(); ++i) {
    FancyTabData *tab_data = tabs_->tabBar()->tabData(i).value<FancyTabData*>();
    if (!tab_data || excluded_views_.contains(tab_data->name())) continue;
    const QString name = tab_data->name();
    QAction *action = views_menu->addAction(tab_data->icon(), tab_data->label());
    action->setCheckable(true);
    action->setChecked(!hidden_views_.contains(name));
    QObject::connect(action, &QAction::toggled, this, [this, name](const bool checked) {
      QStringList hidden_views = hidden_views_;
      if (checked) hidden_views.removeAll(name);
      else hidden_views << name;
      SetHiddenViews(hidden_views);
    });
  }

  menu.exec(e->globalPos());

}

void ITunesSourceList::keyPressEvent(QKeyEvent *e) {

  if ((e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) && state() != QAbstractItemView::EditingState && PlaylistId(currentItem()) != -1) {
    DeletePlaylist(currentItem());
    return;
  }

  QTreeWidget::keyPressEvent(e);

}

void ITunesSourceList::SetDropTarget(QTreeWidgetItem *item, const bool new_playlist) {

  if (item == drop_item_ && new_playlist == drop_new_playlist_) return;
  drop_item_ = item;
  drop_new_playlist_ = new_playlist;
  viewport()->update();

}

void ITunesSourceList::dragEnterEvent(QDragEnterEvent *e) {

  if (CanDrop(e->mimeData())) {
    e->acceptProposedAction();
  }
  else {
    e->ignore();
  }

}

void ITunesSourceList::dragMoveEvent(QDragMoveEvent *e) {

  QTreeWidgetItem *item = itemAt(e->position().toPoint());

  // Songs go to the playlist under the mouse, or to a new playlist when dropped below the list.
  if (item && PlaylistId(item) == -1) {
    SetDropTarget(nullptr, false);
    e->ignore();
    return;
  }

  // Moving songs to the end of the playlist they are dragged from is done in the song list.
  if (item && e->mimeData()->hasFormat(QString::fromLatin1(Playlist::kRowsMimetype)) && PlaylistId(item) == playlist_manager_->current_id()) {
    SetDropTarget(nullptr, false);
    e->ignore();
    return;
  }

  SetDropTarget(item, item == nullptr);
  e->setDropAction(Qt::CopyAction);
  e->accept();

}

void ITunesSourceList::dragLeaveEvent(QDragLeaveEvent *e) {

  Q_UNUSED(e)
  SetDropTarget(nullptr, false);

}

void ITunesSourceList::dropEvent(QDropEvent *e) {

  const int id = PlaylistId(drop_item_);
  const bool new_playlist = drop_new_playlist_;
  SetDropTarget(nullptr, false);

  Playlist *playlist = nullptr;
  if (id != -1) {
    if (!playlist_manager_->IsPlaylistOpen(id)) playlist_manager_->Open(id);
    playlist = playlist_manager_->playlist(id);
  }
  else if (new_playlist) {
    const MimeData *mime_data = qobject_cast<const MimeData*>(e->mimeData());
    playlist_manager_->New(mime_data && !mime_data->name_for_new_playlist_.isEmpty() ? mime_data->name_for_new_playlist_ : tr("untitled playlist"));
    playlist = playlist_manager_->current();
  }

  if (!playlist) {
    e->ignore();
    return;
  }

  e->setDropAction(Qt::CopyAction);
  e->accept();
  playlist->dropMimeData(e->mimeData(), Qt::CopyAction, -1, 0, QModelIndex());

}

void ITunesSourceList::paintEvent(QPaintEvent *e) {

  QTreeWidget::paintEvent(e);

  if (!drop_item_ && !drop_new_playlist_) return;

  QPainter p(viewport());
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(QColor(56, 117, 215), 2));
  p.setBrush(Qt::NoBrush);
  const QRect rect = drop_item_ ? visualItemRect(drop_item_) : viewport()->rect();
  p.drawRoundedRect(QRectF(rect).adjusted(1.5, 1.5, -1.5, -1.5), 4, 4);

}
