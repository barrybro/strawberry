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
#include <vector>

#include <QObject>
#include <QMainWindow>
#include <QWidget>
#include <QLabel>
#include <QSlider>
#include <QSplitter>
#include <QAction>
#include <QMenu>
#include <QStringList>
#include <QTabBar>
#include <QBoxLayout>
#include <QLayoutItem>
#include <QSignalBlocker>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRandomGenerator>
#include <QEvent>

#include "ui_mainwindow.h"
#include "constants/appearancesettings.h"
#include "constants/mainwindowsettings.h"
#include "core/settings.h"
#include "core/player.h"
#include "core/stylesheetloader.h"
#include "core/song.h"
#include "playlist/playlistmanager.h"
#include "playlist/playlistsequence.h"
#include "widgets/fancytabdata.h"
#include "widgets/playingwidget.h"
#include "widgets/searchfield.h"
#include "itunesbutton.h"
#include "ituneslcd.h"
#include "itunessourcelist.h"
#include "itunestheme.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kSettingsGroup[] = "ITunesLayout";
constexpr char kSidebarWidth[] = "sidebar_width";
constexpr char kSource[] = "source";
constexpr char kHiddenSources[] = "hidden_sources";
constexpr char kContextVisible[] = "context_visible";
constexpr char kContextWidth[] = "context_width";
constexpr char kArtworkVisible[] = "artwork_visible";
constexpr int kDefaultSidebarWidth = 190;
constexpr int kDefaultContextWidth = 300;
constexpr int kBottomButtonHeight = 22;

// Adds a widget to a layout, keeping it hidden if it was explicitly hidden before.
void MoveWidget(QWidget *widget, QBoxLayout *layout, const int stretch = 0) {
  const bool hidden = widget->isHidden();
  layout->addWidget(widget, stretch);
  widget->setVisible(!hidden);
}

QWidget *LabeledColumn(QWidget *widget, const QString &text, QWidget *parent) {

  QWidget *column = new QWidget(parent);
  QVBoxLayout *layout = new QVBoxLayout(column);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(2);
  layout->addStretch(1);
  MoveWidget(widget, layout);
  layout->setAlignment(widget, Qt::AlignHCenter);
  QLabel *label = new QLabel(text, column);
  label->setObjectName(u"itunes_caption"_s);
  label->setAlignment(Qt::AlignCenter);
  layout->addWidget(label);
  return column;

}

}  // namespace

ITunesTheme::ITunesTheme(QMainWindow *mainwindow, Ui_MainWindow *ui, SharedPtr<Player> player, SharedPtr<PlaylistManager> playlist_manager, StyleSheetLoader *css_loader)
    : QObject(mainwindow),
      mainwindow_(mainwindow),
      ui_(ui),
      player_(player),
      playlist_manager_(playlist_manager),
      css_loader_(css_loader),
      central_widget_(nullptr),
      volume_(nullptr),
      search_column_(nullptr),
      tabs_(nullptr),
      splitter_(nullptr),
      source_list_(nullptr),
      context_pane_(nullptr),
      volume_slider_(nullptr),
      play_button_(nullptr),
      browse_button_(nullptr),
      shuffle_button_(nullptr),
      repeat_button_(nullptr),
      artwork_button_(nullptr),
      context_button_(nullptr),
      lcd_(nullptr),
      format_label_(nullptr),
      hires_label_(nullptr),
      original_tab_mode_(FancyTabWidget::Mode::LargeSidebar),
      artwork_visible_(true) {}

bool ITunesTheme::IsEnabled() {

  Settings s;
  s.beginGroup(AppearanceSettings::kSettingsGroup);
  const bool enabled = s.value(AppearanceSettings::kITunesLayout, AppearanceSettings::kDefaultITunesLayout).toBool();
  s.endGroup();

  return enabled;

}

void ITunesTheme::Apply() {

  central_widget_ = ui_->centralWidget;
  volume_ = ui_->volume;
  tabs_ = ui_->tabs;
  splitter_ = ui_->splitter;

  // The metal is painted behind everything in the central widget, the widgets on top are transparent unless they fill their own background.
  metal_ = BrushedMetalTexture();
  central_widget_->installEventFilter(this);
  ui_->layout_centralWidget->setContentsMargins(4, 0, 4, 0);

  css_loader_->SetStyleSheet(mainwindow_, QStringList() << u":/style/strawberry.css"_s << u":/style/itunes.css"_s);

  ui_->layout_centralWidget->insertWidget(0, CreateTopBar());
  RearrangeSidebar();
  CreateContextPane();
  RearrangeStatusBar();
  RestoreSettings();

}

