/*********************************************************************************

  SysTrayIcon.cc - System tray icon for QTM
  Copyright (C) 2006-2009 Matthew J Smih

  This file is part of QTM.

  QTM is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License (version 2), as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 *******************************************************************************/

// SysTrayIcon.cc - System tray icon for QTM

#include <QtGlobal>
//#include "useSTI.h"

#ifdef USE_SYSTRAYICON

#ifdef Q_OS_MAC
#include <QSystemTrayIcon>
//#include "macFunctions.h"
#endif

#include <QMenu>
#include <QAction>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QFileDialog>
#include <QMessageBox>
#include <QCoreApplication>
#include <QClipboard>
#include <QUrl>
//#include <QHttpRequestHeader>
#include <QList>
#include <QFile>
#include <QTextStream>
#include <QRegExp>

#include <QtXml>
#include <cstdio>
#include <QtDebug>

#ifndef DONT_USE_DBUS
#include <QtDBus>
#include "DBusAdaptor.h"
#endif

#include "Application.h"
#include "SysTrayIcon.h"
#include "EditingWindow.h"
#include "QuickpostTemplate.h"
#include "QuickpostTemplateDialog.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

//#include "ui_QuickpostTemplateForm.h"

#if QT_VERSION >= 0x050000
#ifdef Q_OS_UNIX
#if !defined Q_OS_MAC && !defined Q_OS_CYGWIN
#define Q_WS_X11
#endif
#endif
#endif

#ifdef Q_WS_X11
#include "susesystray.xpm"
#else
#ifdef Q_OS_WIN32
#include "winsystray.xpm"
#include "winsystray_busy.xpm"
#endif
#endif

#include "qtm_version.h"

  SysTrayIcon::SysTrayIcon( bool noWindow, QObject *parent )
: STI_SUPERCLASS( parent )
{
  bool newWindow;
  bool noNewWindow = false;
  bool isUnityThere = false;
  _dontStart = false;

  QCoreApplication::setOrganizationName( "Catkin Project" );
  QCoreApplication::setOrganizationDomain( "catkin.blogistan.co.uk" );
  QCoreApplication::setApplicationName( "QTM" );
  QCoreApplication::setApplicationVersion( QTM_VERSION );
  userAgentString = "QTM/";
  userAgentString.append( QTM_VERSION );

  qtm = qobject_cast<Application *>( qApp );

  QSettings settings;
  settings.beginGroup( "sysTrayIcon" );
  newWindow = settings.value( "newWindowAtStartup", true ).toBool();
  doubleClickFunction = settings.value( "doubleClickFunction", 0 ).toInt();
  settings.endGroup();
  settings.beginGroup( "account" );
  _copyTitle = settings.value( "copyTitle", true ).toBool();
  settings.endGroup();

#ifndef DONT_USE_DBUS
  new DBusAdaptor( this );

  QDBusConnection sessionBus = QDBusConnection::sessionBus();
  sessionBus.registerObject( "/MainApplication", this );
  sessionBus.registerService( "uk.co.blogistan.catkin" );

  // Now check for presence of Unity
  QDBusConnectionInterface *interface = sessionBus.interface();
  isUnityThere = interface->isServiceRegistered( "com.canonical.Unity" ).value();
  qtm->setIsUnityThere( isUnityThere );
#endif

#ifndef Q_OS_MAC
  switch( doubleClickFunction ) {
    case 0:
      setToolTip( tr( "QTM - Double click for \nnew entry" ) ); break;
    case 1:
      setToolTip( tr( "QTM - Double click for \nQuick Post" ) ); break;
  }

  setIcon( QIcon( QPixmap( sysTrayIcon_xpm ) ) );
#endif

  newWindowAtStartup = new QAction( tr( "New entry at startup" ), this );
  newWindowAtStartup->setCheckable( true );
  newWindowAtStartup->setChecked( newWindow );
  connect( newWindowAtStartup, SIGNAL( toggled( bool ) ),
           this, SLOT( setNewWindowAtStartup( bool ) ) );

  // Override the "newWindowAtStartup" setting if Unity has been found
  QStringList args = QApplication::arguments();
  if( isUnityThere && !args.contains( "--no-new-window" ) )
    newWindow = true;

  menu = new QMenu;
  menu->setObjectName( "iconMenu" );
  menu->addAction( tr( "New entry" ), this, SLOT( newDoc() ) );
  menu->addAction( tr( "Open ..." ), this, SLOT( choose() ) );
  // openRecent = menu->addAction( tr( "Open recent" ) );
  recentFilesMenu = menu->addMenu( tr( "Open recent" ) );
  saveAllAction = menu->addAction( tr( "Save all ..." ), this, SLOT( saveAll() ) );
  menu->addAction( tr( "Quick post" ), this, SLOT( quickpost() ) );
  abortAction = menu->addAction( tr( "Abort quick post" ), this, SLOT( abortQP() ) );
  abortAction->setEnabled( false );
  templateMenu = menu->addMenu( tr( "Quickpost templates" ) );
  templateMenu->setObjectName( "templateMenu" );
  configureTemplates = new QAction( tr( "&Configure ..." ), 0 );
  connect( configureTemplates, SIGNAL( triggered( bool ) ),
           this, SLOT( configureQuickpostTemplates() ) );
  configureTemplates->setObjectName( "QTM Configure Templates" );
  setupQuickpostTemplates();

#if (!defined Q_OS_MAC) || QT_VERSION <= 0x050000
//#ifndef Q_OS_MAC              // Always a new window at startup when
//#if QT_VERSION <= 0x050000   // using Qt 5 on a Mac
  if( !isUnityThere )
    menu->addAction( newWindowAtStartup );
#endif

#ifndef Q_OS_MAC
  menu->addSeparator();
#endif
#ifndef SUPERMENU
  quitAction = menu->addAction( tr( "Quit" ), this, SLOT( doQuit() ) );
#endif
  // quitAction->setMenuRole( QAction::QuitRole );
#ifdef Q_OS_MAC
#if QT_VERSION >= 0x050000 // Qt versions 5.0 and 5.1 have neither of these.
  menu->setAsDockMenu();
#else
  qt_mac_set_dock_menu( menu );
#endif // QT_VERSION
#else // Not a Mac; uses Linux or X11 system tray icon
  setContextMenu( menu );
#endif
  //show();

  qtm->setupRecentFiles();
  noRecentFilesAction = new QAction( tr( "No recent files" ), this );
  noRecentFilesAction->setVisible( true );
  noRecentFilesAction->setEnabled( false );
  for( int i = 0; i < 10; ++i ) {
    recentFileActions[i] = new QAction( this );
    connect( recentFileActions[i], SIGNAL( triggered() ),
             this, SLOT( openRecentFile() ) );
  }
  recentFiles = qtm->recentFiles();
  updateRecentFileMenu();
  // menu->insertMenu( saveAllAction, recentFilesMenu );
  //openRecent->setMenu( recentFilesMenu );
  connect( qtm, SIGNAL( recentFilesUpdated( QList<Application::recentFile> ) ),
           this, SLOT( setRecentFiles( QList<Application::recentFile> ) ) );

#ifndef Q_OS_MAC
  connect( this, SIGNAL( activated( QSystemTrayIcon::ActivationReason ) ),
           this, SLOT( iconActivated( QSystemTrayIcon::ActivationReason ) ) );
#endif

  if( handleArguments() )
    noNewWindow = true;

#ifndef Q_OS_MAC
  if( newWindow && !noNewWindow ) {
    EditingWindow *c = new EditingWindow;
    c->setSTI( this );
    c->setWindowTitle( tr( "QTM - new entry [*]" ) );
    c->show();
    c->activateWindow();
  }
#endif
  
  netmgr = new QNetworkAccessManager;
  httpBusy = false;
  templateQPActive = false;
  activeTemplate = -1;
}

