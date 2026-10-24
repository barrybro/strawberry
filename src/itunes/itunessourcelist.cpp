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

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QTabBar>
#include <QVariant>

#include "widgets/fancytabwidget.h"
#include "widgets/fancytabdata.h"
#include "itunessourcelist.h"

using namespace Qt::Literals::StringLiterals;

ITunesSourceList::ITunesSourceList(FancyTabWidget *tabs, QWidget *parent)
    : QTreeWidget(parent),
      tabs_(tabs),
      rebuilding_(false) {

  setObjectName(u"itunes_source_list"_s);
  setColumnCount(1);
  setHeaderLabels(QStringList() << tr("Source"));
  header()->setSectionsClickable(false);
  setRootIsDecorated(false);
  setIndentation(4);
  setIconSize(QSize(16, 16));
  setUniformRowHeights(true);
  setFrameShape(QFrame::NoFrame);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  QObject::connect(this, &QTreeWidget::currentItemChanged, this, &ITunesSourceList::ItemSelected);
  QObject::connect(tabs_, &FancyTabWidget::CurrentTabChanged, this, &ITunesSourceList::SelectTab);
  // Tab data is set after the tab is inserted, so rebuild once the insertion is complete.
  QObject::connect(tabs_, &FancyTabWidget::TabsChanged, this, &ITunesSourceList::Rebuild, Qt::QueuedConnection);

  Rebuild();

}

void ITunesSourceList::Rebuild() {

  rebuilding_ = true;
  clear();

  for (int i = 0; i < tabs_->count(); ++i) {
    FancyTabData *data = tabs_->tabBar()->tabData(i).value<FancyTabData*>();
    if (!data) continue;
    // Use the names a 2004 jukebox would have used where they fit.
    QString label = data->label();
    if (data->name() == "collection"_L1) label = tr("Library");
    else if (data->name() == "radios"_L1) label = tr("Radio");
    QTreeWidgetItem *item = new QTreeWidgetItem(this, QStringList() << label);
    item->setIcon(0, data->icon());
    item->setData(0, Qt::UserRole, QVariant::fromValue<QWidget*>(data->page()));
  }

  rebuilding_ = false;
  SelectTab(tabs_->currentIndex());

}

void ITunesSourceList::SelectTab(const int idx) {

  QWidget *page = tabs_->widget(idx);
  for (int i = 0; i < topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = topLevelItem(i);
    if (item->data(0, Qt::UserRole).value<QWidget*>() == page) {
      if (currentItem() != item) setCurrentItem(item);
      return;
    }
  }

}

void ITunesSourceList::ItemSelected(QTreeWidgetItem *current) {

  if (rebuilding_ || !current) return;

  QWidget *page = current->data(0, Qt::UserRole).value<QWidget*>();
  const int idx = tabs_->indexOf(page);
  if (idx >= 0 && idx != tabs_->currentIndex()) {
    tabs_->setCurrentIndex(idx);
  }

}
