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


#include "../config.h"
#include "../dbmanager.h"
#include "../iconmanager.h"
#include "../mainwindow.h"
#include "../dialogs/logdialog.h"
#include "../query/queryscheduler.h"
#include "../query/querytoken.h"
#include "../widgets/resultview.h"

#include "queryeditorwidget.h"

QueryEditorWidget::QueryEditorWidget(QWidget *parent)
  : AbstractTabWidget(parent)
{
  setupUi(this);
  setupWidgets();
  setupConnections();

  model       = new QSqlQueryModel(this);
  shortModel  = new QStandardItemModel(this);
  token       = NULL;
}

QueryEditorWidget::~QueryEditorWidget()
{
  if(token)
  {
    token->disconnect();
    delete token;
  }
}

void QueryEditorWidget::acceptToken()
{
  debugText->append(tr("<b>[%1]</b>Query pending")
                    .arg(QTime::currentTime().toString()));
}

AbstractTabWidget::Actions QueryEditorWidget::availableActions()
{
  Actions ret = baseActions;
  if(!isSaved())
    ret |= Save;

  if(editor->document()->isUndoAvailable())
    ret |= Undo;
  if(editor->document()->isRedoAvailable())
    ret |= Redo;

  return ret;
}

void QueryEditorWidget::checkDbOpen()
{
  DbManager::lastIndex = dbChooser->currentIndex();

  QSqlDatabase *db = DbManager::getDatabase(dbChooser->currentIndex());
  if(db == NULL)
  {
    runButton->setEnabled(false);
    return;
  }

  runButton->setEnabled(db->isOpen());

  if(!db->isOpen())
    return;

  QMultiMap<QString, QString> fields;
  QSqlRecord r;
  QStringList tables = db->tables();
  foreach(QString t, tables)
  {
    r = db->record(t);
    for(int i=0; i<r.count(); i++)
      fields.insert(t, r.fieldName(i));
  }

  editor->reloadContext(tables, fields);
}

void QueryEditorWidget::closeEvent(QCloseEvent *event)
{
  if(isSaved())
    event->accept();
  else {
    if(confirmClose())
      event->accept();
    else
      event->ignore();
  }
}

bool QueryEditorWidget::confirmClose()
{
  // check if it's saved or not
  if(!isSaved())
  {
    int ret = QMessageBox::warning(
        this,
        tr( "DbMaster" ),
        tr( "Warning ! All your modifications will be lost.\nDo you want to save ?" ),
        QMessageBox::Cancel | QMessageBox::Save | QMessageBox::Discard,
        QMessageBox::Cancel);

    switch(ret)
    {
    case QMessageBox::Cancel:
      return false;

    case QMessageBox::Save:
      return save();
    }
  }

  return true;
}

int QueryEditorWidget::confirmCloseAll()
{
  if(isSaved())
    return QMessageBox::Yes;

  QString msg(tr("Warning ! All your modifications will be lost.\nDo you want to save ?"));
  if(filePath.isNull())
    msg = QString(tr("Warning ! %1 hasn't been saved\nDo you want to save ?"))
                  .arg(filePath);

  int ret = QMessageBox::warning(this, tr("DbMaster"), msg,
                                 QMessageBox::Save    | QMessageBox::SaveAll  |
                                 QMessageBox::No      | QMessageBox::NoAll    |
                                 QMessageBox::Cancel,
                                 QMessageBox::Cancel);

  return ret;
}

void QueryEditorWidget::copy()
{
  editor->copy();
}

void QueryEditorWidget::cut()
{
  editor->cut();
}

QString QueryEditorWidget::file()
{
  return filePath;
}

/**
 * Forwards from the editor
 */
void QueryEditorWidget::forwardChanged(bool changed)
{
  emit modificationChanged(changed);
}

QIcon QueryEditorWidget::icon()
{
  return IconManager::get("accessories-text-editor");
}

QString QueryEditorWidget::id()
{
  QString ret = "q";
  if( !filePath.isEmpty() )
    ret.append( " %1" ).arg( filePath );
  return ret;
}

bool QueryEditorWidget::isSaved()
{
  return !editor->document()->isModified();
}

void QueryEditorWidget::open(QString path)
{
  if(!confirmClose())
    return;

  setFilePath(path);
  reloadFile();
}

void QueryEditorWidget::paste()
{
	editor->paste();
}

QueryToken *QueryEditorWidget::prepareToken()
{
  QString qtext = editor->textCursor().selectedText();
  if( qtext.isEmpty() )
  qtext = editor->toPlainText();

  QueryToken *token =
      new QueryToken(qtext,
                     DbManager::getDatabase(dbChooser->currentIndex()),
                     this);

  connect(token, SIGNAL(accepted()), this, SLOT(acceptToken()));
  connect(token, SIGNAL(finished(QSqlError)),
          this, SLOT(validateToken(QSqlError)));
  connect(token, SIGNAL(rejected()), this, SLOT(rejectToken()));
  connect(token, SIGNAL(started()), this, SLOT(startToken()));
  return token;
}

void QueryEditorWidget::print()
{
  QPainter painter;
  painter.begin(&m_printer);
  editor->document()->drawContents(&painter);
  painter.end();
}

QPrinter *QueryEditorWidget::printer()
{
  return &m_printer;
}

void QueryEditorWidget::redo()
{
  editor->redo();
}

void QueryEditorWidget::refresh()
{
  checkDbOpen();
}

void QueryEditorWidget::rejectToken()
{

}

void QueryEditorWidget::reload()
{
  run();
}

/**
 * Called after open(QString)
 */