SysTrayIcon::~SysTrayIcon()
{
}

bool SysTrayIcon::handleArguments()
{
  int i;
  bool rv = false;
  EditingWindow *c;
  QStringList failedFiles;
  QStringList args = QApplication::arguments();
  args.removeAll( "&" );

  for( i = 1; i < args.size(); i++ ) {
    c = new EditingWindow;
    if( !args.at( i ).startsWith( "--" ) ) {
      qDebug() << "Opening:" << args.at( i );
      if( c->load( args.at( i ), true ) ) {
        c->setSTI( this );
#ifdef Q_OS_MAC
	//setNoStatusBar( c );
#endif
        c->show();
        rv = true;
      }
      else {
        failedFiles.append( args.at( i ) );
      }
    }
  }
  if( failedFiles.size() ) {
    if( failedFiles.size() == args.size()-1 ) {
      if( QMessageBox::question( 0, tr( "Error" ),
                                 tr( "Could not load the following:\n\n%1" )
                                 .arg( failedFiles.join( "\n" ) ),
                                 tr( "&Continue" ), tr( "E&xit" ) ) ) {
        _dontStart = true;
      }
      else
        rv = false;
    }
    else {
      QMessageBox::information( 0, tr( "Error" ),
                                tr( "Could not load the following:\n\n%1 " )
                                .arg( failedFiles.join( "\n" ) ),
                                QMessageBox::Ok );
      rv = true;
    }
  }
  return rv;
}

void SysTrayIcon::updateRecentFileMenu()
{
  QString text, t;
  int j;
  // qDebug() << "Recent files:" << recentFiles.count();
  QMutableListIterator<Application::recentFile> i( recentFiles );

  while( i.hasNext() ) {
    if( !QFile::exists( i.next().filename ) )
      i.remove();
  }

  recentFilesMenu->clear();

  for( j = 0; j < 10; ++j ) {
    if( j < recentFiles.count() ) {
      t = recentFiles.value( j ).title.section( ' ', 0, 5 );
      if( t != recentFiles.value( j ).title )
        t.append( tr( " ..." ) );
#ifdef Q_OS_MAC
      text = recentFiles.value( j ).title.isEmpty() ?
        recentFiles.value( j ).filename.section( "/", -1, -1 )
        :t.replace( '&', "&&" );
#else
      if( j == 9 )
        text = tr( "1&0 %1" )
          .arg( recentFiles.value( j ).title.isEmpty() ?
                recentFiles.value( j ).filename.section( "/", -1, -1 )
                : t.replace( '&', "&&" ) );
      else
        text = tr( "&%1 %2" )
          .arg( j + 1 )
          .arg( recentFiles.value( j ).title.isEmpty() ?
                recentFiles.value( j ).filename.section( "/", -1, -1 )
                : t.replace( '&', "&&" ) );
#endif
      recentFileActions[j]->setText( text );
      recentFileActions[j]->setData( recentFiles.value( j ).filename );
      recentFilesMenu->addAction( recentFileActions[j] );
      recentFilesMenu->removeAction( noRecentFilesAction );
    }
    else {
      // recentFileActions[j]->setVisible( false );
      if( !recentFiles.count() )
        recentFilesMenu->addAction( noRecentFilesAction );
    }
  }
}

