/*******************************************************************************

  QTM - Qt-based blog manager
  Copyright (C) 2006-2009 Matthew J Smith

  This program is free software; you can redistribute it and/or modify
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

// EditingWindow_ResponseHandlers.cc - XML-RPC response handlers for QTM

#include <QtCore>
#include <QtGui>
#include <QtXml>

#if QT_VERSION >= 0x050000
#include <QtWidgets>
#endif

#include "EditingWindow.h"
#include "SuperMenu.h"
#include "XmlRpcHandler.h"

typedef enum _WWDN {
  NoActivity,
  GettingID,
  GettingContent
} PostIDActivity;

void EditingWindow::blogger_getUsersBlogs( QByteArray response )
{
  QXmlInputSource xis;
  QXmlSimpleReader reader;
  XmlRpcHandler handler( currentHttpBusiness );
  QDomDocument doc;
  QDomNodeList blogNodeList;
  QDomDocumentFragment importedBlogList;

  console->insertPlainText( QString( response ) );
  cw.cbBlogSelector->disconnect( this, SLOT( changeBlog( int ) ) );
  disconnect( this, SIGNAL( httpBusinessFinished() ), 0, 0 );
  handler.setProtocol( currentHttpBusiness );
  reader.setContentHandler( &handler );
  reader.setErrorHandler( &handler );
  xis.setData( response );
  reader.parse( &xis );

  importedBlogList = handler.returnXml();
  blogNodeList = importedBlogList.firstChildElement( "blogs" )
    .elementsByTagName( "blog" );

  cw.cbBlogSelector->clear();

  int i = blogNodeList.count();
  QString fstring = handler.faultString();
  if( !fstring.isEmpty() ) {
    statusWidget->showMessage( tr( "Could not connect - check account details & password" ), 2000 );
    addToConsole( QString( "%1\n" ).arg( fstring ) );
    cw.cbBlogSelector->setEnabled( false );
    cw.chNoCats->setEnabled( false );
    cw.cbMainCat->setEnabled( false );
    cw.cbMainCat->clear();
    cw.lwOtherCats->setEnabled( false );
    cw.lwOtherCats->clear();
  }
  else {
    if( !i ) {
      statusWidget->showMessage( tr( "No blogs found" ), 2000 );
    }
    else {
      currentAccountElement.removeChild( currentAccountElement.firstChildElement( "blogs" ) );
      //qDebug() << "appending new blogs element";
      currentAccountElement.appendChild( accountsDom.importNode( importedBlogList.firstChildElement( "blogs" ), true ) );
      //qDebug() << "done";
      if( !noAutoSave ) {
        QFile domOut( PROPERSEPS( QString( "%1/qtmaccounts2.xml" ).arg( localStorageDirectory ) ) );
        if( domOut.open( QIODevice::WriteOnly ) ) {
          QTextStream domFileStream( &domOut );
          accountsDom.save( domFileStream, 2 );
          domOut.close();
          //qDebug() << "saved";
        }
        else
          statusWidget->showMessage( tr( "Could not write to accounts file (error %1)" ).arg( (int)domOut.error() ), 2000 );
      }

      for( int a = 0; a < i; a++ ) {
        cw.cbBlogSelector->addItem( blogNodeList.at( a ).firstChildElement( "blogName" ).text(),
                                    QVariant( blogNodeList.at( a ).firstChildElement( "blogid" ).text() ) );
      }

      cw.cbBlogSelector->setCurrentIndex( 0 );
      currentBlogElement = currentAccountElement.firstChildElement( "blogs" ).firstChildElement( "blog" );
      currentBlogid = cw.cbBlogSelector->itemData( 0 ).toString();
      cw.cbBlogSelector->setEnabled( true );
      disconnect( cw.cbBlogSelector, SIGNAL( activated( int ) ),
                  this, SLOT( changeBlog( int ) ) );
      connect( cw.cbBlogSelector, SIGNAL( activated( int ) ),
               this, SLOT( changeBlog( int ) ) );
      addToConsole( accountsDom.toString( 2 ) );

      if( !initialChangeBlog ) {
        disconnect( cw.cbBlogSelector, SIGNAL( activated( int ) ),
                    this, SLOT( changeBlog( int ) ) );
        connect( cw.cbBlogSelector, SIGNAL( activated( int ) ),
                 this, SLOT( changeBlog( int ) ) );
      }

      if( loadedEntryBlog != 999 )
        connect( this, SIGNAL( httpBusinessFinished() ),
                 this, SLOT( doInitialChangeBlog() ) );
    }
  }
#ifndef NO_DEBUG_OUTPUT
  // qDebug() << "Finished handling the output";
#endif
  if( QApplication::overrideCursor() )
    QApplication::restoreOverrideCursor();
}

void EditingWindow::metaWeblog_newPost( QByteArray response )
{
  // Returned data should only contain a single string, and no structs. Hence
  // the XmlRpcHandler is not suitable.
#ifndef NO_DEBUG_OUTPUT
  // qDebug() << "posted the piece";
#endif

  addToConsole( QString( response ) );
  if( response.contains( "<fault>" ) ) {
    statusWidget->showMessage( tr( "The submission returned a fault - see console." ), 2000 );
    if( QApplication::overrideCursor() )
      QApplication::restoreOverrideCursor();
    QTimer::singleShot( 1000, this, SLOT( hideProgressBar() ) );
  }
  else {
    QString parsedData( response );
    entryNumber = parsedData.section( "<string>", 1, 1 )
      .section( "</string>", -2, -2 );
    if( !useWordpressAPI ) {
      if( !cw.chNoCats->isChecked() )
        connect( this, SIGNAL( httpBusinessFinished() ),
                 this, SLOT( setPostCategories() ) );
    }

    if( !entryEverSaved ) {
      if( postAsSave && cleanSave ) {
        setWindowModified( false );
        dirtyIndicator->hide();
        setDirtySignals( true );
      }
    }
  entryBlogged = true;
  }

  addToConsole( QString( "Entry number: %1\n" ).arg( entryNumber ) );
}


void EditingWindow::metaWeblog_editPost( QByteArray response )
{
  addToConsole( QString( response ) );

  if( response.contains( "<fault>" ) ) {
    statusWidget->showMessage( tr( "The submission returned a fault - see console." ), 2000 );
    QTimer::singleShot( 1000, this, SLOT( hideProgressBar() ) );
  }
  else {
    if( !useWordpressAPI )
      connect( this, SIGNAL( httpBusinessFinished() ),
               this, SLOT( setPostCategories() ) );
    if( !entryEverSaved ) {
      if( postAsSave && cleanSave ) {
        setWindowModified( false );
        dirtyIndicator->hide();
        setDirtySignals( true );
      }
    }
  }
  if( QApplication::overrideCursor() != 0 )
    QApplication::restoreOverrideCursor();
}

void EditingWindow::metaWeblog_newMediaObject( QByteArray response )
{
  QXmlInputSource xis;
  QXmlSimpleReader reader;
  XmlRpcHandler handler;

#ifndef NO_DEBUG_OUTPUT
  // qDebug() << "Handling RPC response";
#endif
  disconnect( this, SIGNAL( httpBusinessFinished() ), 0, 0 );
  addToConsole( QString( response ) );

  if( !response.contains( "<fault>" ) ) {
    if( response.contains( "<name>url</name>" ) ) {
      remoteFileLocation = QString( response )
        .section( "<string>", 1, 1 )
        .section( "</string>", 0, 0 );
      cw.cbPageSelector->setCurrentIndex( 0 );
      cw.stackedWidget->setCurrentIndex( 0 );
      cw.copyURLWidget->show();
      if( superMenu )
        superMenu->copyULAction->setVisible( true );
      statusWidget->showMessage( tr( "Your file is here: %1" ).arg( remoteFileLocation ), 2000 );
    }
    else
      statusWidget->showMessage( tr( "The upload returned a fault." ), 2000 );
  }
  else {
    statusWidget->showMessage( tr( "The upload returned a fault." ), 2000 );
  }
#ifndef NO_DEBUG_OUTPUT
  // qDebug() << "Finished handling response";
#endif

  if( QApplication::overrideCursor() != 0 )
    QApplication::restoreOverrideCursor();
}

void EditingWindow::mt_publishPost( QByteArray response )
{
  disconnect( this, SIGNAL( httpBusinessFinished() ), 0, 0 );
  addToConsole( QString( response ) );

  if( response.contains( "<fault>" ) )
    statusWidget->showMessage( tr( "An error occurred during rebuilding." ), 2000 );
  else
    statusWidget->showMessage( tr( "The post was published successfully." ), 2000 );

  if( QApplication::overrideCursor() != 0 )
    QApplication::restoreOverrideCursor();
}

void EditingWindow::mt_getCategoryList( QByteArray response )
{
  QXmlInputSource xis;
  QXmlSimpleReader reader;
  XmlRpcHandler handler( currentHttpBusiness );
  QDomDocumentFragment importedCategoryList;
  QDomElement newCategoriesElement, currentCategory, currentID, currentName;
  QString xmlRpcFaultString, ccID;
  QStringList catList;
  int uncategorized = 0, itval = 0;

  cw.lwOtherCats->reset();

  addToConsole( response );

  handler.setProtocol( currentHttpBusiness );
  reader.setContentHandler( &handler );
  reader.setErrorHandler( &handler );
  xis.setData( response );
  reader.parse( &xis );
  while( !handler.isMethodResponseFinished() )
    QCoreApplication::processEvents();

  bool xfault = handler.fault();

  importedCategoryList = handler.returnXml();
  xmlRpcFaultString = handler.faultString();

  categoryNames = handler.returnList( "categoryName" );
  categoryIDs = handler.returnList( "categoryId" );

  QDomElement returnedCategoriesElement = importedCategoryList.firstChildElement( "categories" );
  QDomNodeList returnedCats = returnedCategoriesElement.elementsByTagName( "category" );
  int i = returnedCats.size();
  for( int j = 0; j < i; j++ )
    if( !returnedCats.at( j ).firstChildElement( "categoryId" ).isNull() &&
        !returnedCats.at( j ).firstChildElement( "categoryName" ).isNull() ) {
      catList.append( QString( "%1 ##ID:%2" )
                      .arg( returnedCats.at( j ).firstChildElement( "categoryName" ).text() )
                      .arg( returnedCats.at( j ).firstChildElement( "categoryId" ).text() ) );
    }
  if( !noAlphaCats )
    qSort( catList.begin(), catList.end(), EditingWindow::caseInsensitiveLessThan );

  if( xfault ) {
    statusWidget->showMessage( tr( "Could not connect; check account details & password" ), 2000 );
  }
  else {
    if( !i ) {
      statusWidget->showMessage( tr( "There are no categories." ) );
      cw.chNoCats->setEnabled( false );
      cw.cbMainCat->setEnabled( false );
      cw.lwOtherCats->setEnabled( false );
    }
    else {
      newCategoriesElement = accountsDom.createElement( "categories" );
      QStringList::iterator it;
      cw.cbMainCat->clear();
      cw.lwOtherCats->clear();
      for( it = catList.begin(); it != catList.end(); ++it ) {
        ccID = it->section( " ##ID:", 1, 1 );
        cw.cbMainCat->addItem( it->section( " ##ID:", 0, 0 ),
                               QVariant( ccID ) );
        cw.lwOtherCats->addItem( it->section( " ##ID:", 0, 0 ) );
        if( ccID == "1" )
          uncategorized = itval;
        currentCategory = accountsDom.createElement( "category" );
        currentID = accountsDom.createElement( "categoryId" );
        currentID.appendChild( accountsDom.createTextNode( it->section( " ##ID:", 1 ) ) );
        currentName = accountsDom.createElement( "categoryName" );
        currentName.appendChild( accountsDom.createTextNode( it->section( " ##ID:", 0, 0 ) ) );
        currentCategory.appendChild( currentID );
        currentCategory.appendChild( currentName );
        newCategoriesElement.appendChild( currentCategory );
        itval++;
      }
      if( uncategorized )
        cw.cbMainCat->setCurrentIndex( uncategorized );
      if( currentBlogElement.firstChildElement( "categories" ).isNull() )
        currentBlogElement.appendChild( newCategoriesElement );
      else
        currentBlogElement.replaceChild( newCategoriesElement, currentBlogElement.firstChildElement( "categories" ) );

      cw.chNoCats->setEnabled( true );
      cw.cbMainCat->setEnabled( true );
      cw.lwOtherCats->setEnabled( true );
      handleEnableCategories();
      if( !noAutoSave ) {
        QFile domOut( PROPERSEPS( QString( "%1/qtmaccounts2.xml" ).arg( localStorageDirectory ) ) );
        if( domOut.open( QIODevice::WriteOnly ) ) {
          QTextStream domFileStream( &domOut );
          accountsDom.save( domFileStream, 2 );
          domOut.close();
          //qDebug() << "saved";
        }
        else
          statusWidget->showMessage( tr( "Could not write to accounts file (error %1)" ).arg( (int)domOut.error() ), 2000 );
      }
    }
  }

  addToConsole( accountsDom.toString( 2 ) );

  connect( this, SIGNAL( httpBusinessFinished() ),
           this, SIGNAL( categoryRefreshFinished() ) );

  if( QApplication::overrideCursor() != 0 )
    QApplication::restoreOverrideCursor();
}

void EditingWindow::mt_setPostCategories( QByteArray response )
{
  disconnect( this, SIGNAL( httpBusinessFinished() ), 0, 0 );

  QXmlInputSource xis;
  QXmlSimpleReader reader;
  XmlRpcHandler handler( currentHttpBusiness );
  QList<QString> parsedData;
  QString rdata( response );

  if( rdata.contains( "<fault>" ) ) {
    statusWidget->showMessage( tr( "Categories not set successfully; see console." ), 2000 );
    QTimer::singleShot( 1000, this, SLOT( hideProgressBar() ) );
  }
  else {
    statusWidget->showMessage( tr( "Categories set successfully." ), 2000 );

    if( ( location.contains( "mt-xmlrpc.cgi" ) || useWordpressAPI ) && cw.cbStatus->currentIndex() == 1 )
      connect( this, SIGNAL( httpBusinessFinished() ),
               this, SLOT( publishPost() ) );
  }
  addToConsole( rdata );
  QApplication::restoreOverrideCursor();
}

void EditingWindow::wp_getTags( QByteArray response )
{
  disconnect( this, SIGNAL( httpBusinessFinished() ), 0, 0 );
  addToConsole( response );

  QString responseString = QString::fromUtf8( response );
  QRegExp rx( "<member>.*>name<.*<string>(.*)</string>.*</member>" );
  rx.setMinimal( true );
  //rx.exactMatch( responseString ); */
  
  QStringList tagList;
  QString currentTag;
  QDomElement newTagElement, newTagListElement, oldTagListElement;
  /* bool receivingTagName = false;
  QXmlStreamReader::TokenType token;

  QXmlStreamReader xml( response );

  while( !xml.atEnd() && !xml.hasError() ) {
    token = xml.readNext();
    
    if( token == QXmlStreamReader::StartDocument )
      continue;

    if( token == QXmlStreamReader::StartElement ) {
      //qDebug() << "starting element";
      if( xml.name().toString() == "name" ) {
        //qDebug() << "reading a name";
        token == xml.readNext();
        if( token != QXmlStreamReader::Characters )
          continue;
        qDebug() << "reading characters";
        if( xml.text().toString() == "name" ) {
          qDebug() << "this is a name";
          receivingTagName = true;
          continue;
        }
      }

      if( xml.name().toString() == "string" && receivingTagName ) {
        token = xml.readNext();
        if( token != QXmlStreamReader::Characters )
          continue;
        currentTag = xml.text().toString();
        qDebug() << "retrieving tag";
        if( !tagList.contains( currentTag ) )
          tagList << currentTag;
        receivingTagName = false;
        continue;
      }
    }

  } */

  int pos = 0;
  while( (pos = rx.indexIn( responseString, pos )) != -1 ) {
    tagList += rx.cap( 1 );
    pos += rx.matchedLength();
  }

  if( tagList.count() > 0 ) {
    newTagListElement = accountsDom.createElement( "wpTags" );
    Q_FOREACH( QString text, tagList ) {
      newTagElement = accountsDom.createElement( "tag" );
      newTagElement.appendChild( accountsDom.createTextNode( text ) );
      newTagListElement.appendChild( newTagElement );
    }
    if( currentBlogElement.firstChildElement( "wpTags" ).isNull() ) {
      currentBlogElement.appendChild( newTagListElement );
      saveAccountsDom();
    }
    else {
      oldTagListElement = currentBlogElement.firstChildElement( "wpTags" );
      currentBlogElement.replaceChild( newTagListElement, oldTagListElement );
      saveAccountsDom();
    }

    cw.lwAvailKeywordTags->clear();
    cw.lwAvailKeywordTags->addItems( tagList );

    addToConsole( accountsDom.toString( 2 ) );

  }
  else
    statusWidget->showMessage( tr( "This blog has no tags." ), 2000 );

  addToConsole( QString( response ) );

  if( QApplication::overrideCursor() != 0 )
    QApplication::restoreOverrideCursor();
}