void QueryEditorWidget::reloadFile()
{
  if(!confirmClose())
    return;

  editor->clear();

  // loading the file
  QFile file(filePath);
  if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    // OMG ! an error occured
    QMessageBox::critical(this, tr("DbMaster"),
                          tr("Unable to open the file !"),
                          QMessageBox::Ok );
    return;
  }

  if(Config::editorEncoding == "latin1")
    editor->append(QString::fromLatin1(file.readAll().data()));

  if(Config::editorEncoding == "utf8")
    editor->append(QString::fromUtf8(file.readAll().data()));

  file.close();

  editor->document()->setModified(false);

  emit fileChanged(filePath);
}

void QueryEditorWidget::run()
{
  tabWidget->setVisible(true);
  tabWidget->setTabEnabled(1, false);
  tabWidget->setCurrentIndex(0);
  debugText->clear();
  runButton->setEnabled(false);

  statusBar->showMessage(tr("Running..."));

  oldToken = token;
  token = prepareToken();

  QueryScheduler::push(token);
}

/**
 * @returns false in case of error
 */
bool QueryEditorWidget::save()
{
  if(isSaved())
    return true;

  if(filePath.isNull())
  {
    setFilePath(QFileDialog::getSaveFileName(
        this, tr("DbMaster"), lastDir.path(),
        tr("Query file (*.sql);;All files (*.*)")));

    if(filePath.isNull())
      return false;
  }

  QFile file(filePath);
  lastDir = filePath;

  if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    QMessageBox::critical(this, tr("DbMaster"), tr("Unable to save the file !"),
                          QMessageBox::Ok);
    return false;
  }

  editor->setEnabled( false );
  setCursor(Qt::BusyCursor);

  QTextStream out( &file );
  QStringList list( editor->toPlainText().split( '\n' ) );

  int count = list.count();
  for( int i = 0; i < count; i++ )
  {
    out << list[i];
    if( i != count )
    out << '\n';
  }

  file.close();

  editor->document()->setModified(false);

  editor->setEnabled(true);
  setCursor(Qt::ArrowCursor);

  emit modificationChanged(editor->document()->isModified());
  return true;
}

void QueryEditorWidget::saveAs( QString name )
{
  filePath = name;
  save();
}

void QueryEditorWidget::selectAll()
{
  editor->selectAll();
}

void QueryEditorWidget::setFilePath( QString path )
{
  this->filePath = path;

  if(path.size() > 30)
    path = QFileInfo(path).fileName();

  pathLabel->setText(path);
}

void QueryEditorWidget::setupConnections()
{
  connect(dbChooser, SIGNAL(currentIndexChanged(int)),
          this, SLOT(checkDbOpen()));
  connect(tabView, SIGNAL(reloadRequested()), this, SLOT(reload()));

  connect(runButton, SIGNAL(clicked()), this, SLOT(run()));

  connect(editor->document(), SIGNAL(modificationChanged(bool)),
          this, SLOT(forwardChanged(bool)));
}

void QueryEditorWidget::setupWidgets()
{
  editor->setFont(Config::editorFont);

  tabWidget->setVisible(false);

  statusBar = new QStatusBar(this);
  statusBar->setSizeGripEnabled(false);
  gridLayout->addWidget(statusBar, gridLayout->rowCount(), 0, 1, -1);

  statusLabel = new QLabel(this);
  statusBar->addWidget(statusLabel, 200);

  optionsMenu = new QMenu(this);
  // optionsMenu->addAction(actionThreaded_query);
  optionsMenu->addAction(actionLogQuery);
  optionsMenu->addAction(actionClearOnSuccess);
  toolButton->setMenu(optionsMenu);

  baseActions = Copy | Cut | Paste | Print | SaveAs | Search | SelectAll;

  dbChooser->setModel(DbManager::model());
  dbChooser->setCurrentIndex(DbManager::lastIndex);

  runButton->setIcon(IconManager::get("media-playback-start"));

  refresh();
}

void QueryEditorWidget::startToken()
{
}

QString QueryEditorWidget::title()
{
  QString t;
  if(!filePath.isEmpty())
    t = QFileInfo(filePath).fileName();
  else
    t = tr("Query editor");

  return t;
}

QTextEdit* QueryEditorWidget::textEdit()
{
  return editor;
}

void QueryEditorWidget::undo()
{
  editor->undo();
}

void QueryEditorWidget::validateToken(QSqlError err)
{
  QString                 logMsg;
  QMap<QString, QVariant> logData;
  tabWidget->setTabEnabled(1, false);
  switch(err.type())
  {
  case QSqlError::NoError:
    tabView->setToken(token);
    tabWidget->setCurrentIndex(1);
    tabWidget->setTabEnabled(1, true);

    if(actionClearOnSuccess->isChecked())
      editor->clear();

    if(oldToken)
    {
      oldToken->disconnect();
      delete oldToken;
    }

    statusBar->showMessage(
        tr("Query executed with success in %2secs (%1 lines returned)")
        .arg(token->model()->rowCount())
        .arg(token->duration()));

    logMsg = tr("Query executed with success");
    logData["query"] = token->query();
    LogDialog::instance()->append(logMsg, logData);

    debugText->append(QString("<b>[%1]</b>%2")
                      .arg(QTime::currentTime().toString())
                      .arg(tr("Query executed with success in %2secs (%1 lines returned)")
                           .arg(token->model()->rowCount())
                           .arg(token->duration())));
    break;

  default:
    statusBar->showMessage(tr("Unable to run query"));

    debugText->append(QString("<b>[%1]</b>%2")
                      .arg(QTime::currentTime().toString())
                      .arg(err.text()));
    break;
  }

  runButton->setEnabled(true);
}
