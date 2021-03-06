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


#ifndef CONFIG_H
#define CONFIG_H

#include <QFont>
#include <QMap>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QStringListIterator>
#include <QTextCharFormat>

class Config
{
public:
  static void init();
  static void reload();
  static void save();

  static int                           compCharCount;
  static QString                       defaultDriver;
  static QFont                         editorFont;
  static bool                          editorAutoSave;
  static QString                       editorEncoding;
  static bool                          editorSemantic;
  static QMap<QString,QColor>          shColor;
  static QMap<QString,QTextCharFormat> shFormat;
  static QStringList                   shGroupList;
};

#endif // CONFIG_H
