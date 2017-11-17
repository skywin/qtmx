/***************************************************************************
 * Application.cc - Application subclass for QTM
 *
 * Copyright (C) 2008, Matthew J Smith
 *
 * This file is part of QTM.
 * QTM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2), as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *****************************************************************************/


#include "Application.h"
#include "EditingWindow.h"
#include <QtCore>
#include <QMenuBar>

#ifdef Q_OS_MAC
#include "SuperMenu.h"
#endif

  Application::Application( int &argc, char **argv )
: QApplication( argc, argv )
{
  setOrganizationName( "Catkin Project" );
  setOrganizationDomain( "catkin.blogistan.co.uk" );
  setApplicationName( "QTM" );
  _currentEditingWindow = 0;

  connect( this, SIGNAL( aboutToQuit() ), this, SLOT( saveRecentFiles() ) );
  connect( this, SIGNAL( lastWindowClosed() ), this, SLOT( handleLastWindowClosed() ) );
}

void Application::setupRecentFiles()
{
#ifdef Q_OS_MAC
  QString orgString = "catkin.blogistan.co.uk";
#else
  QString orgString = "Catkin Project";
#endif
  QSettings settings( orgString, "QTM" );
  // qDebug() << "Settings path:" << settings.fileName();
  int i;
  recentFile currentRF;
  QString crf;

  settings.beginGroup( "recentFiles" );
  for( i = 0; i < 20; i++ ) {
    crf = settings.value( QString( "recentFile%1" ).arg( i ), "" ).toString();
    // qDebug() << "Recent file:" << crf;
    /*    if( crf.isEmpty() )
          qDebug() << QString( "recentFile%1" ).arg( i ) << "is empty"; */
    currentRF.filename = crf.section( "filename:", 1, 1 ).section( " ##title:", 0, 0 );
    currentRF.title = crf.section( " ##title:", 1, 1 );
    if( currentRF.filename.isEmpty() )
      break;
    _recentFiles.append( currentRF );
    // qDebug() << "Added one recent file";
  }
}

QStringList Application::titles()
{
  int i;
  QStringList returnValue;

  for( i = 0; i < _recentFiles.count(); ++i ) {
    if( _recentFiles.value( i ).title.isEmpty() )
      returnValue << QString();
    else
      returnValue << _recentFiles.value( i ).title;
  }

  return returnValue;
}

QStringList Application::filenames()
{
  int i;
  QStringList returnValue;

  for( i = 0; i < _recentFiles.count(); ++i ) {
    if( _recentFiles.value( i ).filename.isEmpty() )
      returnValue << QString();
    else
      returnValue << _recentFiles.value( i ).filename;
  }

  return returnValue;
}

/* inline void Application::setIsUnityThere( bool iut )
{
  _isUnityThere = iut;
}

inline bool Application::isUnityThere()
{
  return _isUnityThere;
} */

QList<Application::recentFile> Application::recentFiles()
{
  return _recentFiles;
}

Application::recentFile Application::getRecentFile( int index )
{
  recentFile return_value;

  if( index >= _recentFiles.count() ) {
    return_value.title = QString();
    return_value.filename = QString();
  }
  else
    return_value = _recentFiles.at( index );

  return return_value;
}


void Application::setRecentFiles( const QStringList &titles, const QStringList &filenames )
{
  int i;
  QList<recentFile> rfs;
  recentFile thisFile;

  for( i = 0; i < titles.count() && i < 20; ++i ) {
    if( titles.at( i ).isEmpty() )
      thisFile.title = QString();
    else
      thisFile.title = titles.at( i );

    if( filenames.at( i ).isEmpty() )
      thisFile.filename = QString();
    else
      thisFile.filename = filenames.at( i );

    rfs << thisFile;
  }

  _recentFiles = rfs;
  emit recentFilesUpdated( titles, filenames );
}

void Application::addRecentFile( const QString &title, const QString &filename )
{
  recentFile thisFile;
  int i;

  thisFile.title = title;
  thisFile.filename = filename;

  for( i = 0; i < 20; ++i ) {
    if( i == _recentFiles.count() )
      break;
    if( _recentFiles.value( i ).filename == filename )
      _recentFiles.removeAt( i );
  }
  _recentFiles.prepend( thisFile );

  emit recentFilesUpdated( _recentFiles );
}

void Application::saveAll()
{
  EditingWindow *e;
  QWidgetList tlw = QApplication::topLevelWidgets();

  Q_FOREACH( QWidget *w, tlw ) {
    e = qobject_cast<EditingWindow *>( w );
    if( e )
      e->save();
  }
}

void Application::setMainWindow( EditingWindow *ew )
{
#ifdef Q_OS_MAC
  emit mainWindowChanged( ew );
#endif
  _currentEditingWindow = ew;
}

void Application::saveRecentFiles()
{
  int i;
  QSettings settings;

  settings.beginGroup( "recentFiles" );
  for( i = 0; i < 20; ++i ) {
    settings.setValue( QString( "recentFile%1" ).arg( i ),
                       QString( "filename:%1 ##title:%2" )
                       .arg( _recentFiles.value( i ).filename )
                       .arg( _recentFiles.value( i ).title ) );
  }
}

void Application::emitPreferencesUpdated( QObject *obj )
{
  emit preferencesUpdated( obj );
}

void Application::handleWindowChange( QWidget *oldW, QWidget *newW )
{
#ifdef Q_OS_MAC
  QWidget *oldWindow = oldW->window();
  QWidget *newWindow = newW->window();
  EditingWindow *newEW;

  if( oldWindow != newWindow ) {
    newEW = qobject_cast<EditingWindow *>( newWindow );
    if( newEW )
      superMenu->setMainWindow( newEW );
    else
      superMenu->setMainWindow( 0 );
  }
#endif
}

void Application::handleLastWindowClosed()
{
  _currentEditingWindow = 0;
}

