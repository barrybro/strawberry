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

#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QPolygonF>
#include <QFont>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QMouseEvent>

#include "constants/timeconstants.h"
#include "core/player.h"
#include "engine/enginebase.h"
#include "playlist/playlistitem.h"
#include "utilities/timeutils.h"
#include "ituneslcd.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kPositionUpdateMsec = 250;
constexpr int kRotateMsec = 3500;
constexpr qreal kTimeWidth = 44.0;
const QColor kTextColor(40, 42, 28);
}  // namespace

ITunesLcd::ITunesLcd(SharedPtr<Player> player, QWidget *parent)
    : QWidget(parent),
      player_(player),
      position_timer_(new QTimer(this)),
      rotate_timer_(new QTimer(this)),
      active_(false),
      can_seek_(false),
      show_total_(false),
      rotate_index_(0),
      position_sec_(0),
      length_sec_(0),
      dragging_(false),
      drag_sec_(0) {

  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setMouseTracking(false);

  position_timer_->setInterval(kPositionUpdateMsec);
  QObject::connect(position_timer_, &QTimer::timeout, this, &ITunesLcd::UpdatePosition);

  rotate_timer_->setInterval(kRotateMsec);
  QObject::connect(rotate_timer_, &QTimer::timeout, this, &ITunesLcd::RotateLine);

}

QSize ITunesLcd::sizeHint() const { return QSize(460, 50); }

QSize ITunesLcd::minimumSizeHint() const { return QSize(260, 50); }

void ITunesLcd::SongChanged(const Song &song) {

  song_ = song;
  rotate_index_ = 0;
  position_sec_ = 0;
  length_sec_ = song.length_nanosec() / kNsecPerSec;
  update();

}

void ITunesLcd::Playing() {

  active_ = true;

  PlaylistItemPtr item = player_->GetCurrentItem();
  can_seek_ = item && !(item->options() & PlaylistItem::Option::SeekDisabled);

  position_timer_->start();
  rotate_timer_->start();
  UpdatePosition();

}

void ITunesLcd::Paused() {

  active_ = true;
  position_timer_->stop();
  UpdatePosition();

}

void ITunesLcd::Stopped() {

  active_ = false;
  dragging_ = false;
  song_ = Song();
  position_sec_ = 0;
  length_sec_ = 0;
  position_timer_->stop();
  rotate_timer_->stop();
  update();

}

void ITunesLcd::UpdatePosition() {

  position_sec_ = player_->engine()->position_nanosec() / kNsecPerSec;
  const qint64 length_sec = player_->engine()->length_nanosec() / kNsecPerSec;
  if (length_sec > 0) length_sec_ = length_sec;
  update();

}

void ITunesLcd::RotateLine() {

  ++rotate_index_;
  update();

}

QRectF ITunesLcd::ProgressRect() const {

  const QRectF r = QRectF(rect()).adjusted(12, 0, -12, 0);
  return QRectF(r.left() + kTimeWidth, height() - 15, r.width() - kTimeWidth * 2.0, 7);

}

QRectF ITunesLcd::RemainingRect() const {

  const QRectF progress = ProgressRect();
  return QRectF(progress.right(), progress.top() - 5, kTimeWidth, progress.height() + 10);

}

qint64 ITunesLcd::SecondsAt(const qreal x) const {

  const QRectF progress = ProgressRect();
  const qreal fraction = std::clamp((x - progress.left()) / progress.width(), 0.0, 1.0);
  return static_cast<qint64>(fraction * static_cast<qreal>(length_sec_));

}

void ITunesLcd::DrawBackground(QPainter *p, const QRectF &rect) const {

  // Outer bevel, slightly darker than the metal around it
  QLinearGradient bevel(rect.topLeft(), rect.bottomLeft());
  bevel.setColorAt(0, QColor(95, 97, 85));
  bevel.setColorAt(1, QColor(185, 187, 175));
  p->setPen(Qt::NoPen);
  p->setBrush(bevel);
  p->drawRoundedRect(rect, 7, 7);

  // The backlit glass
  const QRectF glass = rect.adjusted(1.5, 1.5, -1.5, -1.5);
  QLinearGradient light(glass.topLeft(), glass.bottomLeft());
  light.setColorAt(0, QColor(222, 226, 190));
  light.setColorAt(0.15, QColor(240, 243, 214));
  light.setColorAt(1, QColor(228, 232, 196));
  p->setBrush(light);
  p->drawRoundedRect(glass, 6, 6);

}

