/* 
 * The Biomechanical ToolKit
 * Copyright (c) 2009-2012, Arnaud Barré
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *     * Redistributions of source code must retain the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *     * Neither the name(s) of the copyright holders nor the names
 *       of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written
 *       permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
 
#include "UpdateManager_p.h"
#include "btk_ioapi_qiodevice.h"

#include <QXmlStreamReader>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QTime>
#include <QSettings>

UpdateApplicationDownload::UpdateApplicationDownload()
: url(), hash(), data(), buffer()
{
  this->unz = NULL;
}

UpdateApplicationDownload::~UpdateApplicationDownload()
{
  // Try to close the unzip file
  if (this->unz != NULL)
    unzClose(this->unz);
}

// ------------------------------------------------------------------------- //

UpdateController::UpdateController(const QString& appVer, const QString& feedUrl)
: QObject(NULL), m_CurrentVersion(appVer.split(".")), m_FeedUrl(feedUrl), 
  m_Download()
{
  this->mp_Manager = 0;
};

void UpdateController::setFeedUrl(const QString& url)
{
  this->m_FeedUrl = url;
};

void UpdateController::setApplicationDownloadUrl(const QString& url)
{
  // this->m_ApplicationDownloadUrl = url;
  this->m_Download.url = url;
};

void UpdateController::checkUpdate()
{
  this->mp_Manager = new QNetworkAccessManager(this);
  connect(this->mp_Manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(parseFeed(QNetworkReply*)));
  this->mp_Manager->get(QNetworkRequest(QUrl(this->m_FeedUrl)));
};

void UpdateController::parseFeed(QNetworkReply* reply)
{
  bool updateAvailable = false, updateError = true, compatiblePackageFound = false;  
  QString appName, appNewVer, appLatestNewVer, appNote, appPubDate;
  if ((reply->error() == QNetworkReply::NoError) && reply->open(QIODevice::ReadOnly))
  {
    QXmlStreamReader xmlReader(reply);
    if (xmlReader.readNextStartElement())
    {
      appLatestNewVer = this->m_CurrentVersion.join(".");
      if ((xmlReader.name() == "UpdateChecker") && (xmlReader.attributes().value("version") == "1.0"))
      {
        appName = xmlReader.attributes().value("name").toString();
        updateError = false;
        while (xmlReader.readNextStartElement())
        {
          if (xmlReader.name() == "Release")
          {
            appPubDate = xmlReader.attributes().value("pubdate").toString();
            appNewVer = xmlReader.attributes().value("version").toString();
            if (this->isNewRelease(appNewVer.split(".")))
            {
              QString appNote_;
              appNote_ += "<div style=\"margin-top:15px;\">";
              appNote_ += "<div style=\"background-color:#A4DDED;\"><b>&nbsp;" + appPubDate + " - " + appName + " " + appNewVer + " released</b><div>";
              appNote_ += "<div><ul style=\"margin:5 0 0 -25;\">";
              while (xmlReader.readNextStartElement())
              {
                if (xmlReader.name() == "Enhancement")
                {
                  appNote_ += "<li><b>Enhancements</b></li><ul style=\"margin:5 0 5 -25;\">";
                  while (xmlReader.readNextStartElement())
                  {
                    if (xmlReader.name() == "item")
                      appNote_ += "<li>" + xmlReader.readElementText() + "</li>";
                  }
                  appNote_ += "</ul>";
                }
                else if (xmlReader.name() == "Update")
                {
                  appNote_ += "<li><b>Updates</b></li><ul style=\"margin:5 0 5 -25;\">";
                  while (xmlReader.readNextStartElement())
                  {
                    if (xmlReader.name() == "item")
                      appNote_ += "<li>" + xmlReader.readElementText() + "</li>";
                  }
                  appNote_ += "</ul>";
                }
                else if (xmlReader.name() == "Fix")
                {
                  appNote_ += "<li><b>Fixes</b></li><ul style=\"margin:5 0 5 -25;\">";
                  while (xmlReader.readNextStartElement())
                  {
                    if (xmlReader.name() == "item")
                      appNote_ += "<li>" + xmlReader.readElementText() + "</li>";
                  }
                  appNote_ += "</ul>";
                }
                else if (xmlReader.name() == "Binary")
                {
                  int latestCompatibleOS = -1;
                  while (xmlReader.readNextStartElement())
                  {
                    if (xmlReader.name() == "item")
                    {
                      QString os = xmlReader.attributes().value("os").toString();
                      QString minver = xmlReader.attributes().value("minver").toString();
                      QString arch = xmlReader.attributes().value("arch").toString();
                      QString url = xmlReader.attributes().value("url").toString();
                      QString hash = xmlReader.attributes().value("hash").toString();
                      int osRequired = 9999;
#if defined(Q_OS_MAC)
                      const int osVersion = static_cast<int>(QSysInfo::MacintoshVersion);
                      if (os.compare("mac") == 0)
                      {
                        if (minver.compare("10.5", Qt::CaseInsensitive) == 0)
                          osRequired = static_cast<int>(QSysInfo::MV_10_5);
                        else if (minver.compare("10.6", Qt::CaseInsensitive) == 0)
                          osRequired = static_cast<int>(QSysInfo::MV_10_6);
  #if QT_VERSION >= 0x040800
                        else if (minver.compare("10.7", Qt::CaseInsensitive) == 0)
                          osRequired = static_cast<int>(QSysInfo::MV_10_7);
  #elif QT_VERSION >= 0x050000
                        else if (minver.compare("10.8", Qt::CaseInsensitive) == 0)
                          osRequired = static_cast<int>(QSysInfo::MV_10_8);
  #endif
#elif defined(Q_OS_WIN)
                      const int osVersion = static_cast<int>(QSysInfo::WindowsVersion);
                      if (os.compare("win"))
                      {
                        if (minver.compare("WinXP", Qt::CaseInsensitive) == 0)
                          osRequired = static_cast<int>(QSysInfo::WV_XP);
                        else if (minver.compare("WinVista", Qt::CaseInsensitive) == 0)
                          osRequired = static_cast<int>(QSysInfo::WV_VISTA);
                        else if (minver.compare("Win7", Qt::CaseInsensitive) == 0)
                          osRequired = static_cast<int>(QSysInfo::WV_WINDOWS7);
#else
                      if (0)
                      {
#endif
                        int archRequired = (arch.compare("x86",  Qt::CaseInsensitive) == 0 ? 32 : 64);
                        if ((QSysInfo::WordSize == archRequired) && (osVersion >= osRequired))
                        {
                          compatiblePackageFound = true;
                          if (osRequired > latestCompatibleOS)
                          {
                            latestCompatibleOS = osRequired;
                            this->m_Download.url = url;
                            this->m_Download.hash = hash;
                          }
                        }
                      }
                      xmlReader.readNext();
                    }
                  }
                }
                xmlReader.readNext();
              }
              appNote_ += "</ul></div>";
              appNote_ += "</div>";
              
              // Check if this new release is supported or not by the current OS
              if (compatiblePackageFound)
              {
                appNote += appNote_;
                updateAvailable = true;
                if (this->isGreaterRelease(appNewVer.split("."), appLatestNewVer.split(".")))
                  appLatestNewVer = appNewVer;
              }
            }
            else
              xmlReader.skipCurrentElement();
          }
          else
            xmlReader.skipCurrentElement();
        }
      }
    }
    reply->close();
  }
  if (updateError)
    emit updateIncomplete();
  else if (updateAvailable)
    emit updateFound(appName, this->m_CurrentVersion.join("."), appLatestNewVer, appNote);
  else
    emit updateNotFound(appName, this->m_CurrentVersion.join("."));
  
  emit parsingFinished();
  
  reply->deleteLater();
  reply->manager()->deleteLater();
};

void UpdateController::downloadUpdate()
{
  this->mp_Manager = new QNetworkAccessManager(this);
  connect(this->mp_Manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(checkDownload(QNetworkReply*)));
  this->mp_DownloadReply = this->mp_Manager->get(QNetworkRequest(QUrl(this->m_Download.url)));
  connect(this->mp_DownloadReply, SIGNAL(downloadProgress(qint64, qint64)), this, SIGNAL(downloadProgress(qint64, qint64)));
};

void UpdateController::abortDownload()
{
  this->mp_DownloadReply->abort();
  this->mp_DownloadReply->deleteLater();
  this->mp_DownloadReply->manager()->deleteLater();
};

const uLong maxFilenameLength = 1024;

void UpdateController::checkDownload(QNetworkReply* reply)
{
  
  bool incompleteDownload = true;
  if ((reply->error() == QNetworkReply::NoError) && reply->isReadable())
  {
    // There is no choice to use a QBuffer as a QNetworkReply is a sequential QIODevice.
    // See http://bugreports.qt-project.org/browse/QTBUG-10622
    this->m_Download.data = reply->readAll();
    QByteArray hash = QCryptographicHash::hash(this->m_Download.data, QCryptographicHash::Sha1);
    if (QString(hash.toHex()).compare(this->m_Download.hash) == 0)
    {
      this->m_Download.buffer.setBuffer(&(this->m_Download.data));
      const char* fakeUnzipFilename = reinterpret_cast<const char*>(&(this->m_Download.buffer));
      zlib_filefunc_def pzlib_filefunc_def;
      fill_qiodevice_open_filefunc(&pzlib_filefunc_def);
      this->m_Download.unz = unzOpen2(fakeUnzipFilename, &pzlib_filefunc_def);
      if (this->m_Download.unz != NULL)
      {
        // Check if it is possible to go through all the files in the archive.
        if (unzGoToFirstFile(this->m_Download.unz) == UNZ_OK)
        {
          incompleteDownload = false;
          int res = 0;
          do
          {
            char filename[maxFilenameLength] = "\0";
            if (unzGetCurrentFileInfo(this->m_Download.unz, NULL, filename, maxFilenameLength, NULL, 0, NULL, 0) != UNZ_OK)
            {
              qDebug("Error when retrieving file information during files extraction.");
              break;
            }
          }
          while ((res = unzGoToNextFile(this->m_Download.unz)) != UNZ_END_OF_LIST_OF_FILE);
          if ((res == UNZ_END_OF_LIST_OF_FILE) && (unzGoToFirstFile(this->m_Download.unz) == UNZ_OK))
          {
            incompleteDownload = false;
            emit downloadReadyToInstall();
          }
          else
            qDebug("Impossible to extract all the files from the downloaded content.");
        }
        else
          qDebug("Impossible to go to the first compressed file.");
      }
      else
        qDebug("Impossible to uncompress the downloaded content.");
    }
    else
      qDebug("Incorrect checksum for the downloaded content.");
  }
  if (incompleteDownload)
    emit downloadIncomplete();
  
  emit downloadFinished();
  
  reply->deleteLater();
  reply->manager()->deleteLater();
};

void UpdateController::installUpdate()
{
  bool incompleteInstallation = true;
  QString appPath = QCoreApplication::applicationDirPath();
#ifdef Q_OS_MAC
  appPath += "/../.."; // Go back to the Application.app folder
#endif
  appPath = "/Applications/Mokka.app/Contents/MacOS/../..";
  QDir appDir(appPath);
  QTime time = QTime::currentTime();
  QString moveSuffix = "." + QString::number(time.second() * 1000 + time.msec());
  QStringList suffixToBeMoved;
#if defined(Q_OS_MAC)
  suffixToBeMoved.append(""); // executable or framework
  suffixToBeMoved.append("dylib"); // shared library
  suffixToBeMoved.append("so"); // shared library
#elif defined(Q_OS_WIN)
  suffixToBeMoved.append("exe"); // executable
  suffixToBeMoved.append("dll"); // shared library
  suffixToBeMoved.append("pyd"); // Python dynamic library
#elif defined(Q_OS_LINUX)
  suffixToBeMoved.append(""); // executable
  suffixToBeMoved.append("so"); // shared library
#else
  #warning "Only MacOS X / Windows / Linux are supported for the moment."
#endif
  QStringList filesMoved;
  // Move the files which need to be updated and install the new ones
  int res = 0;
  do
  {
    // Extract the filename 
    char name[maxFilenameLength] = "\0";
    unz_file_info unzFileIinfo; 
    if (unzGetCurrentFileInfo(this->m_Download.unz, &unzFileIinfo, name, maxFilenameLength, NULL, 0, NULL, 0) != UNZ_OK)
    {
      qDebug("Error when retrieving file information during files extraction.");
      break;
    }
    
    // Special check for the MacOS X archives
    QString relativeFilename(name);
    if (relativeFilename.startsWith("__MACOSX"))
      continue;
    
    // Open the current compressed files
    if (unzOpenCurrentFile(this->m_Download.unz) != UNZ_OK)
    {
      qDebug("Impossible to open at least one compressed file during the update process.");
      break;
    }
    int idx = relativeFilename.indexOf("/");
    if (idx == -1)
      relativeFilename = "";
    else
      relativeFilename = relativeFilename.mid(idx);
    QFileInfo fI(appPath + "/" + relativeFilename);
    QString absFilePath = fI.absoluteFilePath();
    unzFileIinfo.external_fa = unzFileIinfo.external_fa >> 16; // At least under MacOS X
    printf("%s\n", qPrintable(absFilePath));
    if (!fI.exists() && S_ISDIR(unzFileIinfo.external_fa))
    {
      if (!appDir.mkpath(fI.absolutePath()))
      {
        qDebug("Impossible to create at least one subdirectory during the update process.");
        break;
      }
    }
    /*
    // TODO: Need to support the symlink
    else if (S_ISLNK(unzFileIinfo.external_fa))
    {
    }
    */
    else if (!fI.isDir())
    {
      // If the file already exists and has a suffix in the list 'suffixToBeMoved', then it is renamed with a new suffix
      if (fI.exists())
      {
        bool needToBeMoved = false;
        QString suffix = fI.completeSuffix();
        if (suffix.isEmpty())
        {
          if (suffixToBeMoved.contains(""))
            needToBeMoved = true;
        }
        else
        {
          for (QStringList::const_iterator itS = suffixToBeMoved.begin() ; itS != suffixToBeMoved.end() ; ++itS)
          {
            if (suffix.startsWith(*itS))
            {
              needToBeMoved = true;
              break;
            }
          }
        }
        if (needToBeMoved)
        {
          if (!appDir.rename(absFilePath, absFilePath + moveSuffix))
          {
            qDebug("Impossible to move at least one file during the update process.");
            break;
          }
          else
          {
            filesMoved.append(absFilePath + moveSuffix);
            printf(" -> Moved!\n");
          }
        }
      }
      // Create the file and feed it
      QFile file(absFilePath);
      bool unzipRead = false;
      if (file.open(QIODevice::WriteOnly))
      {
        // Extract the content of the file
        char buffer[4096] = {0};
        int bytes = 0;
        for(;;)
        {
          bytes = unzReadCurrentFile(this->m_Download.unz, buffer, sizeof(buffer));
          if (bytes == 0)
          {
            printf(" -> Updated!\n");
            unzipRead = true;
            break;
          }
          else if ((bytes < 0) || ((int)file.write(buffer, bytes) != bytes))
          {
            qDebug("Impossible to update the content of one file during the update process.");
            break;
          }
        }
        // Set the permission of the file.
        QFile::Permissions perms;
        if (unzFileIinfo.external_fa & 0400) perms |= (QFile::ReadOwner);
        if (unzFileIinfo.external_fa & 0200) perms |= (QFile::WriteOwner);
        if (unzFileIinfo.external_fa & 0100) perms |= (QFile::ExeOwner);
        if (unzFileIinfo.external_fa & 0400) perms |= (QFile::ReadUser);
        if (unzFileIinfo.external_fa & 0200) perms |= (QFile::WriteUser);
        if (unzFileIinfo.external_fa & 0100) perms |= (QFile::ExeUser);
        if (unzFileIinfo.external_fa & 0040) perms |= (QFile::ReadGroup);
        if (unzFileIinfo.external_fa & 0020) perms |= (QFile::WriteGroup);
        if (unzFileIinfo.external_fa & 0010) perms |= (QFile::ExeGroup);
        if (unzFileIinfo.external_fa & 0004) perms |= (QFile::ReadOther);
        if (unzFileIinfo.external_fa & 0002) perms |= (QFile::WriteOther);
        if (unzFileIinfo.external_fa & 0001) perms |= (QFile::ExeOther);
        if (!file.setPermissions(perms))
        {
          qDebug("Impossible to set the permissions on one file during the update process: \n%i", file.error());
          break;
        }
        unzCloseCurrentFile(this->m_Download.unz);
        file.close();
      }
      if ((file.error() != QFile::NoError) || !unzipRead)
      {
        qDebug("Impossible to create at least one file during the update process.");
        unzCloseCurrentFile(this->m_Download.unz);
        break;
      }
    }
  }
  while ((res = unzGoToNextFile(this->m_Download.unz)) != UNZ_END_OF_LIST_OF_FILE);
  if (res == UNZ_END_OF_LIST_OF_FILE)
    incompleteInstallation = false;
  
  // Append the moved files in the setting used during the finalization of the installation
  QSettings settings;
  QStringList oldFilesMoved = settings.value("Updater/MovedFiles").toStringList();
  settings.setValue("Updater/MovedFiles", filesMoved + oldFilesMoved);
  
  // Close the compressed file
  if (unzClose(this->m_Download.unz) == UNZ_OK)
  {
    this->m_Download.unz = NULL;
    if (!incompleteInstallation)
    {
      emit installationComplete();
      return;
    }
  }
  else
    qDebug("Impossible to close the downloaded content.");
  
  emit installationIncomplete();
};

