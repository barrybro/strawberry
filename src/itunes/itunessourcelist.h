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

class FancyTabWidget;

// The "Source" list on the left.
// It mirrors the tabs of the sidebar tab widget, whose own tab bar is hidden, and switches between them.
class ITunesSourceList : public QTreeWidget {
  Q_OBJECT

 public:
  explicit ITunesSourceList(FancyTabWidget *tabs, QWidget *parent = nullptr);

 private Q_SLOTS:
  void Rebuild();
  void SelectTab(const int idx);
  void ItemSelected(QTreeWidgetItem *current);

 private:
  FancyTabWidget *tabs_;
  bool rebuilding_;
};

#endif  // ITUNESSOURCELIST_H