void SysTrayIcon::setRecentFiles( const QList<Application::recentFile> &rfs )
{
  recentFiles = rfs;
  updateRecentFileMenu();
}

void SysTrayIcon::setDoubleClickFunction( int func )
{
  doubleClickFunction = func;

#ifndef Q_OS_MAC
  switch( doubleClickFunction ) {
    case 0:
      setToolTip( tr( "QTM - Double click for \nnew entry" ) ); break;
    case 1:
      setToolTip( tr( "QTM - Double click for \nQuick Post" ) ); break;
  }
#endif
}

QStringList SysTrayIcon::templateTitles()
{
  QStringList rv;
  int i;

  for( i = 0; i < templateTitleList.count(); i++ ) {
    rv.append( QString( "%1.%2" )
               .arg( i )
               .arg( templateTitleList.value( i ) ) );
  }
  return rv;
}

QStringList SysTrayIcon::templates()
{
  QStringList rv;
  QString currentTemplate;
  int i;

  for( i = 0; i< templateTitleList.count(); i++ ) {
    currentTemplate = templateList.value( i )
      .replace( "\n", "\\n" )
      .replace( "]", "\\]" );
    rv.append( QString( "%1.[%2].[%3]" )
               .arg( i )
               .arg( templateTitleList.value( i ) )
               .arg( currentTemplate ) );
  }
  return rv;
}

void SysTrayIcon::setCopyTitle( bool status )
{
  _copyTitle = status;
}

void SysTrayIcon::newDoc()
{
  EditingWindow *c = new EditingWindow;
  c->setSTI( this );
  c->setWindowTitle( QObject::tr( "QTM - new entry [*]" ) );
  c->setPostClean();
#ifdef Q_OS_MAC
  //setNoStatusBar( c );
#endif
  EditingWindow *activeWidget = qtm->currentEditingWindow();
  qDebug() << "got active widget";
  if( activeWidget )
    EditingWindow::positionWidget( c, activeWidget );
  qDebug() << "positioned widget";
  c->show();
  c->activateWindow();
  QApplication::alert( c );
}

void SysTrayIcon::openRecentFile()
{
  QAction *action = qobject_cast<QAction *>( sender() );
  if( action )
    choose( action->data().toString() );
}

void SysTrayIcon::choose( QString fname )
{
  QString fn;
  QSettings settings;
  QString localStorageFileExtn, localStorageDirectory;
  QStringList filesSelected;

  settings.beginGroup( "account" );
  localStorageFileExtn = settings.value( "localStorageFileExtn", "cqt" ).toString();
  localStorageDirectory = settings.value( "localStorageDirectory", "" ).toString();
  settings.endGroup();

  QString extn( QString( "%1 (*.%2)" ).arg( tr( "Blog entries" ) )
                .arg( localStorageFileExtn ) );

  if( fname.isEmpty() )
    fn = QFileDialog::getOpenFileName( 0, tr( "Choose a file to open" ),
                                       localStorageDirectory, extn );
  else
    fn = fname;

  if( !fn.isEmpty() ) {
    EditingWindow *e = new EditingWindow;
    if( !e->load( fn, true ) ) {
#ifdef Q_OS_MAC
      QMessageBox::warning( 0, "QTM",
                            tr( "Could not load the file you specified." ),
                            QMessageBox::Cancel, QMessageBox::NoButton );
#else
      showMessage( tr( "Error" ), 
                   tr( "Could not load the file you specified." ),
                   QSystemTrayIcon::Warning );
#endif
      e->deleteLater();
    } else {
      e->setSTI( this );
      qtm->addRecentFile( e->postTitle(), fn );
#ifdef Q_OS_MAC
      //setNoStatusBar( e );
#endif
      e->show();
      e->activateWindow();
      QApplication::alert( e );
    }
  }
}

void SysTrayIcon::chooseRecentFile()
{
#ifdef UNITY_LAUNCHER
  int i, j;
  QString fileToOpen;
  QList<QListWidgetItem *> recentFilesForDialog;
  QListWidgetItem *thisRecentFile;

  QDialog chooser;
  Ui::ListDialogBase ldui;
  ldui.setupUi( &chooser );

  chooser.setAttribute( Qt::WA_DeleteOnClose, false );
  chooser.setWindowTitle( tr( "Choose recent file" ) );
  ldui.mainLabel->setText( tr( "Recent files:" ) );

  // Load the recent files into the list widget
  recentFiles = qtm->recentFiles();
  j = (recentFiles.count() >= 10) ? 10 : recentFiles.count();
  for( i = 0; i < j; i++ ) {
    thisRecentFile = new QListWidgetItem( recentFiles.at( i ).title );
    thisRecentFile->setData( 32, recentFiles.at( i ).filename );
    ldui.lwFileTemplateList->addItem( thisRecentFile );
  }

  connect( ldui.lwFileTemplateList, SIGNAL( itemDoubleClicked( QListWidgetItem * ) ),
           this, SLOT( openRecentFileFromDialog( QListWidgetItem * ) ) );
  // Dialog must only accept if OK is clicked, as double-click is handled by
  // the above slot.
  connect( ldui.lwFileTemplateList, SIGNAL( itemDoubleClicked( QListWidgetItem * ) ),
           &chooser, SLOT( reject() ) );

  if( chooser.exec() ) {
    fileToOpen = ldui.lwFileTemplateList->currentItem()->data( 32 ).toString();
    choose( fileToOpen );
  }
#endif
}