QWidget *ITunesTheme::CreateTopBar() {

  QWidget *bar = new QWidget(central_widget_);
  bar->setObjectName(u"itunes_top_bar"_s);
  bar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  QHBoxLayout *layout = new QHBoxLayout(bar);
  layout->setContentsMargins(10, 6, 10, 6);
  layout->setSpacing(12);

  // Transport buttons with the volume slider below them
  QWidget *transport = new QWidget(bar);
  QVBoxLayout *transport_layout = new QVBoxLayout(transport);
  transport_layout->setContentsMargins(0, 0, 0, 0);
  transport_layout->setSpacing(4);

  QHBoxLayout *buttons_layout = new QHBoxLayout;
  buttons_layout->setSpacing(2);
  ITunesButton *previous_button = new ITunesButton(ITunesButton::Glyph::Previous, ITunesButton::Shape::Round, 32, transport);
  previous_button->SetAction(ui_->action_previous_track);
  play_button_ = new ITunesButton(ITunesButton::Glyph::Play, ITunesButton::Shape::Round, 38, transport);
  play_button_->SetAction(ui_->action_play_pause);
  ITunesButton *next_button = new ITunesButton(ITunesButton::Glyph::Next, ITunesButton::Shape::Round, 32, transport);
  next_button->SetAction(ui_->action_next_track);
  buttons_layout->addWidget(previous_button, 0, Qt::AlignVCenter);
  buttons_layout->addWidget(play_button_, 0, Qt::AlignVCenter);
  buttons_layout->addWidget(next_button, 0, Qt::AlignVCenter);
  transport_layout->addLayout(buttons_layout);

  QHBoxLayout *volume_layout = new QHBoxLayout;
  volume_layout->setSpacing(3);
  QLabel *quiet = new QLabel(transport);
  quiet->setPixmap(SpeakerPixmap(false));
  QLabel *loud = new QLabel(transport);
  loud->setPixmap(SpeakerPixmap(true));
  volume_slider_ = new QSlider(Qt::Horizontal, transport);
  volume_slider_->setObjectName(u"itunes_volume"_s);
  volume_slider_->setRange(0, 100);
  volume_slider_->setValue(ui_->volume->value());
  volume_slider_->setEnabled(volume_->isEnabled());
  volume_slider_->setFocusPolicy(Qt::NoFocus);
  volume_layout->addWidget(quiet);
  volume_layout->addWidget(volume_slider_, 1);
  volume_layout->addWidget(loud);
  transport_layout->addLayout(volume_layout);

  // The stock volume slider stays hidden in the player controls, follow it to keep the enabled state in sync.
  volume_->installEventFilter(this);
  QObject::connect(volume_slider_, &QSlider::valueChanged, &*player_, &Player::SetVolumeFromSlider);
  QObject::connect(&*player_, &Player::VolumeChanged, volume_slider_, [this](const uint volume) {
    const QSignalBlocker blocker(volume_slider_);
    volume_slider_->setValue(static_cast<int>(volume));
  });

  QObject::connect(&*player_, &Player::Playing, play_button_, [this]() { play_button_->SetGlyph(ITunesButton::Glyph::Pause); });
  QObject::connect(&*player_, &Player::Paused, play_button_, [this]() { play_button_->SetGlyph(ITunesButton::Glyph::Play); });
  QObject::connect(&*player_, &Player::Stopped, play_button_, [this]() { play_button_->SetGlyph(ITunesButton::Glyph::Play); });

  layout->addWidget(transport);
  layout->addStretch(1);

  // Status display
  lcd_ = new ITunesLcd(player_, bar);
  lcd_->setMaximumWidth(560);
  QObject::connect(&*playlist_manager_, &PlaylistManager::CurrentSongChanged, lcd_, &ITunesLcd::SongChanged);
  QObject::connect(&*player_, &Player::Playing, lcd_, &ITunesLcd::Playing);
  QObject::connect(&*player_, &Player::Paused, lcd_, &ITunesLcd::Paused);
  QObject::connect(&*player_, &Player::Stopped, lcd_, &ITunesLcd::Stopped);
  QObject::connect(lcd_, &ITunesLcd::SeekTo, &*player_, &Player::SeekTo);
  layout->addWidget(lcd_, 6);
  layout->addStretch(1);

  // The analyzer sits next to the display, like the visualizer it replaces.
  ui_->analyzer->setFixedSize(110, 40);
  MoveWidget(ui_->analyzer, layout);

  // Search, moved from the playlist toolbar.
  SearchField *search_field = ui_->playlist->findChild<SearchField*>(u"search_field"_s);
  if (search_field) {
    search_field->setFixedWidth(170);
    search_column_ = LabeledColumn(search_field, tr("Search"), bar);
    QSizePolicy size_policy = search_column_->sizePolicy();
    size_policy.setRetainSizeWhenHidden(true);
    search_column_->setSizePolicy(size_policy);
    layout->addWidget(search_column_);
  }

  return bar;

}