void EditingWindow::wp_getPosts_ID_Only( QByteArray response )
{
  disconnect( this, SIGNAL( httpBusinessFinished() ), 0, 0 );
  addToConsole( response );

  QString responseString( response );
  QStringList structs = responseString.split( QRegExp( "</?struct>" ) );

  QString post_id, post_content, this_post_ID, this_post_content;

  Q_FOREACH( QString this_struct, structs ) {
    if( this_struct.startsWith( "</value>" ) )
      continue;

    this_post_ID = this_struct.section( "<name>post_id</name><value><string>", 1, 1 )
      .section( "</string>", 0, 0 );
    this_post_content = this_struct.section( "<name>post_content</name><value><string>", 1, 1 )
      .section( "</string>", 0, 0 );
    qDebug() << "Post ID:" << this_post_ID;
    qDebug() << "Content:" << this_post_content.left( 50 ) << "..." << this_post_content.right( 50 );

    if( this_post_content.contains( entryHash ) ) {
      entryNumber = this_post_ID;
      statusWidget->showMessage( tr( "Post published successfully." ), 2000 );
      cw.progressBar->setValue( cw.progressBar->value() + 1 );
      if( QApplication::overrideCursor() != 0 )
        QApplication::restoreOverrideCursor();
      if( !entryEverSaved ) {
        if( postAsSave && cleanSave ) {
          setWindowModified( false );
          dirtyIndicator->hide();
          setDirtySignals( true );
        }
      }
      entryBlogged = true;

      return;
    }
  }
/*
  QXmlStreamReader::TokenType tokenType;
  QString currentElementName, currentText, postID, postContent;

  QXmlStreamReader streamReader( response );
  PostIDActivity currentActivity = NoActivity;
  while( !streamReader.atEnd() ) {
    tokenType = streamReader.readNext();
    switch( tokenType ) {
      case QXmlStreamReader::StartElement:
        currentElementName = streamReader.name().toString();
        if( currentElementName == "struct" ) {
          postID = "";
          postContent = "";
        }
        break;
      case QXmlStreamReader::Characters:
        currentText = streamReader.text().toString();
        if( currentElementName == "name" ) {
          if( currentText == "post_id" ) {
            currentActivity = GettingID;
            break;
          }
          if( currentText == "post_content" ) {
            currentActivity = GettingContent;
            break;
          }
        }
        if( currentElementName == "string" && currentActivity == GettingID ) {
          postID = currentText;
          currentActivity = NoActivity;
          break;
        }
        if( currentElementName == "string" && currentActivity == GettingContent ) {
          postContent = currentText;
          currentActivity = NoActivity;
          break;
        }
        break;
      case QXmlStreamReader::EndElement:
        currentElementName = streamReader.name().toString();
        if( currentElementName == "struct" ) {
          if( postContent.contains( entryHash ) ) {
            entryNumber = postID;
            cw.progressBar->setValue( cw.progressBar->value() + 1 );
            if( QApplication::overrideCursor() != 0 )
              QApplication::restoreOverrideCursor();
            if( !entryEverSaved ) {
              if( postAsSave && cleanSave ) {
                setWindowModified( false );
                dirtyIndicator->hide();
                setDirtySignals( true );
              }
            }
            
            return;
          }
        }
    }

  }*/
  statusWidget->showMessage( tr( "Could not get entry ID, but entry may have posted." ), 2000 );

  if( QApplication::overrideCursor() != 0 )
    QApplication::restoreOverrideCursor();
}

void EditingWindow::wp_newCategory( QByteArray response )
{
  disconnect( this, SIGNAL( httpBusinessFinished() ), 0, 0 );

  // Parse the XML
  QString responseString( response );
  QRegExp rx( "<methodResponse>\\s*<params>\\s*<param>\\s*<value>\\s*<int>([0-9]*)</int>\\s*</value>\\s*"
              "</param>\\s*</params>\\s*</methodResponse>" );
  if( rx.indexIn( response ) != -1 ) {
    cw.cbMainCat->clear();
    cw.lwOtherCats->clear();
    connect( this, SIGNAL( httpBusinessFinished() ),
             this, SLOT( refreshCategories() ) );
  }
  else
    statusWidget->showMessage( tr( "The request caused a fault; see console." ), 2000 );

  addToConsole( responseString );

  QApplication::restoreOverrideCursor();
}

void EditingWindow::wp_newPost( QByteArray response )
{
  metaWeblog_newPost( response );
}

void EditingWindow::wp_editPost( QByteArray response )
{
  metaWeblog_editPost( response );
}