void SysTrayIcon::openRecentFileFromDialog( QListWidgetItem *item )
{
#ifdef UNITY_LAUNCHER
  QString fileToOpen = item->data( 32 ).toString();
  choose( fileToOpen );
#endif
}

void SysTrayIcon::iconActivated( QSystemTrayIcon::ActivationReason ar )
{
#ifndef Q_OS_MAC
  if( ar == QSystemTrayIcon::DoubleClick )
    switch( doubleClickFunction ) {
      case 0: newDoc(); break;
      case 1: quickpost(); break;
    }

#ifdef Q_WS_X11
  if( ar == QSystemTrayIcon::MiddleClick )
    quickpost( QClipboard::Selection );
#endif
#endif
}

void SysTrayIcon::quickpost( QClipboard::Mode mode )
{
  int i, j;
  bool qpt = false;
  QRegExp regExp;

  cbtext = QApplication::clipboard()->text( mode );

  if( cbtext == "" ) {
#ifdef Q_OS_MAC
    QMessageBox::warning( 0, tr( "No selection! - QTM" ),
                          tr( "No web location specified to blog about." ),
                          QMessageBox::Cancel );
#else
    showMessage( tr( "No selection!" ),
                 tr( "No web location specified to blog about." ),
                 QSystemTrayIcon::Warning );
#endif
  }
  else {
    if( !cbtext.startsWith( "http://" )
#if !defined DONT_USE_SSL
        && !cbtext.startsWith( "https://" )
#endif
      ) {

      // If it's not obviously an URL.
      if( cbtext.startsWith( "https" ) ) {
#ifndef Q_OS_MAC
          showMessage( tr( "Error" ), tr( "This version of QTM does not support HTTPS." ) );
#else
          QMessageBox::information( 0, tr( "Quickpost Error" ),
                                    tr( "This version of QTM does not support HTTPS." ) );
#endif
      }
      doQP( "" );
    }
    else {
      if( !QUrl( cbtext, QUrl::StrictMode ).isValid() ) {
        if( cbtext.contains( QChar( ' ' ) ) ) {
          doQP( "" );
        }
        else {
#ifdef Q_OS_MAC
          QMessageBox::warning( 0, tr( "Invalid web location" ),
                                tr( "Quick Post requires valid web location." ),
                                QMessageBox::Cancel );
#else
          showMessage( tr( "Invalid web location" ),
                       tr( "Quick Post requires valid web location." ),
                       QSystemTrayIcon::Warning );
#endif
        }
      }
      else {
        // Otherwise, it's an URL, and has to be downloaded to extract the title.
        if( !httpBusy ) {
          if( assocHostLists.count() ) {
            for( i = 0; i < assocHostLists.count(); ++i ) {
              if( assocHostLists.at( i ).count() ) {
                for( j = 0; j < assocHostLists.at( i ).count(); j++ ) {
                  if( (!assocHostLists.at( i ).at( j ).isEmpty()) &&
                      cbtext.contains( QRegExp( QString( "[/\\.]%1[/\\.]" )
                                                .arg( assocHostLists.at( i ).at( j ) ) ) ) ) {
                    qpt = true;
                    quickpostFromTemplate( i, 
                                           quickpostTemplateActions.value( i )->postTemplate(),
                                           cbtext );
                    break;
                  }
                }
              }
              if( qpt )
                break;
            }
          }

          if( !qpt ) {
            //// qDebug() << "doing quickpost";
            QNetworkRequest request;
            request.setUrl( QUrl( cbtext ) );
            request.setRawHeader( "User-Agent", userAgentString );
            currentReply = netmgr->get( request );
            qDebug() << "Fetching URL:" << request.url();
                                                    /* 
            QString withoutHttp = cbtext.section( "://", 1 );
            QString host = withoutHttp.section( "/", 0, 0 );
            QString loc = withoutHttp.section( '/', 1, -1, QString::SectionIncludeLeadingSep );
            QHttpRequestHeader header( "GET", loc );
            header.setValue( "Host", host );
            header.setValue( "User-Agent", userAgentString );

#if QT_VERSION >= 0x040300 && !defined DONT_USE_SSL
            http->setHost( host, cbtext.startsWith( "https://" ) ?
                           QHttp::ConnectionModeHttps : QHttp::ConnectionModeHttp );
            connect( http, SIGNAL( sslErrors( QList<QSslError> ) ),
                     http, SLOT( ignoreSslErrors() ) );
#else
            http->setHost( host );
#endif
            http->request( header ); */
            abortAction->setEnabled( true );
            httpBusy = true;
            templateQPActive = false;
#ifdef Q_OS_WIN32
            setIcon( QIcon( QPixmap( sysTrayIcon_busy_xpm ) ) );
#endif
            connect( netmgr, SIGNAL( finished( QNetworkReply * ) ),
                     this, SLOT( handleDone( QNetworkReply * ) ) );
          }
        }
      }
    }
  }
}


