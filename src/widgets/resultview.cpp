/*
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 */

#include "resultview.h"

#include "../iconmanager.h"

ResultView::ResultView(QWidget *parent)
  : QWidget(parent)
{
  setupUi(this);
  table->setModel(0);
  currentAction = Browse;
  offset = 0;
  m_mode = QueryMode;
  shortModel = new QStandardItemModel(this);
  table->setModel(shortModel);

  setupMenus();
  setupConnections();

  // loading icons from theme
  firstPageButton->setIcon(IconManager::get("go-first"));
  lastPageButton->setIcon(IconManager::get("go-last"));
  nextPageButton->setIcon(IconManager::get("go-next"));
  prevPageButton->setIcon(IconManager::get("go-previous"));
  reloadButton->setIcon(IconManager::get("view-refresh"));
  insertButton->setIcon(IconManager::get("list-add"));
  deleteButton->setIcon(IconManager::get("list-remove"));
}

void ResultView::contextMenuEvent(QContextMenuEvent *e)
{
  if(e->reason() != QContextMenuEvent::Mouse
     || !table->geometry().contains(e->pos())
     || model == 0)
    return;

  if( model->columnCount() == 0 || model->rowCount() == 0 )
    return;

  actionExport->setEnabled(m_mode == QueryMode);

  contextMenu->move(e->globalPos());
  contextMenu->exec();
}

void ResultView::exportContent()
{
  if( model == 0 )
    return;

  if( model->columnCount() == 0 || model->rowCount() == 0 )
    return;

//  exportWizard = new ExportWizard(model, this);
  exportWizard = new ExportWizard(m_token, this);
  exportWizard->exec();
}

/**
 * Clic sur le bouton supprimer : peut annuler l'opération en cours ou supprimer
 * la ligne en cours.
 */
void ResultView::on_deleteButton_clicked()
{
  if(m_mode != TableMode)
    return;

  QSqlTableModel *tmodel = (QSqlTableModel*) model;

  if (currentAction != Browse) {
    // Annulation de l'opération en cours
    insertButton->setIcon(IconManager::get("list-add"));
    deleteButton->setIcon(IconManager::get("list-remove"));

    currentAction = Browse;
    tmodel->revertAll();
  } else {
    // suppression des lignes sélectionnées
    QSet<int> rows;
    foreach(QModelIndex i, table->selectionModel()->selectedIndexes())
      rows << (i.row() + offset);

    foreach(int r, rows)
      model->removeRow(r);

    tmodel->submitAll();
  }

  updateView();
}

/**
 * Clic sur le bouton d'insertion : peut passer en mode insertion ou valider un
 * ajout/modif.
 */
void ResultView::on_insertButton_clicked()
{
  if(m_mode != TableMode)
    return;

  if (currentAction == Insert || currentAction == Update) {
    insertButton->setIcon(IconManager::get("list-add"));
    deleteButton->setIcon(IconManager::get("list-remove"));

    QSqlTableModel *tmodel = (QSqlTableModel*) model;

    foreach (int row, modifiedRecords.keys()) {
      tmodel->setRecord(row, modifiedRecords[row]);
    }

    if (!tmodel->submitAll()) {
      QMessageBox::critical(this, "Error", tmodel->lastError().text());
    }

    currentAction = Browse;
    tmodel->select();
    updateView();
  } else {
    insertButton->setIcon(QIcon());
    insertButton->setText(tr("Apply"));
    deleteButton->setIcon(QIcon());
    deleteButton->setText(tr("Cancel"));

    model->insertRow(model->rowCount());
    currentAction = Insert;
    updateView();
    table->scrollToBottom();
  }
}

void ResultView::on_reloadButton_clicked()
{
  switch (m_mode) {
  case QueryMode:
    emit reloadRequested();
    break;

  case TableMode:
    ((QSqlTableModel*) model)->select();
    updateView();
    break;
  }
}

void ResultView::resizeColumnsToContents()
{
  table->resizeColumnsToContents();
}

void ResultView::resizeRowsToContents()
{
  table->resizeRowsToContents();
}

void ResultView::scrollBegin()
{
  offset = 0;
  updateView();
}

void ResultView::scrollDown()
{
  offset += resultSpinBox->value();
  updateView();
}

void ResultView::scrollEnd()
{
  double last;
  modf(model->rowCount() / resultSpinBox->value(), &last);

  offset = last * resultSpinBox->value();
  updateView();
}

void ResultView::scrollUp()
{
  offset -= resultSpinBox->value();
  updateView();
}

void ResultView::setAlternatingRowColors(bool enable)
{
  table->setAlternatingRowColors(enable);
}

void ResultView::setMode(Mode m)
{
  m_mode = m;

  switch(m_mode)
  {
  case QueryMode:
    insertButton->setVisible(false);
    deleteButton->setVisible(false);
    break;

  case TableMode:
    insertButton->setVisible(true);
    deleteButton->setVisible(true);
    break;
  }
}

