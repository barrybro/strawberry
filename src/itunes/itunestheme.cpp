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
#include "playlist/playlistmanager.h"
#include "widgets/searchfield.h"
#include "itunesbutton.h"
#include "ituneslcd.h"
#include "itunessourcelist.h"
#include "itunestheme.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kSettingsGroup[] = "ITunesLayout";
constexpr char kSourceSplitterState[] = "source_splitter_state";
constexpr char kBrowse[] = "browse";
constexpr char kBrowseWidth[] = "browse_width";
constexpr int kDefaultSourceListWidth = 170;
constexpr int kDefaultBrowseWidth = 300;

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
      tabs_(nullptr),
      splitter_(nullptr),
      source_splitter_(nullptr),
      volume_slider_(nullptr),
      play_button_(nullptr),
      browse_button_(nullptr),
      lcd_(nullptr),
      original_tab_mode_(FancyTabWidget::Mode::LargeSidebar),
      browse_width_(kDefaultBrowseWidth) {}

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
  RearrangeStatusBar();
  RearrangeSidebar();

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
    layout->addWidget(LabeledColumn(search_field, tr("Search"), bar));
  }

  // Browse shows and hides the collection browser next to the source list.
  browse_button_ = new ITunesButton(ITunesButton::Glyph::Browse, ITunesButton::Shape::Round, 30, bar);
  browse_button_->setCheckable(true);
  browse_button_->setToolTip(tr("Browse"));
  QObject::connect(browse_button_, &ITunesButton::clicked, this, &ITunesTheme::SetBrowseVisible);
  layout->addWidget(LabeledColumn(browse_button_, tr("Browse"), bar));

  return bar;

}

void ITunesTheme::RearrangeStatusBar() {

  // The transport controls have moved to the top, so the stock player control row is not needed.
  ui_->player_controls->hide();
  ui_->line_6->hide();
  ui_->status_bar_line->hide();
  ui_->line_5->hide();
  ui_->line_2->hide();
  ui_->line_love->hide();
  // Progress is shown in the status display.
  ui_->track_slider->hide();

  QHBoxLayout *layout = ui_->layout_status_bar_internal;
  while (QLayoutItem *item = layout->takeAt(0)) {
    delete item;
  }
  layout->setContentsMargins(6, 4, 6, 4);
  layout->setSpacing(6);

  ITunesButton *add_button = new ITunesButton(ITunesButton::Glyph::Plus, ITunesButton::Shape::RoundedRect, 20, ui_->status_bar_internal);
  add_button->SetAction(ui_->action_new_playlist);
  layout->addWidget(add_button);
  MoveWidget(ui_->playlist_sequence, layout);
  layout->addStretch(1);
  MoveWidget(ui_->status_bar_stack, layout);
  layout->addStretch(1);
  MoveWidget(ui_->button_scrobble, layout);
  MoveWidget(ui_->widget_love, layout);

  ui_->playlist_summary->setAlignment(Qt::AlignCenter);

}

void ITunesTheme::RearrangeSidebar() {

  // Replace the tab bar with the source list, the tab widget is only used to switch between the views.
  original_tab_mode_ = tabs_->mode();
  tabs_->SetMode(FancyTabWidget::Mode::Tabs);
  tabs_->tabBar()->hide();
  tabs_->setDocumentMode(true);
  QObject::connect(tabs_, &FancyTabWidget::ModeChanged, this, &ITunesTheme::TabModeChanged);

  // The now playing artwork lives at the bottom of the source list instead of moving between the views.
  tabs_->AddBottomWidget(nullptr);

  source_splitter_ = new QSplitter(Qt::Horizontal, ui_->sidebar_layout);
  source_splitter_->setObjectName(u"itunes_source_splitter"_s);
  source_splitter_->setChildrenCollapsible(false);

  QWidget *source_column = new QWidget(source_splitter_);
  source_column->setObjectName(u"itunes_source_column"_s);
  QVBoxLayout *source_layout = new QVBoxLayout(source_column);
  source_layout->setContentsMargins(0, 0, 0, 0);
  source_layout->setSpacing(0);
  source_layout->addWidget(new ITunesSourceList(tabs_, source_column), 1);
  MoveWidget(ui_->widget_playing, source_layout);

  source_splitter_->addWidget(source_column);
  source_splitter_->addWidget(tabs_);
  source_splitter_->setStretchFactor(0, 0);
  source_splitter_->setStretchFactor(1, 1);
  ui_->layout_left->insertWidget(0, source_splitter_, 1);

  Settings s;
  s.beginGroup(kSettingsGroup);
  const QByteArray source_splitter_state = s.value(kSourceSplitterState).toByteArray();
  const bool browse = s.value(kBrowse, true).toBool();
  browse_width_ = s.value(kBrowseWidth, kDefaultBrowseWidth).toInt();
  s.endGroup();

  if (source_splitter_state.isEmpty() || !source_splitter_->restoreState(source_splitter_state)) {
    source_splitter_->setSizes(QList<int>() << kDefaultSourceListWidth << kDefaultBrowseWidth);
    // First time in this layout, make room for both the source list and the browser.
    QList<int> sizes = splitter_->sizes();
    const int sidebar_width = kDefaultSourceListWidth + (browse ? kDefaultBrowseWidth : 0);
    if (sizes.count() == 2 && sizes[0] < sidebar_width) {
      sizes[1] = std::max(0, sizes[1] - (sidebar_width - sizes[0]));
      sizes[0] = sidebar_width;
      splitter_->setSizes(sizes);
    }
  }

  // The main splitter was saved with the browser in this state already, so don't resize it.
  tabs_->setVisible(browse);
  browse_button_->setChecked(browse);

}

void ITunesTheme::SetBrowseVisible(const bool visible) {

  browse_button_->setChecked(visible);
  if (visible == !tabs_->isHidden()) return;

  // Grow or shrink the sidebar by the width of the browser, so the source list keeps its size.
  QList<int> sizes = splitter_->sizes();
  if (visible) {
    tabs_->show();
    if (sizes.count() == 2) {
      const int width = std::min(browse_width_, sizes[1] - 100);
      sizes[0] += width;
      sizes[1] -= width;
    }
  }
  else {
    browse_width_ = tabs_->width() + source_splitter_->handleWidth();
    tabs_->hide();
    if (sizes.count() == 2) {
      sizes[0] -= browse_width_;
      sizes[1] += browse_width_;
    }
  }
  splitter_->setSizes(sizes);

}

void ITunesTheme::TabModeChanged(const FancyTabWidget::Mode mode) {

  // The tab bar is hidden, but a mode can still be picked from the context menu.
  // Remember it for the stock layout and stay in tabs mode.
  if (mode == FancyTabWidget::Mode::Tabs) return;
  original_tab_mode_ = mode;
  tabs_->SetMode(FancyTabWidget::Mode::Tabs);

}

void ITunesTheme::SaveSettings() {

  if (!source_splitter_) return;

  Settings s;

  // Don't let the tabs mode used here replace the mode of the stock layout.
  s.beginGroup(MainWindowSettings::kSettingsGroup);
  s.setValue(FancyTabWidget::kTabMode, static_cast<int>(original_tab_mode_));
  s.endGroup();

  s.beginGroup(kSettingsGroup);
  s.setValue(kSourceSplitterState, source_splitter_->saveState());
  s.setValue(kBrowse, !tabs_->isHidden());
  s.setValue(kBrowseWidth, browse_width_);
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
