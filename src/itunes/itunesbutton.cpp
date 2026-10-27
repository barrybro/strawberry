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

#include <QAbstractButton>
#include <QAction>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QPolygonF>
#include <QPainterPathStroker>
#include <QPaintEvent>

#include "itunesbutton.h"

namespace {

// The glyphs are filled, so line drawings are turned into outlines.
QPainterPath Stroked(const QPainterPath &path, const qreal width) {

  QPainterPathStroker stroker;
  stroker.setWidth(width);
  stroker.setCapStyle(Qt::RoundCap);
  stroker.setJoinStyle(Qt::RoundJoin);
  return stroker.createStroke(path);

}

QPainterPath Arrowhead(const QPointF &tip, const qreal dx, const qreal h) {

  QPainterPath path;
  path.moveTo(tip);
  path.lineTo(tip.x() - dx, tip.y() - h);
  path.lineTo(tip.x() - dx, tip.y() + h);
  path.closeSubpath();
  return path;

}

}  // namespace

ITunesButton::ITunesButton(const Glyph glyph, const Shape shape, const int diameter, QWidget *parent)
    : QAbstractButton(parent),
      glyph_(glyph),
      shape_(shape),
      diameter_(diameter) {

  setFocusPolicy(Qt::NoFocus);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

}

void ITunesButton::SetGlyph(const Glyph glyph) {

  if (glyph == glyph_) return;
  glyph_ = glyph;
  update();

}

void ITunesButton::SetAction(QAction *action) {

  QObject::connect(this, &ITunesButton::clicked, action, &QAction::trigger);
  QObject::connect(action, &QAction::changed, this, [this, action]() {
    setEnabled(action->isEnabled());
    setToolTip(action->text().remove(u'&'));
  });
  setEnabled(action->isEnabled());
  setToolTip(action->text().remove(u'&'));

}

QSize ITunesButton::sizeHint() const {

  if (shape_ == Shape::RoundedRect) {
    return QSize(diameter_ * 3 / 2, diameter_);
  }

  return QSize(diameter_, diameter_);

}

void ITunesButton::paintEvent(QPaintEvent *e) {

  Q_UNUSED(e)

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const QSize size = sizeHint();
  const QRectF outer(QPointF((width() - size.width()) / 2.0 + 0.5, (height() - size.height()) / 2.0 + 0.5), QSizeF(size.width() - 1, size.height() - 1));
  const QRectF inner = outer.adjusted(1.5, 1.5, -1.5, -1.5);
  const qreal radius = shape_ == Shape::Round ? outer.height() / 2.0 : 4.0;
  const bool sunken = isDown();
  const bool lit = isChecked() && !sunken;

  // Bevel: dark on top, light on the bottom, as if the button sits in a hole in the metal.
  QLinearGradient bevel(outer.topLeft(), outer.bottomLeft());
  bevel.setColorAt(0, QColor(70, 70, 70));
  bevel.setColorAt(1, QColor(150, 150, 150));
  p.setPen(Qt::NoPen);
  p.setBrush(bevel);
  p.drawRoundedRect(outer, radius, radius);

  // Face
  QLinearGradient face(inner.topLeft(), inner.bottomLeft());
  if (sunken) {
    face.setColorAt(0, QColor(150, 150, 150));
    face.setColorAt(0.5, QColor(185, 185, 185));
    face.setColorAt(1, QColor(215, 215, 215));
  }
  else if (lit) {
    face.setColorAt(0, QColor(236, 244, 255));
    face.setColorAt(0.5, QColor(176, 204, 240));
    face.setColorAt(1, QColor(214, 232, 255));
  }
  else {
    face.setColorAt(0, QColor(252, 252, 252));
    face.setColorAt(0.5, QColor(212, 212, 212));
    face.setColorAt(1, QColor(242, 242, 242));
  }
  const qreal inner_radius = shape_ == Shape::Round ? inner.height() / 2.0 : 3.0;
  p.setBrush(face);
  p.drawRoundedRect(inner, inner_radius, inner_radius);

  // Gloss on the upper half
  if (!sunken) {
    QRectF gloss = inner.adjusted(inner.width() * 0.12, 1, -inner.width() * 0.12, -inner.height() * 0.5);
    QLinearGradient gloss_gradient(gloss.topLeft(), gloss.bottomLeft());
    gloss_gradient.setColorAt(0, QColor(255, 255, 255, 220));
    gloss_gradient.setColorAt(1, QColor(255, 255, 255, 30));
    p.setBrush(gloss_gradient);
    p.drawRoundedRect(gloss, gloss.height() / 2.0, gloss.height() / 2.0);
  }

  // Glyph, with a light emboss below it
  // The small buttons in the bottom bar are wider than they are high, so their glyphs can be bigger.
  const qreal glyph_size = shape_ == Shape::RoundedRect ? inner.height() * 1.45 : inner.height();
  const QRectF glyph_rect = QRectF(inner.center() - QPointF(glyph_size / 2.0, glyph_size / 2.0), QSizeF(glyph_size, glyph_size));
  p.setBrush(QColor(255, 255, 255, 170));
  DrawGlyph(&p, glyph_rect.translated(0, 1));
  p.setBrush(!isEnabled() ? QColor(140, 140, 140) : lit ? QColor(24, 84, 178) : QColor(40, 40, 40));
  DrawGlyph(&p, glyph_rect);

}

