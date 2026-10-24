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
#include <QPaintEvent>

#include "itunesbutton.h"

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
    return QSize(diameter_ + diameter_ / 3, diameter_);
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
  const bool sunken = isDown() || isChecked();

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
  const QRectF glyph_rect = QRectF(inner.center() - QPointF(inner.height() / 2.0, inner.height() / 2.0), QSizeF(inner.height(), inner.height()));
  p.setBrush(QColor(255, 255, 255, 170));
  DrawGlyph(&p, glyph_rect.translated(0, 1));
  p.setBrush(isEnabled() ? QColor(40, 40, 40) : QColor(140, 140, 140));
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
  }

}