ITunesButton *ITunesTheme::AddBottomButton(const ITunesButton::Glyph glyph, const QString &tooltip, QBoxLayout *layout) {

  ITunesButton *button = new ITunesButton(glyph, ITunesButton::Shape::RoundedRect, kBottomButtonHeight, ui_->status_bar_internal);
  button->setToolTip(tooltip);
  layout->addWidget(button);
  return button;

}

void ITunesTheme::RearrangeStatusBar() {

  // The transport controls have moved to the top, so the stock player control row is not needed.
  ui_->player_controls_container->hide();
  ui_->line_6->hide();
  ui_->status_bar_line->hide();
  ui_->line_5->hide();
  ui_->line_2->hide();
  ui_->line_love->hide();
  // Progress is shown in the status display.
  ui_->track_slider->hide();
  // Shuffle and repeat get their own buttons, the stock widget keeps the modes.
  ui_->playlist_sequence->hide();

  // The bottom bar runs along the whole window, below the source list too.
  ui_->status_bar->setParent(central_widget_);
  ui_->layout_centralWidget->addWidget(ui_->status_bar);
  ui_->status_bar->show();

  QHBoxLayout *layout = ui_->layout_status_bar_internal;
  while (QLayoutItem *item = layout->takeAt(0)) {
    delete item;
  }
  layout->setContentsMargins(6, 5, 6, 5);
  layout->setSpacing(6);

  // Left: playlist, playback order and artwork
  ITunesButton *add_button = AddBottomButton(ITunesButton::Glyph::Plus, QString(), layout);
  add_button->SetAction(ui_->action_new_playlist);
  add_button->setToolTip(tr("Create a playlist"));

  PlaylistSequence *sequence = ui_->playlist_sequence;
  shuffle_button_ = AddBottomButton(ITunesButton::Glyph::Shuffle, QString(), layout);
  shuffle_button_->setCheckable(true);
  shuffle_button_->setContextMenuPolicy(Qt::CustomContextMenu);
  QObject::connect(shuffle_button_, &ITunesButton::clicked, this, &ITunesTheme::ShuffleClicked);
  QObject::connect(shuffle_button_, &ITunesButton::customContextMenuRequested, this, [this, sequence](const QPoint &pos) { sequence->shuffle_menu()->exec(shuffle_button_->mapToGlobal(pos)); });
  QObject::connect(sequence, &PlaylistSequence::ShuffleModeChanged, this, &ITunesTheme::ShuffleModeChanged);
  ShuffleModeChanged(sequence->shuffle_mode());

  repeat_button_ = AddBottomButton(ITunesButton::Glyph::Repeat, QString(), layout);
  repeat_button_->setCheckable(true);
  repeat_button_->setContextMenuPolicy(Qt::CustomContextMenu);
  QObject::connect(repeat_button_, &ITunesButton::clicked, this, &ITunesTheme::RepeatClicked);
  QObject::connect(repeat_button_, &ITunesButton::customContextMenuRequested, this, [this, sequence](const QPoint &pos) { sequence->repeat_menu()->exec(repeat_button_->mapToGlobal(pos)); });
  QObject::connect(sequence, &PlaylistSequence::RepeatModeChanged, this, &ITunesTheme::RepeatModeChanged);
  RepeatModeChanged(sequence->repeat_mode());

  artwork_button_ = AddBottomButton(ITunesButton::Glyph::Artwork, tr("Show or hide the artwork of the song playing"), layout);
  artwork_button_->setCheckable(true);
  QObject::connect(artwork_button_, &ITunesButton::clicked, this, &ITunesTheme::SetArtworkVisible);

  // Middle: what's in the song list, or progress while loading
  layout->addStretch(1);
  MoveWidget(ui_->status_bar_stack, layout);
  ui_->playlist_summary->setAlignment(Qt::AlignCenter);
  layout->addStretch(1);

  // Right: what is playing exactly, and the views that sit around the song list
  format_label_ = new QLabel(ui_->status_bar_internal);
  format_label_->setObjectName(u"itunes_format"_s);
  format_label_->setToolTip(tr("Format of the song playing"));
  layout->addWidget(format_label_);
  hires_label_ = new QLabel(tr("Hi-Res"), ui_->status_bar_internal);
  hires_label_->setObjectName(u"itunes_hires"_s);
  hires_label_->setToolTip(tr("Lossless, with more than 16 bits or 48 kHz"));
  hires_label_->hide();
  layout->addWidget(hires_label_);
  layout->addSpacing(6);
  QObject::connect(&*playlist_manager_, &PlaylistManager::CurrentSongChanged, this, &ITunesTheme::SongChanged);
  QObject::connect(&*player_, &Player::Stopped, this, [this]() { SongChanged(Song()); });

  MoveWidget(ui_->button_scrobble, layout);
  MoveWidget(ui_->widget_love, layout);

  ITunesButton *output_button = AddBottomButton(ITunesButton::Glyph::Speaker, tr("Audio output"), layout);
  QObject::connect(output_button, &ITunesButton::clicked, this, &ITunesTheme::OpenOutputSettings);

  browse_button_ = AddBottomButton(ITunesButton::Glyph::Columns, tr("Browse the library"), layout);
  browse_button_->setCheckable(true);
  QObject::connect(browse_button_, &ITunesButton::clicked, this, &ITunesTheme::BrowseClicked);

  if (context_pane_) {
    context_button_ = AddBottomButton(ITunesButton::Glyph::Lyrics, tr("Show or hide lyrics and song information"), layout);
    context_button_->setCheckable(true);
    QObject::connect(context_button_, &ITunesButton::clicked, this, &ITunesTheme::SetContextVisible);
  }

  ITunesButton *equalizer_button = AddBottomButton(ITunesButton::Glyph::Equalizer, QString(), layout);
  equalizer_button->SetAction(ui_->action_equalizer);

}