void SysTrayIcon::quickpostFromTemplate( int id, QString templateString, QString t )
{
  activeTemplate = id;

  if( t.isNull() )
    cbtext = QApplication::clipboard()->text( QClipboard::Clipboard );
  else
    cbtext = t;

  if( cbtext.startsWith( "https://" ) ) {
#ifndef Q_OS_MAC
      showMessage( tr( "Error" ),
                   tr( "This version of QTM does not support HTTPS." ) );
#else
      QMessageBox::information( 0, tr( "Quickpost Error" ),
                                tr( "This version of QTM does not support HTTPS." ) );
#endif
  }

  if( !QUrl( cbtext, QUrl::StrictMode ).isValid() ) {
#ifdef Q_OS_MAC
    QMessageBox::warning( 0, tr( "No selection!" ),
                          tr( "The selection is not a web location, or is invalid." ),
                          QMessageBox::Cancel );
#else
    showMessage( tr( "No selection!" ),
                 tr( "The selection is not a web location, or is invalid." ),
                 QSystemTrayIcon::Warning );
#endif
  }
  else {
    if( !httpBusy ) {
      QNetworkRequest request;
      request.setUrl( QUrl( cbtext ) );
      request.setRawHeader( "User-Agent", userAgentString );
      currentReply = netmgr->get( request ); /*
      QString withoutHttp = cbtext.section( "://", 1 );
      QString host = withoutHttp.section( "/", 0, 0 );
      QString loc = withoutHttp.section( '/', 1, -1, QString::SectionIncludeLeadingSep );

#if QT_VERSION >= 0x040300 && !defined DONT_USE_SSL
      http->setHost( host, cbtext.startsWith( "https://" ) ?
                     QHttp::ConnectionModeHttps : QHttp::ConnectionModeHttp );
      connect( http, SIGNAL( sslErrors( QList<QSslError> ) ),
               http, SLOT( ignoreSslErrors() ) );
#else
      http->setHost( host );
#endif
      http->get( loc ); */
      abortAction->setEnabled( true );
      httpBusy = true;
      templateQPActive = true;
#ifdef Q_OS_WIN32
      setIcon( QIcon( QPixmap( sysTrayIcon_busy_xpm ) ) );
#endif
      connect( netmgr, SIGNAL( finished( QNetworkReply * ) ),
               this, SLOT( handleDone( QNetworkReply * ) ) );
/*      connect( http, SIGNAL( readyRead( const QHttpResponseHeader & ) ),
               this, SLOT( handleResponseHeader( const QHttpResponseHeader & ) ) ); */
    }
  }
}

void SysTrayIcon::quickpostFromDBus( QString &url, QString &content )
{
  int i, j;

  for( i = 0; i < assocHostLists.count(); ++i ) {
    if( assocHostLists.at( i ).count() ) {
      for( j = 0; j < assocHostLists.at( i ).count(); j++ ) {
        if( (!assocHostLists.at( i ).at( j ).isEmpty()) &&
            url.contains( QRegExp( QString( "[/\\.]%1[/\\.]" )
                                   .arg( assocHostLists.at( i ).at( j ) ) ) ) ) {
          activeTemplate = i;
          break;
        }
      }
    }
  }
  cbtext = url;
  doQP( content );
}

void SysTrayIcon::chooseQuickpostTemplate()
{
#ifdef UNITY_LAUNCHER
  QListWidgetItem *thisTemplate;

  QDialog chooser;
  Ui::ListDialogBase ldui;
  ldui.setupUi( &chooser );

  chooser.setAttribute( Qt::WA_DeleteOnClose, false );
  chooser.setWindowTitle( tr( "Choose template" ) );
  ldui.mainLabel->setText( tr( "Templates:" ) );

  Q_FOREACH( QuickpostTemplate *temp, quickpostTemplateActions ) {
    if( temp != 0 ) {
      thisTemplate = new QListWidgetItem( temp->text() );
      thisTemplate->setData( 32, temp->identifier() );
      ldui.lwFileTemplateList->addItem( thisTemplate );
    }
  }

  connect( ldui.lwFileTemplateList, SIGNAL( itemDoubleClicked( QListWidgetItem * ) ),
           this, SLOT( actOnChooseQuickpostTemplate( QListWidgetItem * ) ) );
  connect( ldui.lwFileTemplateList, SIGNAL( itemDoubleClicked( QListWidgetItem * ) ),
           &chooser, SLOT( reject() ) );

  if( chooser.exec() ) {
    if( quickpostTemplateActions.at( ldui.lwFileTemplateList->currentRow() ) != 0 )
      quickpostTemplateActions[ldui.lwFileTemplateList->currentRow()]->trigger();
  }
#endif
}

void SysTrayIcon::actOnChooseQuickpostTemplate( QListWidgetItem *item )
{
#ifdef UNITY_LAUNCHER
  QuickpostTemplate *temp = quickpostTemplateActions.at( item->data( 32 ).toInt() );
  if( temp )
    temp->trigger();
#endif
}

void SysTrayIcon::setNewWindowAtStartup( bool nwas )
{
  QSettings settings;

  settings.beginGroup( "sysTrayIcon" );
  settings.setValue( "newWindowAtStartup", nwas );
  settings.endGroup();
}

