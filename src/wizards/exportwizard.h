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


#ifndef EXPORTWIZARD_H
#define EXPORTWIZARD_H

#include <QtGui>

#include "../query/querytoken.h"

#include "ui_exportwizard.h"
#include "ui_ew_firstpage.h"
#include "ui_ew_csvpage.h"
#include "ui_ew_htmlpage.h"
#include "ui_ew_sqlpage.h"
#include "ui_ew_exportpage.h"

class ExportWizard : public QWizard, Ui::ExportWizard
{
Q_OBJECT
public:
  enum Pages {
    FirstPage,
    CsvPage,
    HtmlPage,
    SqlPage,
    ExportPage
  };

  enum Format {
    CsvFormat   = 0,
    HtmlFormat  = 1,
    SqlFormat   = 2
  };

  ExportWizard(QueryToken *token, QWidget *parent =0);

  QueryToken *token;
};


class EwFirstPage : public QWizardPage, Ui::EwFirstPage
{
Q_OBJECT
public:
  EwFirstPage(QWizard *parent=0);

  int nextId() const;

public slots:
  void browse();

private:
  ExportWizard::Format selectedFormat();
  QMap<QRadioButton*, ExportWizard::Format> formatMap;

  static QString lastPath;
};


class EwCsvPage : public QWizardPage, Ui::EwCsvPage
{
Q_OBJECT
public:
  EwCsvPage(QWizard *parent=0);
  int nextId() const;
};

class EwHtmlPage : public QWizardPage, Ui::EwHtmlPage
{
Q_OBJECT
public:
  EwHtmlPage(QWizard *parent=0);
  void initializePage();
  int nextId() const;
};


class EwSqlPage : public QWizardPage, Ui::EwSqlPage
{
Q_OBJECT
public:
  EwSqlPage(QWizard *parent=0);
  int nextId() const;
};


class EwExportPage : public QWizardPage, Ui::EwExportPage, QRunnable
{
Q_OBJECT
public:
  EwExportPage(QueryToken *token, QWizard *parent =0);
  //bool isComplete();
  void initializePage();

signals:
  void progress(int);

protected:
  void run();

private slots:
  void checkProgress();

private:
  QProgressDialog    *dial;
  bool                finished;
  QueryToken         *token;
};

#endif // EXPORTWIZARD_H