void ITunesTheme::CreateContextPane() {

  // The context view, with lyrics and song information, sits on the right of the song list instead of being a view of its own.
  QWidget *context_view = nullptr;
  for (int i = 0; i < tabs_->count(); ++i) {
    FancyTabData *tab_data = tabs_->tabBar()->tabData(i).value<FancyTabData*>();
    if (tab_data && tab_data->name() == "context"_L1) {
      context_view = tab_data->widget_view();
      break;
    }
  }
  if (!context_view) return;

  context_pane_ = new QWidget(splitter_);
  context_pane_->setObjectName(u"itunes_context_pane"_s);
  QVBoxLayout *layout = new QVBoxLayout(context_pane_);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(context_view);
  context_view->show();
  splitter_->addWidget(context_pane_);
  splitter_->setStretchFactor(2, 0);

  source_list_->SetExcludedViews(QStringList() << u"context"_s);

}

void ITunesTheme::RestoreSettings() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  const int sidebar_width = std::max(100, s.value(kSidebarWidth, kDefaultSidebarWidth).toInt());
  const int context_width = std::max(150, s.value(kContextWidth, kDefaultContextWidth).toInt());
  const bool context_visible = s.value(kContextVisible, false).toBool();
  const bool artwork_visible = s.value(kArtworkVisible, true).toBool();
  if (s.contains(kHiddenSources)) {
    source_list_->SetHiddenViews(s.value(kHiddenSources).toStringList());
  }
  const QString source = s.value(kSource, u"collection"_s).toString();
  s.endGroup();

  // The stock layout's splitter state is kept for when the layout is switched back, so the widths are restored separately.
  splitter_->setStretchFactor(0, 0);
  splitter_->setStretchFactor(1, 1);
  QList<int> sizes = QList<int>() << sidebar_width << std::max(400, mainwindow_->width() - sidebar_width - context_width);
  if (context_pane_) sizes << context_width;
  splitter_->setSizes(sizes);

  SetContextVisible(context_visible);
  SetArtworkVisible(artwork_visible);
  source_list_->SetCurrentSource(source);

}

