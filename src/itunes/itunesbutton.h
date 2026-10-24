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

#ifndef ITUNESBUTTON_H
#define ITUNESBUTTON_H

#include <QAbstractButton>
#include <QSize>

class QPainter;
class QPaintEvent;
class QAction;

// A brushed-metal era "aqua" button, painted entirely in code.
// Round buttons are used for the transport controls, rounded rectangles for the small buttons in the bottom bar.
class ITunesButton : public QAbstractButton {
  Q_OBJECT

 public:
  enum class Glyph {
    Previous,
    Play,
    Pause,
    Next,
    Browse,
    Plus
  };

  enum class Shape {
    Round,
    RoundedRect
  };

  explicit ITunesButton(const Glyph glyph, const Shape shape, const int diameter, QWidget *parent = nullptr);

  void SetGlyph(const Glyph glyph);
  Glyph glyph() const { return glyph_; }

  // Triggers the action when clicked, and follows its enabled state.
  void SetAction(QAction *action);

  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent *e) override;

 private:
  void DrawGlyph(QPainter *p, const QRectF &rect) const;

 private:
  Glyph glyph_;
  Shape shape_;
  int diameter_;
};

#endif  // ITUNESBUTTON_H