// HTTP Quickpost handler routines

void SysTrayIcon::handleDone( QNetworkReply *reply )
{
  QString ddoc, newTitle, errorString;
  QNetworkReply::NetworkError netError;

  abortAction->setEnabled( false );
  httpBusy = false;

  netError = reply->error();
  if( netError != QNetworkReply::NoError ) {
    switch( netError ) {
      case QNetworkReply::HostNotFoundError:
        errorString = tr( "Could not find the host." ); break;
      case QNetworkReply::ConnectionRefusedError:
        errorString = tr( "Connection was refused." ); break;
      case QNetworkReply::RemoteHostClosedError:
        errorString = tr( "Connection closed unexpectedly." ); break;
      default:
        errorString = tr( "Document was not received correctly." ); break;
    }
#ifdef Q_OS_MAC
    QMessageBox::warning( 0, tr( "Quickpost Error" ),
                          errorString, QMessageBox::Cancel );
#else
    showMessage( tr( "Quickpost Error" ), errorString, QSystemTrayIcon::Warning );
#endif
  }
  else {
    responseData = reply->readAll();
    doQP( QString( responseData ) );
    responseData = "";
  }

  // http->close();
  netmgr->disconnect();
  currentReply->deleteLater();
  currentReply = NULL;
#ifndef Q_OS_MAC
  setIcon( QIcon( QPixmap( sysTrayIcon_xpm ) ) );
#endif
}

void SysTrayIcon::doQuit()
{
  int edwins = 0;
  int a;

  QWidgetList tlw = qApp->topLevelWidgets();
  for( a = 0; a < tlw.size(); a++ ) {
    if( (QString( tlw[a]->metaObject()->className() ) == "EditingWindow")
        && tlw[a]->isVisible() )
      edwins++;
  }

  if( !edwins )
    QCoreApplication::quit();
  else {
    qApp->setQuitOnLastWindowClosed( true );
    qApp->closeAllWindows();
  }

}

void SysTrayIcon::saveAll()
{
  Application *qtm = qobject_cast<Application *>( qApp );
  qtm->saveAll();
}

void SysTrayIcon::doQP( QString receivedText )
{
  QString newPost, newTitle;
  // QRegExp titleRegExp( "<title( (dir=\"(ltr)|(rtl)\")|((xml:)?lang=\"[a-zA-Z-]+\"){0, 2}>", Qt::CaseInsensitive );
  QRegExp titleRegExp( "<title.*</title>", Qt::CaseInsensitive );
  QRegExp openTitleRX( "<title", Qt::CaseInsensitive );
  QRegExp closeTitleRX( "</title", Qt::CaseInsensitive );

  if( activeTemplate >= 0 ) {
    if( receivedText == "" )
      newPost = cbtext;
    else {
      newPost = templateList[activeTemplate];
      newPost.replace( "%url%", cbtext );
      newPost.replace( "%host%", cbtext.section( "//", 1, 1 ).section( "/", 0, 0 ) );
      newPost.replace( "%domain%", cbtext.section( "//", 1, 1 ).section( "/", 0, 0 )
                       .remove( "www." ) );
      if( receivedText.contains( titleRegExp )
          && receivedText.section( openTitleRX, 1 )
          .section( ">", 1 ).contains( closeTitleRX ) ) {
        newTitle = receivedText.section( openTitleRX, 1 ).section( ">", 1 )
          .section( closeTitleRX, 0, 0 ).simplified();
        newPost.replace( "%title%", newTitle );
      }
    }
    newPost.replace( "\\n", "\n" );
  } else {
    if( receivedText == "" )
      newPost = cbtext;
    else {
      //// // qDebug( receivedText.left( 300 ).toAscii().constData() );
      // The title check will accept flaky 1990s HTML - this isn't a browser
      if( receivedText.contains( titleRegExp ) ) {
        /*&& receivedText.section( titleRegExp, 1 )
          .section( '\n', 0 ).contains( "</title>", Qt::CaseInsensitive ) )*/
        newTitle = receivedText.section( openTitleRX, 1 ).section( ">", 1 )
          .section( closeTitleRX, 0, 0 )
          .simplified();
        if( !templateQPActive )
          newPost = QString( "<a title = \"%1\" href=\"%2\">%1</a>\n\n" )
            .arg( newTitle )
            .arg( cbtext );
      }
      else // Post has no valid title
        newPost = QString( tr( "<a href=\"%1\">Insert link text here</a>" ) )
          .arg( cbtext );
    }
  }

  EditingWindow *c = new EditingWindow( newPost );
  c->setSTI( this );
  c->setPostClean();

  if( activeTemplate >= 0 ) {
    switch( defaultPublishStatusList[activeTemplate] ) {
      case 0:
      case 1:
        c->setPublishStatus( defaultPublishStatusList[activeTemplate] );
    }
    // Copy the title, if a quickpost
    if( copyTitleStatusList[activeTemplate] ) {
      if( !newTitle.isEmpty() )
        c->setWindowTitle( QString( "%1 - QTM [*]" ).arg( newTitle ) );
      c->setPostTitle( newTitle );
    }
  }
  else {
    if( _copyTitle ) {
      // Copy the title, if not a quickpost
      c->setPostTitle( newTitle );
      if( !newTitle.isEmpty() )
        c->setWindowTitle( QString( "%1 - QTM [*]" ).arg( newTitle ) );
    }
  }

#ifdef Q_OS_MAC
  //setNoStatusBar( c );
#endif
  c->show();
  c->activateWindow();
#ifdef Q_OS_WIN32
  setIcon( QIcon( QPixmap( sysTrayIcon_xpm ) ) );
#endif

  QApplication::alert( c );

  activeTemplate = -1;
  templateQPActive = false;
}