void ITunesButton::DrawGlyph(QPainter *p, const QRectF &rect) const {

  const qreal s = rect.width();
  const QPointF c = rect.center();

  switch (glyph_) {
    case Glyph::Play:{
      QPolygonF triangle;
      triangle << QPointF(c.x() - s * 0.15, c.y() - s * 0.22) << QPointF(c.x() - s * 0.15, c.y() + s * 0.22) << QPointF(c.x() + s * 0.24, c.y());
      p->drawPolygon(triangle);
      break;
    }
    case Glyph::Pause:{
      const qreal w = s * 0.12;
      const qreal h = s * 0.40;
      p->drawRect(QRectF(c.x() - w - s * 0.05, c.y() - h / 2.0, w, h));
      p->drawRect(QRectF(c.x() + s * 0.05, c.y() - h / 2.0, w, h));
      break;
    }
    case Glyph::Next:
    case Glyph::Previous:{
      const qreal dir = glyph_ == Glyph::Next ? 1.0 : -1.0;
      const qreal w = s * 0.19;
      const qreal h = s * 0.17;
      for (int i = 0; i < 2; ++i) {
        const qreal base = c.x() + dir * (i == 0 ? -w : 0.0);
        QPolygonF triangle;
        triangle << QPointF(base, c.y() - h) << QPointF(base, c.y() + h) << QPointF(base + dir * w, c.y());
        p->drawPolygon(triangle);
      }
      break;
    }
    case Glyph::Plus:{
      const qreal t = s * 0.12;
      const qreal l = s * 0.48;
      p->drawRect(QRectF(c.x() - l / 2.0, c.y() - t / 2.0, l, t));
      p->drawRect(QRectF(c.x() - t / 2.0, c.y() - l / 2.0, t, l));
      break;
    }
    case Glyph::Browse:{
      // An eye
      const qreal w = s * 0.32;
      const qreal h = s * 0.20;
      QPainterPath eye;
      eye.moveTo(c.x() - w, c.y());
      eye.quadTo(c.x(), c.y() - h * 2.0, c.x() + w, c.y());
      eye.quadTo(c.x(), c.y() + h * 2.0, c.x() - w, c.y());
      QPainterPath pupil;
      pupil.addEllipse(c, s * 0.09, s * 0.09);
      QPainterPath white;
      white.addEllipse(c, s * 0.16, s * 0.16);
      p->drawPath(eye.subtracted(white).united(pupil));
      break;
    }
    case Glyph::Shuffle:{
      // Two crossing arrows
      const qreal w = s * 0.30;
      const qreal h = s * 0.15;
      QPainterPath lines;
      lines.moveTo(c.x() - w, c.y() - h);
      lines.cubicTo(c.x(), c.y() - h, c.x(), c.y() + h, c.x() + w * 0.75, c.y() + h);
      lines.moveTo(c.x() - w, c.y() + h);
      lines.cubicTo(c.x(), c.y() + h, c.x(), c.y() - h, c.x() + w * 0.75, c.y() - h);
      QPainterPath path = Stroked(lines, s * 0.07);
      path.addPath(Arrowhead(QPointF(c.x() + w + s * 0.04, c.y() - h), s * 0.16, s * 0.10));
      path.addPath(Arrowhead(QPointF(c.x() + w + s * 0.04, c.y() + h), s * 0.16, s * 0.10));
      p->drawPath(path.simplified());
      break;
    }
    case Glyph::Repeat:
    case Glyph::RepeatOne:{
      // A loop with an arrow on the top going right and one on the bottom going left
      const qreal w = s * 0.30;
      const qreal h = s * 0.16;
      QPainterPath loop;
      loop.addRoundedRect(QRectF(c.x() - w, c.y() - h, w * 2.0, h * 2.0), h, h);
      QPainterPath path = Stroked(loop, s * 0.07);
      // Gaps in the loop for the arrowheads
      QPainterPath gaps;
      gaps.addRect(QRectF(c.x() + w * 0.05, c.y() - h - s * 0.08, s * 0.10, s * 0.16));
      gaps.addRect(QRectF(c.x() - w * 0.05 - s * 0.10, c.y() + h - s * 0.08, s * 0.10, s * 0.16));
      path = path.subtracted(gaps);
      path.addPath(Arrowhead(QPointF(c.x() + w * 0.05 + s * 0.12, c.y() - h), s * 0.16, s * 0.10));
      path.addPath(Arrowhead(QPointF(c.x() - w * 0.05 - s * 0.12, c.y() + h), -s * 0.16, s * 0.10));
      if (glyph_ == Glyph::RepeatOne) {
        // A small "1" badge in the middle of the loop
        path.addRect(QRectF(c.x() - s * 0.025, c.y() - h * 0.55, s * 0.06, h * 1.1));
      }
      p->drawPath(path.simplified());
      break;
    }
    case Glyph::Artwork:{
      // A picture frame with a mountain and a sun
      const QRectF frame(c.x() - s * 0.27, c.y() - s * 0.20, s * 0.54, s * 0.40);
      QPainterPath outline;
      outline.addRect(frame);
      QPainterPath path = Stroked(outline, s * 0.06);
      QPainterPath mountain;
      mountain.moveTo(frame.left(), frame.bottom());
      mountain.lineTo(frame.left() + frame.width() * 0.38, frame.top() + frame.height() * 0.45);
      mountain.lineTo(frame.left() + frame.width() * 0.62, frame.top() + frame.height() * 0.72);
      mountain.lineTo(frame.left() + frame.width() * 0.76, frame.top() + frame.height() * 0.58);
      mountain.lineTo(frame.right(), frame.bottom());
      mountain.closeSubpath();
      path.addPath(mountain);
      path.addEllipse(QPointF(frame.left() + frame.width() * 0.75, frame.top() + frame.height() * 0.30), s * 0.05, s * 0.05);
      p->drawPath(path.simplified());
      break;
    }
    case Glyph::Speaker:{
      // A speaker cabinet, for the audio output
      const QRectF cabinet(c.x() - s * 0.17, c.y() - s * 0.26, s * 0.34, s * 0.52);
      QPainterPath outline;
      outline.addRoundedRect(cabinet, s * 0.04, s * 0.04);
      QPainterPath path = Stroked(outline, s * 0.06);
      path.addEllipse(QPointF(c.x(), cabinet.top() + cabinet.height() * 0.24), s * 0.045, s * 0.045);
      QPainterPath woofer;
      woofer.addEllipse(QPointF(c.x(), cabinet.top() + cabinet.height() * 0.64), s * 0.10, s * 0.10);
      path.addPath(Stroked(woofer, s * 0.06));
      p->drawPath(path.simplified());
      break;
    }
    case Glyph::Columns:{
      // Three columns under a header, for the browser
      const QRectF frame(c.x() - s * 0.27, c.y() - s * 0.20, s * 0.54, s * 0.40);
      QPainterPath lines;
      lines.addRect(frame);
      lines.moveTo(frame.left() + frame.width() / 3.0, frame.top());
      lines.lineTo(frame.left() + frame.width() / 3.0, frame.bottom());
      lines.moveTo(frame.left() + frame.width() * 2.0 / 3.0, frame.top());
      lines.lineTo(frame.left() + frame.width() * 2.0 / 3.0, frame.bottom());
      QPainterPath path = Stroked(lines, s * 0.06);
      path.addRect(QRectF(frame.left(), frame.top(), frame.width(), frame.height() * 0.22));
      p->drawPath(path.simplified());
      break;
    }
    case Glyph::Lyrics:{
      // A speech bubble with lines of text
      const QRectF bubble(c.x() - s * 0.27, c.y() - s * 0.22, s * 0.54, s * 0.34);
      QPainterPath outline;
      outline.addRoundedRect(bubble, s * 0.08, s * 0.08);
      QPainterPath tail;
      tail.moveTo(c.x() - s * 0.12, bubble.bottom() - 1);
      tail.lineTo(c.x() - s * 0.16, bubble.bottom() + s * 0.14);
      tail.lineTo(c.x() + s * 0.02, bubble.bottom() - 1);
      tail.closeSubpath();
      QPainterPath path = Stroked(outline, s * 0.06).united(tail);
      QPainterPath text;
      text.moveTo(bubble.left() + s * 0.11, c.y() - s * 0.09);
      text.lineTo(bubble.right() - s * 0.11, c.y() - s * 0.09);
      text.moveTo(bubble.left() + s * 0.11, c.y() + s * 0.01);
      text.lineTo(bubble.right() - s * 0.20, c.y() + s * 0.01);
      path.addPath(Stroked(text, s * 0.05));
      p->drawPath(path.simplified());
      break;
    }
    case Glyph::Equalizer:{
      // Three sliders at different levels
      const qreal spacing = s * 0.17;
      const qreal h = s * 0.25;
      const qreal levels[] = { 0.35, -0.40, 0.10 };
      QPainterPath lines;
      QPainterPath knobs;
      for (int i = 0; i < 3; ++i) {
        const qreal x = c.x() + (i - 1) * spacing;
        lines.moveTo(x, c.y() - h);
        lines.lineTo(x, c.y() + h);
        knobs.addRoundedRect(QRectF(x - s * 0.06, c.y() + levels[i] * h - s * 0.035, s * 0.12, s * 0.07), s * 0.02, s * 0.02);
      }
      QPainterPath path = Stroked(lines, s * 0.035);
      path.addPath(knobs);
      p->drawPath(path.simplified());
      break;
    }
  }

}