void ResultView::setModel(QSqlQueryModel *model)
{
  this->model = model;
  updateView();
}

void ResultView::setRowsPerPage(int rowPP)
{
  resultSpinBox->setValue(rowPP);
}

void ResultView::setTable(QString table, QSqlDatabase *db)
{
  setMode(TableMode);
  QSqlTableModel *m = new QSqlTableModel(this, *db);
  m->setTable(table);
  m->setEditStrategy(QSqlTableModel::OnManualSubmit);
  m->select();
  if(m->lastError().type() == QSqlError::NoError)
  {
    setModel(m);
  } else {
    QMessageBox::critical(this,
                          tr("Error"),
                          tr("The specified table doesn't exist"),
                          QMessageBox::Ok);
  }
}

void ResultView::setToken(QueryToken *token)
{
  setMode(QueryMode);
  m_token = token;
  offset = 0;

  setModel(m_token->model());
}

void ResultView::setupConnections()
{
  connect(actionAlternateColor, SIGNAL(toggled(bool)),
          this, SLOT(setAlternatingRowColors(bool)));
  connect(actionExport, SIGNAL(triggered()), this, SLOT(exportContent()));

  connect(firstPageButton, SIGNAL(clicked()), this, SLOT(scrollBegin()));
  connect(prevPageButton, SIGNAL(clicked()), this, SLOT(scrollUp()));
  connect(nextPageButton, SIGNAL(clicked()), this, SLOT(scrollDown()));
  connect(lastPageButton, SIGNAL(clicked()), this, SLOT(scrollEnd()));

  connect(reloadButton, SIGNAL(clicked()), this, SLOT(on_reloadButton_clicked()));
  connect(resultSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateView()));

  connect(deleteButton, SIGNAL(clicked()), this, SLOT(on_deleteButton_clicked()));

  connect(shortModel, SIGNAL(itemChanged(QStandardItem*)),
          this, SLOT(updateItem(QStandardItem*)));
}

void ResultView::setupMenus()
{
  contextMenu = new QMenu(this);

  actionAlternateColor = new QAction(tr("Alternate row colors"), this);
  actionAlternateColor->setCheckable(true);
  actionAlternateColor->setChecked(false);
  contextMenu->addAction(actionAlternateColor);

  actionExport = new QAction(tr("Export"), this);
  actionExport->setIcon(IconManager::get("document-save-as"));
  actionExport->setShortcut(QKeySequence("Ctrl+E"));
  contextMenu->addAction(actionExport);
}

/**
 * Called by the shortmodel's signal dataChanged in order to forward it to the
 * real one.
 */
void ResultView::updateItem(QStandardItem *item)
{
  if (currentAction == Browse) {
    currentAction = Update;
    insertButton->setText(tr("Apply"));
    deleteButton->setText(tr("Cancel"));
  }

  QSqlRecord record;
  int row = item->row() + offset;
  if (modifiedRecords.contains(row)) {
    record = modifiedRecords[row];
  } else {
    record = model->record(row);
  }
  record.setValue(item->column(), item->data(Qt::DisplayRole));
  modifiedRecords[row] = record;
}

void ResultView::updateView()
{
  shortModel->clear();

  if(model->rowCount() == 0)
    return;

  int startIndex;
  startIndex = offset;
  if(startIndex > model->rowCount())
    startIndex = model->rowCount() - resultSpinBox->value();
  if(startIndex < 0)
    startIndex = 0;

  double page, maxpage;
  modf(startIndex / resultSpinBox->value(), &page);
  modf(model->rowCount() / resultSpinBox->value(), &maxpage);
  pageLabel->setText(tr("Page %1/%2")
                     .arg(page+1)
                     .arg(maxpage+1));

  int endIndex;
  endIndex = startIndex + resultSpinBox->value();
  if(endIndex > model->rowCount())
    endIndex = model->rowCount();

  firstPageButton->setEnabled(startIndex > 0);
  prevPageButton->setEnabled(startIndex > 0);
  nextPageButton->setEnabled(endIndex < model->rowCount());
  lastPageButton->setEnabled(endIndex < model->rowCount());

  for(int i=0; i<model->columnCount(); i++)
    shortModel->setHorizontalHeaderItem(i, new QStandardItem(
        model->headerData(i, Qt::Horizontal).toString()));

  QStandardItem *item;
  QStringList vlabels;
  QSqlRecord r;
  QList<QStandardItem*> row;
  for(int i=startIndex; i<endIndex; i++)
  {
    row.clear();
    r = model->record(i);
    for(int j=0; j<model->columnCount(); j++)
    {
      item = new QStandardItem();
      item->setData( r.value( j ), Qt::DisplayRole );
      row << item;
    }
    shortModel->appendRow(row);
    vlabels << QString::number(i+1);
  }
  if(currentAction == Insert)
  {
    vlabels.removeLast();
    vlabels << "*";
  }
  shortModel->setVerticalHeaderLabels(vlabels);

  resizeColumnsToContents();
  resizeRowsToContents();
}