void ITunesTheme::SetContextVisible(const bool visible) {

  if (!context_pane_) return;
  context_pane_->setVisible(visible);
  context_button_->setChecked(visible);

}

void ITunesTheme::SetArtworkVisible(const bool visible) {

  artwork_visible_ = visible;
  artwork_button_->setChecked(visible);
  ui_->widget_playing->SetEnabled(visible);

}

void ITunesTheme::ShuffleClicked() {

  PlaylistSequence *sequence = ui_->playlist_sequence;
  sequence->SetShuffleMode(sequence->shuffle_mode() == PlaylistSequence::ShuffleMode::Off ? PlaylistSequence::ShuffleMode::All : PlaylistSequence::ShuffleMode::Off);

}

void ITunesTheme::RepeatClicked() {

  // Off, the whole playlist, one song, like the jukebox this layout is modelled on.
  // The other modes are in the menu on a right click.
  PlaylistSequence *sequence = ui_->playlist_sequence;
  switch (sequence->repeat_mode()) {
    case PlaylistSequence::RepeatMode::Off:
      sequence->SetRepeatMode(PlaylistSequence::RepeatMode::Playlist);
      break;
    case PlaylistSequence::RepeatMode::Playlist:
      sequence->SetRepeatMode(PlaylistSequence::RepeatMode::Track);
      break;
    default:
      sequence->SetRepeatMode(PlaylistSequence::RepeatMode::Off);
      break;
  }

}

void ITunesTheme::ShuffleModeChanged(const PlaylistSequence::ShuffleMode mode) {

  QString text;
  switch (mode) {
    case PlaylistSequence::ShuffleMode::Off:         text = tr("Off"); break;
    case PlaylistSequence::ShuffleMode::All:         text = tr("All songs"); break;
    case PlaylistSequence::ShuffleMode::InsideAlbum: text = tr("Within an album"); break;
    case PlaylistSequence::ShuffleMode::Albums:      text = tr("Albums"); break;
    case PlaylistSequence::ShuffleMode::Grouping:    text = tr("Within a grouping"); break;
  }
  shuffle_button_->setChecked(mode != PlaylistSequence::ShuffleMode::Off);
  shuffle_button_->setToolTip(tr("Shuffle: %1 (right click for more)").arg(text));

}

void ITunesTheme::RepeatModeChanged(const PlaylistSequence::RepeatMode mode) {

  QString text;
  switch (mode) {
    case PlaylistSequence::RepeatMode::Off:      text = tr("Off"); break;
    case PlaylistSequence::RepeatMode::Track:    text = tr("One song"); break;
    case PlaylistSequence::RepeatMode::Album:    text = tr("Album"); break;
    case PlaylistSequence::RepeatMode::Playlist: text = tr("Playlist"); break;
    case PlaylistSequence::RepeatMode::OneByOne: text = tr("Stop after each song"); break;
    case PlaylistSequence::RepeatMode::Scan:     text = tr("Intro scan"); break;
  }
  repeat_button_->setChecked(mode != PlaylistSequence::RepeatMode::Off);
  repeat_button_->SetGlyph(mode == PlaylistSequence::RepeatMode::Track ? ITunesButton::Glyph::RepeatOne : ITunesButton::Glyph::Repeat);
  repeat_button_->setToolTip(tr("Repeat: %1 (right click for more)").arg(text));

}