void UpdateController::finalizeUpdate()
{
  QSettings settings;
  QStringList filesToRemove = settings.value("Updater/MovedFiles").toStringList();
  for (QStringList::const_iterator itS = filesToRemove.begin() ; itS != filesToRemove.end() ; ++itS)
    QFile::remove(*itS);
  settings.setValue("Updater/MovedFiles", QStringList());
  emit installationFinalized();
};

int UpdateController::compareRelease(const QStringList& ver1, const QStringList& ver2) const
{
  // Inspired by http://stackoverflow.com/questions/198431/how-do-you-compare-two-version-strings-in-java
  // More robust method than the previous one, but still some problems with the content of the suffix (e.g. 0.1a2 > 0.1a10...)
  int i = 0;
  for (i = 0 ; i < ver1.count() ; ++i)
  {
    if (i >= ver2.count())
    {
      for (int j = i ; j < ver1.count() ; ++j)
      {
        QString suffix1;
        int number1 = this->extractReleaseNumber(ver1[j], suffix1);
        if ((number1 != 0) || (suffix1.length() != 0))
        {
          // Version one is longer than number two, and non-zero
          return 1;
        }
      }
      // Version one is longer than version two, but zero
      return 0;
    }
    
    QString suffix1, suffix2;
    int number1 = this->extractReleaseNumber(ver1[i], suffix1);
    int number2 = this->extractReleaseNumber(ver2[i], suffix2);
    
     if (number1 < number2) {
          // Number one is less than number two
          return -1;
      }
      if (number1 > number2) {
          // Number one is greater than number two
          return 1;
      }

      bool empty1 = suffix1.length() == 0;
      bool empty2 = suffix2.length() == 0;

      if (empty1 && empty2) continue; // No suffixes
      if (empty1) return 1; // First suffix is empty (1.2 > 1.2b)
      if (empty2) return -1; // Second suffix is empty (1.2a < 1.2)

      // Lexical comparison of suffixes
      int result = suffix1.compare(suffix2);
      if (result != 0) return result;
  }
  if (i < ver2.count())
  {
    for (int j = i ; j < ver2.count() ; ++j)
    {
      QString suffix2;
      int number2 = this->extractReleaseNumber(ver2[j], suffix2);
      if ((number2 != 0) || (suffix2.length() != 0))
      {
        // Version one is longer than version two, and non-zero
        return -1;
      }
    }
    // Version two is longer than version one, but zero
    return 0;
  }
  
  return 0;
}

int UpdateController::extractReleaseNumber(const QString& str, QString& suffix) const
{
  int num = 0;
  int inc = 0;
  while (inc < str.length())
  {
    QChar c = str.at(inc);
    if (c.isDigit())    
      num = num * 10 + c.digitValue();
    else
      break;
    ++inc;
  }
  suffix = str.mid(inc+1);
  return num;
};