void SysTrayIcon::abortQP()
{
  currentReply->abort();
  currentReply->deleteLater();
  currentReply = NULL;

  netmgr->disconnect();
  abortAction->setEnabled( false );
  httpBusy = false;
}

void SysTrayIcon::setupQuickpostTemplates()
{
  QString templateFile;
  QString errorString;
  QString currentTitle, currentTemplate;
  int currentDefaultPublishStatus;
  int errorLine, errorCol;
  QDomNodeList templateNodes, titles, templateStrings, defaultPublishStates;
  QDomNodeList assocHostsInTemplate;
  QDomElement currentAssocHostListsElement, currentCopyStatusElement;
  QString currentCopyStatusElementText;
  int numTemplates, t, j;
  QTextStream ts( stdout );
  bool useDefaultPublishStatus, ok;

  QSettings settings;
  settings.beginGroup( "account" );
  templateFile = QString( "%1/qptemplates.xml" ).
    arg( settings.value( "localStorageDirectory", "" ).toString() );
  settings.endGroup();

  if( QFile::exists( templateFile ) ) {
    QDomDocument domDoc( "quickpostTemplates" );
    QFile file( templateFile );
    if( !domDoc.setContent( &file, true, &errorString, &errorLine, &errorCol ) )
      QMessageBox::warning( 0, tr( "Failed to read templates" ),
                            QString( tr( "Error: %1\n"
                                         "Line %2, col %3" ) )
                            .arg( errorString ).arg( errorLine )
                            .arg( errorCol ) );
    else {
      templateNodes = domDoc.elementsByTagName( "template" );
      titles = domDoc.elementsByTagName( "title" );
      templateStrings = domDoc.elementsByTagName( "templateString" );
      defaultPublishStates = domDoc.elementsByTagName( "defaultPublishStatus" );

      int tnc = templateNodes.count();
      if( tnc ) {
        for( int i = 0; i < templateNodes.count(); i++ ) {
          currentAssocHostListsElement = templateNodes.at( i ).firstChildElement( "associatedHosts" );
          assocHostLists.append( QStringList() );
          if( !currentAssocHostListsElement.isNull() ) {
            assocHostsInTemplate = currentAssocHostListsElement.elementsByTagName( "associatedHost" );
            for( j = 0; j < assocHostsInTemplate.count(); j++ )
              assocHostLists[i].append( assocHostsInTemplate.at( j ).toElement().text() );
          }
          currentCopyStatusElement = templateNodes.at( i ).firstChildElement( "copyTitleStatus" );
          if( currentCopyStatusElement.isNull() ) {
            copyTitleStatusList.append( false );
          }
          else {
            currentCopyStatusElementText = currentCopyStatusElement.text();
            if( currentCopyStatusElementText.startsWith( "1" ) )
              copyTitleStatusList.append( true );
            else
              copyTitleStatusList.append( false );
          }
        }
      }

      // // qDebug() << "Built list of associated hosts";

      if( titles.size() ) {
        quickpostTemplateActions.clear();
        numTemplates = ( titles.size() >= templateStrings.size() ?
                         titles.size() : templateStrings.size() );
        useDefaultPublishStatus = (defaultPublishStates.size() < numTemplates) ?
          false : true;

        // // qDebug( "Populating template menu" );
        // templateMenu->addSeparator();
        templateTitleList.clear();
        templateList.clear();
        for( t = 0; t < numTemplates; t++ ) {
          currentTitle = titles.item( t ).toElement().text();
          currentTemplate = templateStrings.item( t ).toElement().text();
          currentDefaultPublishStatus = useDefaultPublishStatus ? 
            defaultPublishStates.item( t ).toElement().text().toInt( &ok ) : 2;
          templateTitleList.append( currentTitle );
          templateList.append( currentTemplate );
          if( ok ) {
            switch( currentDefaultPublishStatus ) {
              case 0:
              case 1:
                defaultPublishStatusList.append( currentDefaultPublishStatus ); break;
              default:
                defaultPublishStatusList.append( 2 );
            }
          }
          else
            defaultPublishStatusList.append( 2 );

          ts << titles.item( t ).nodeValue();
          ts << templateStrings.item( t ).nodeValue();
          quickpostTemplateActions.append( new QuickpostTemplate( t, currentTitle,
                                                                  currentTemplate, 
                                                                  templateMenu ) );
          templateMenu->addAction( quickpostTemplateActions[t] );
          connect( quickpostTemplateActions[t], 
                   SIGNAL( quickpostRequested( int, QString ) ),
                   this, SLOT( quickpostFromTemplate( int, QString ) ) );
        }
        templateMenu->addSeparator();
      }
      /*      else
      // qDebug( "No templates found." );*/
    }
    file.close();
  }
  templateMenu->addAction( configureTemplates );
}

