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

#ifndef ITUNESLCD_H
#define ITUNESLCD_H

#include <QtGlobal>
#include <QWidget>
#include <QString>
#include <QSize>
#include <QRectF>

#include "includes/shared_ptr.h"
#include "core/song.h"

class QTimer;
class QPainter;
class QPaintEvent;
class QMouseEvent;
class Player;

// The backlit "status display" in the middle of the top bar.
// Shows the song title, alternates between artist and album on the second line, and has a seekable progress bar with elapsed and remaining time.
class ITunesLcd : public QWidget {
  Q_OBJECT

 public:
  explicit ITunesLcd(SharedPtr<Player> player, QWidget *parent = nullptr);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

 public Q_SLOTS:
  void SongChanged(const Song &song);
  void Playing();
  void Paused();
  void Stopped();

 Q_SIGNALS:
  void SeekTo(const quint64 seconds);

 protected:
  void paintEvent(QPaintEvent *e) override;
  void mousePressEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;

 private Q_SLOTS:
  void UpdatePosition();
  void RotateLine();

 private:
  QRectF ProgressRect() const;
  QRectF RemainingRect() const;
  qint64 SecondsAt(const qreal x) const;
  void DrawBackground(QPainter *p, const QRectF &rect) const;

 private:
  SharedPtr<Player> player_;
  QTimer *position_timer_;
  QTimer *rotate_timer_;

  Song song_;
  bool active_;
  bool can_seek_;
  bool show_total_;
  int rotate_index_;
  qint64 position_sec_;
  qint64 length_sec_;

  bool dragging_;
  qint64 drag_sec_;
};

#endif  // ITUNESLCD_H