void ITunesLcd::paintEvent(QPaintEvent *e) {

  Q_UNUSED(e)

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);

  DrawBackground(&p, QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));

  const QRectF content = QRectF(rect()).adjusted(12, 3, -12, -3);

  QFont font_normal(font());
  font_normal.setPointSizeF(std::max(7.0, font().pointSizeF() * 0.9));
  QFont font_bold(font_normal);
  font_bold.setBold(true);

  p.setPen(kTextColor);

  if (!active_ || !song_.is_valid()) {
    QFont font_title(font_bold);
    font_title.setPointSizeF(font_bold.pointSizeF() * 1.25);
    p.setFont(font_title);
    p.drawText(content, Qt::AlignCenter, u"Strawberry"_s);
    return;
  }

  // Line 1: Title
  const QRectF line1(content.left(), content.top(), content.width(), 14);
  p.setFont(font_bold);
  p.drawText(line1, Qt::AlignCenter, QFontMetrics(font_bold).elidedText(song_.PrettyTitle(), Qt::ElideRight, static_cast<int>(line1.width())));

  // Line 2: Alternates between artist and album
  QStringList details;
  if (!song_.artist().isEmpty()) details << song_.artist();
  else if (!song_.effective_albumartist().isEmpty()) details << song_.effective_albumartist();
  if (!song_.album().isEmpty()) details << song_.album();
  const QRectF line2(content.left(), line1.bottom(), content.width(), 14);
  if (!details.isEmpty()) {
    p.setFont(font_normal);
    const QString text = details.value(rotate_index_ % details.count());
    p.drawText(line2, Qt::AlignCenter, QFontMetrics(font_normal).elidedText(text, Qt::ElideRight, static_cast<int>(line2.width())));
  }

  // Line 3: Elapsed, progress and remaining
  p.setFont(font_normal);
  const qint64 position = dragging_ ? drag_sec_ : position_sec_;
  const QRectF progress = ProgressRect();
  const QRectF elapsed_rect(content.left(), progress.top() - 5, kTimeWidth, progress.height() + 10);

  if (length_sec_ <= 0) {
    // Streams have no length, just show the elapsed time.
    p.drawText(QRectF(content.left(), progress.top() - 5, content.width(), progress.height() + 10), Qt::AlignCenter, Utilities::PrettyTime(static_cast<int>(position)));
    return;
  }

  p.drawText(elapsed_rect, Qt::AlignLeft | Qt::AlignVCenter, Utilities::PrettyTime(static_cast<int>(position)));
  const QString right_text = show_total_ ? Utilities::PrettyTime(static_cast<int>(length_sec_)) : u"-"_s + Utilities::PrettyTime(static_cast<int>(std::max(0LL, length_sec_ - position)));
  p.drawText(RemainingRect(), Qt::AlignRight | Qt::AlignVCenter, right_text);

  // Progress bar outline, with the played part filled
  const qreal fraction = std::clamp(static_cast<qreal>(position) / static_cast<qreal>(length_sec_), 0.0, 1.0);
  const QRectF bar = progress.adjusted(4, 0.5, -4, -0.5);
  p.setPen(QPen(kTextColor, 1.0));
  p.setBrush(Qt::NoBrush);
  p.drawRect(bar);
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(kTextColor.red(), kTextColor.green(), kTextColor.blue(), 90));
  p.drawRect(QRectF(bar.left(), bar.top(), bar.width() * fraction, bar.height()));

  // Diamond position marker
  const QPointF c(bar.left() + bar.width() * fraction, bar.center().y());
  const qreal d = bar.height() * 0.8;
  QPolygonF diamond;
  diamond << QPointF(c.x(), c.y() - d) << QPointF(c.x() + d * 0.75, c.y()) << QPointF(c.x(), c.y() + d) << QPointF(c.x() - d * 0.75, c.y());
  p.setBrush(kTextColor);
  p.drawPolygon(diamond);

}

void ITunesLcd::mousePressEvent(QMouseEvent *e) {

  if (!active_ || length_sec_ <= 0) {
    QWidget::mousePressEvent(e);
    return;
  }

  const QPointF pos = e->position();

  if (RemainingRect().contains(pos)) {
    show_total_ = !show_total_;
    update();
    return;
  }

  if (can_seek_ && ProgressRect().adjusted(0, -5, 0, 5).contains(pos)) {
    dragging_ = true;
    drag_sec_ = SecondsAt(pos.x());
    update();
    return;
  }

  QWidget::mousePressEvent(e);

}

void ITunesLcd::mouseMoveEvent(QMouseEvent *e) {

  if (dragging_) {
    drag_sec_ = SecondsAt(e->position().x());
    update();
    return;
  }

  QWidget::mouseMoveEvent(e);

}

void ITunesLcd::mouseReleaseEvent(QMouseEvent *e) {

  if (dragging_) {
    dragging_ = false;
    position_sec_ = drag_sec_;
    Q_EMIT SeekTo(static_cast<quint64>(drag_sec_));
    update();
    return;
  }

  QWidget::mouseReleaseEvent(e);

}