void ITunesTheme::SongChanged(const Song &song) {

  // For the ones who care: the exact format of the song playing, e.g. "FLAC · 24-bit · 96 kHz · 2304 kbps".
  QStringList parts;
  if (song.is_valid()) {
    if (song.filetype() != Song::FileType::Unknown) parts << song.TextForFiletype();
    if (song.bitdepth() > 0) parts << tr("%1-bit").arg(song.bitdepth());
    if (song.samplerate() > 0) parts << tr("%1 kHz").arg(QString::number(song.samplerate() / 1000.0, 'g', 4));
    if (song.bitrate() > 0) parts << tr("%1 kbps").arg(song.bitrate());
  }
  format_label_->setText(parts.join(u" · "_s));
  hires_label_->setVisible(song.is_valid() && song.IsFileLossless() && (song.bitdepth() > 16 || song.samplerate() > 48000));

}

void ITunesTheme::RearrangeSidebar() {

  // The tab widget is only used to switch between the views, the source list replaces its tab bar.
  original_tab_mode_ = tabs_->mode();
  tabs_->SetMode(FancyTabWidget::Mode::Tabs);
  tabs_->tabBar()->hide();
  tabs_->setDocumentMode(true);
  QObject::connect(tabs_, &FancyTabWidget::ModeChanged, this, &ITunesTheme::TabModeChanged);

  // The now playing artwork lives at the bottom of the source list instead of moving between the views.
  tabs_->AddBottomWidget(nullptr);

  // Playlists are picked and managed in the source list, so the playlist tabs and the toolbar buttons are not needed.
  // The toolbar itself stays visible with no height, the song list is only filtered while it is visible.
  if (QWidget *tab_bar = ui_->playlist->findChild<QWidget*>(u"tab_bar"_s)) {
    tab_bar->hide();
  }
  if (QWidget *toolbar = ui_->playlist->findChild<QWidget*>(u"toolbar"_s)) {
    const QList<QWidget*> toolbar_widgets = toolbar->findChildren<QWidget*>(Qt::FindDirectChildrenOnly);
    for (QWidget *widget : toolbar_widgets) widget->hide();
    toolbar->setFixedHeight(0);
  }

  QWidget *source_column = new QWidget(ui_->sidebar_layout);
  source_column->setObjectName(u"itunes_source_column"_s);
  QVBoxLayout *source_layout = new QVBoxLayout(source_column);
  source_layout->setContentsMargins(0, 0, 0, 0);
  source_layout->setSpacing(0);
  source_list_ = new ITunesSourceList(tabs_, playlist_manager_, source_column);
  source_layout->addWidget(source_list_, 1);
  MoveWidget(ui_->widget_playing, source_layout);
  ui_->layout_left->insertWidget(0, source_column, 1);

  // The views are shown in place of the song list.
  ui_->layout_right->insertWidget(0, tabs_, 1);
  ui_->layout_right->setStretchFactor(ui_->playlist, 1);
  QObject::connect(source_list_, &ITunesSourceList::SourceChanged, this, &ITunesTheme::ShowSource);

  // New playlists are named in the source list, also when created from the menu.
  QObject::disconnect(ui_->action_new_playlist, &QAction::triggered, ui_->playlist, nullptr);
  QObject::connect(ui_->action_new_playlist, &QAction::triggered, source_list_, &ITunesSourceList::NewPlaylist);

}

void ITunesTheme::ShowSource(const bool playlist) {

  tabs_->setVisible(!playlist);
  ui_->playlist->setVisible(playlist);
  // Search and the summary are about the song list, the views have their own search.
  if (search_column_) search_column_->setVisible(playlist);
  ui_->playlist_summary->setVisible(playlist);
  if (browse_button_) browse_button_->setChecked(source_list_->CurrentSource() == "collection"_L1);

}

void ITunesTheme::BrowseClicked(const bool checked) {

  // Browse switches between the library and the playlist last shown.
  if (checked) {
    source_list_->SelectLibrary();
  }
  else {
    source_list_->SelectCurrentPlaylist();
  }

}

void ITunesTheme::TabModeChanged(const FancyTabWidget::Mode mode) {

  // The tab bar is hidden, but a mode can still be picked from the context menu.
  // Remember it for the stock layout and stay in tabs mode.
  if (mode == FancyTabWidget::Mode::Tabs) return;
  original_tab_mode_ = mode;
  tabs_->SetMode(FancyTabWidget::Mode::Tabs);

}