void SysTrayIcon::configureQuickpostTemplates( QWidget *parent )
{
  QDomDocument templateDocument;
  QDomElement quickpostTemplates;
  QString templateFileName;
  QString templateFile;
  QSettings settings;

  QuickpostTemplateDialog templateDialog( templateTitleList, templateList,
                                          defaultPublishStatusList, copyTitleStatusList,
                                          assocHostLists, _copyTitle, parent );

  configureTemplates->setEnabled( false );
  configureTemplates->setMenu( 0 );
  if( templateDialog.exec() ) {
    // Set templates menu back up
    templateMenu->clear();
    templateTitleList = templateDialog.templateTitles();
    templateList = templateDialog.templateStrings();
    defaultPublishStatusList = templateDialog.defaultPublishStates();
    copyTitleStatusList = templateDialog.copyTitleStates();
    assocHostLists = templateDialog.assocHostLists();
    quickpostTemplates = templateDocument.createElement( "QuickpostTemplates" );
    int numTemplates = (templateTitleList.size() <= templateList.size()) ?
      templateTitleList.size() : templateList.size();
    quickpostTemplateActions.clear();
    for( int i = 0; i < numTemplates; i++ ) {
      quickpostTemplates.appendChild( templateElement( templateDocument,
                                                       templateTitleList[i],
                                                       templateList[i],
                                                       defaultPublishStatusList[i],
                                                       copyTitleStatusList[i],
                                                       assocHostLists[i] ) );

      quickpostTemplateActions.append( new QuickpostTemplate( i,
                                                              templateTitleList[i],
                                                              templateList[i],
                                                              templateMenu ) );
      templateMenu->addAction( quickpostTemplateActions[i] );
      connect( quickpostTemplateActions[i],
               SIGNAL( quickpostRequested( int, QString ) ),
               this, SLOT( quickpostFromTemplate( int, QString ) ) );
    }
    templateDocument.appendChild( quickpostTemplates );
    templateMenu->addSeparator();
    configureTemplates->setEnabled( true );
    templateMenu->addAction( configureTemplates );

    // Save template XML file
    templateDocument.insertBefore( templateDocument.createProcessingInstruction( "xml", "version=\"1.0\"" ),
                                   templateDocument.firstChild() );
    settings.beginGroup( "account" );
#if defined Q_OS_WIN32
    QString templateFileDir = QDir::toNativeSeparators( settings.value( "localStorageDirectory",
                                                                        QString( "%1/QTM blog" ).arg( QDir::homePath() ) ).toString() );
#else
    QString templateFileDir = settings.value( "localStorageDirectory",
                                              QString( "%1/qtm-blog" ).arg( QDir::homePath() ) ).toString();
#endif
    QDir tfd( templateFileDir );
    if( !tfd.exists() )
      tfd.mkpath( templateFileDir );
    templateFileName = QString( "%1/qptemplates.xml" ).arg( templateFileDir );
    settings.endGroup();
    QFile *templateFile = new QFile( templateFileName );
    if( templateFile->open( QIODevice::WriteOnly ) ) {
      QTextStream templateFileStream( templateFile );
      templateDocument.save( templateFileStream, 4 );
      templateFile->close();
    }
    else
#ifdef Q_OS_MAC
      QMessageBox::warning( 0, tr( "Error" ),
                            tr( "Could not write to templates file" ),
                            QMessageBox::Cancel );
#else
      showMessage( tr( "Error" ),
                   tr( "Could not write to templates file" ), Warning );
#endif
    emit quickpostTemplateTitlesUpdated( templateTitles() );
    emit quickpostTemplatesUpdated( templates() );
    templateFile->deleteLater();
  }
  else {
    configureTemplates->setEnabled( true );
  }
}

QDomElement SysTrayIcon::templateElement( QDomDocument &doc,
                                          QString &title,
                                          QString &templateString,
                                          int &defaultPublishStatus,
                                          bool &copyTitleStatus,
                                          QStringList &assocHosts )
{
  QDomElement returnElement = doc.createElement( "template" );
  QDomElement titleElement = doc.createElement( "title" );
  titleElement.appendChild( QDomText( doc.createTextNode( title ) ) );
  QDomElement templateStringElement = doc.createElement( "templateString" );
  templateStringElement.appendChild( QDomText( doc.createTextNode( templateString ) ) );
  QDomElement defaultPublishStatusElement( doc.createElement( "defaultPublishStatus" ) );
  defaultPublishStatusElement.appendChild( QDomText( doc.createTextNode( QString::number( defaultPublishStatus ) ) ) );
  QDomElement copyTitleElement( doc.createElement( "copyTitleStatus" ) );
  copyTitleElement.appendChild( QDomText( doc.createTextNode( QString::number( copyTitleStatus ? 1 : 0 ) ) ) );
  returnElement.appendChild( titleElement );
  returnElement.appendChild( templateStringElement );
  returnElement.appendChild( defaultPublishStatusElement );
  returnElement.appendChild( copyTitleElement );

  if( assocHosts.count() ) {
    QDomElement associatedHost;
    QDomElement assocHostListsElement = doc.createElement( "associatedHosts" );
    for( int i = 0; i < assocHosts.count(); i++ ) {
      associatedHost = doc.createElement( "associatedHost" );
      associatedHost.appendChild( QDomText( doc.createTextNode( assocHosts[i] ) ) );
      assocHostListsElement.appendChild( associatedHost );
    }
    returnElement.appendChild( assocHostListsElement );
  }

  return returnElement;
}
#endif