void ITunesTheme::SaveSettings() {

  if (!source_list_) return;

  Settings s;

  // Don't let the tabs mode used here replace the mode of the stock layout.
  s.beginGroup(MainWindowSettings::kSettingsGroup);
  s.setValue(FancyTabWidget::kTabMode, static_cast<int>(original_tab_mode_));
  s.endGroup();

  s.beginGroup(kSettingsGroup);
  const QList<int> sizes = splitter_->sizes();
  if (!sizes.isEmpty() && sizes[0] > 0) {
    s.setValue(kSidebarWidth, sizes[0]);
  }
  if (context_pane_ && sizes.count() > 2 && sizes[2] > 0) {
    s.setValue(kContextWidth, sizes[2]);
  }
  s.setValue(kContextVisible, context_pane_ && !context_pane_->isHidden());
  s.setValue(kArtworkVisible, artwork_visible_);
  s.setValue(kSource, source_list_->CurrentSource());
  s.setValue(kHiddenSources, source_list_->hidden_views());
  // Settings of the earlier versions of this layout.
  s.remove(u"source_splitter_state"_s);
  s.remove(u"browse"_s);
  s.remove(u"browse_width"_s);
  s.remove(u"hidden_views"_s);
  s.endGroup();

}

bool ITunesTheme::eventFilter(QObject *obj, QEvent *e) {

  if (obj == central_widget_ && e->type() == QEvent::Paint) {
    QPainter p(central_widget_);
    p.drawTiledPixmap(central_widget_->rect(), metal_);
    // Light from above
    QLinearGradient shade(0, 0, 0, central_widget_->height());
    shade.setColorAt(0, QColor(255, 255, 255, 60));
    shade.setColorAt(0.4, QColor(255, 255, 255, 0));
    shade.setColorAt(1, QColor(0, 0, 0, 30));
    p.fillRect(central_widget_->rect(), shade);
  }
  else if (obj == volume_ && e->type() == QEvent::EnabledChange) {
    volume_slider_->setEnabled(volume_->isEnabled());
  }

  return QObject::eventFilter(obj, e);

}

QPixmap ITunesTheme::BrushedMetalTexture() {

  // Horizontal streaks: random noise per row, smeared out horizontally.
  // The blur wraps around so the texture tiles seamlessly, and rows are independent so it tiles vertically too.
  constexpr int kWidth = 512;
  constexpr int kHeight = 256;
  constexpr int kBlurRadius = 24;
  constexpr int kBlurSize = kBlurRadius * 2 + 1;

  QRandomGenerator random(2004);
  QImage image(kWidth, kHeight, QImage::Format_RGB32);
  std::vector<int> noise(kWidth);

  for (int y = 0; y < kHeight; ++y) {
    const int row_base = 192 + random.bounded(-5, 6);
    for (int x = 0; x < kWidth; ++x) {
      noise[x] = random.bounded(-40, 41);
    }
    int sum = 0;
    for (int k = -kBlurRadius; k <= kBlurRadius; ++k) {
      sum += noise[(k + kWidth) % kWidth];
    }
    QRgb *line = reinterpret_cast<QRgb*>(image.scanLine(y));
    for (int x = 0; x < kWidth; ++x) {
      const int value = std::clamp(row_base + (sum * 3) / kBlurSize, 0, 255);
      line[x] = qRgb(value, value, std::min(255, value + 3));
      sum += noise[(x + kBlurRadius + 1) % kWidth] - noise[(x - kBlurRadius + kWidth) % kWidth];
    }
  }

  return QPixmap::fromImage(image);

}

QPixmap ITunesTheme::SpeakerPixmap(const bool loud) {

  const qreal dpr = 2.0;
  QPixmap pixmap(QSize(14, 12) * dpr);
  pixmap.setDevicePixelRatio(dpr);
  pixmap.fill(Qt::transparent);

  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(50, 50, 50));

  QPainterPath speaker;
  speaker.moveTo(1, 4);
  speaker.lineTo(3.5, 4);
  speaker.lineTo(7, 1);
  speaker.lineTo(7, 11);
  speaker.lineTo(3.5, 8);
  speaker.lineTo(1, 8);
  speaker.closeSubpath();
  p.drawPath(speaker);

  if (loud) {
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(50, 50, 50), 1.1));
    p.drawArc(QRectF(5, 3.5, 4, 5), -60 * 16, 120 * 16);
    p.drawArc(QRectF(5, 1.5, 7, 9), -60 * 16, 120 * 16);
  }

  return pixmap;

}
