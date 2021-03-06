/***************************************************************************
    qgstexteditwrapper.cpp
     --------------------------------------
    Date                 : 5.1.2014
    Copyright            : (C) 2014 Matthias Kuhn
    Email                : matthias at opengis dot ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgstexteditwrapper.h"

#include "qgsfield.h"
#include "qgsfieldvalidator.h"
#include "qgsfilterlineedit.h"

#include <QSettings>

QgsTextEditWrapper::QgsTextEditWrapper( QgsVectorLayer* vl, int fieldIdx, QWidget* editor, QWidget* parent )
    : QgsEditorWidgetWrapper( vl, fieldIdx, editor, parent )
    , mTextEdit( nullptr )
    , mPlainTextEdit( nullptr )
    , mLineEdit( nullptr )
{
}

QVariant QgsTextEditWrapper::value() const
{
  QString v;

  if ( mTextEdit )
  {
    if ( config( "UseHtml" ).toBool() )
    {
      v = mTextEdit->toHtml();
    }
    else
    {
      v = mTextEdit->toPlainText();
    }
  }

  if ( mPlainTextEdit )
  {
    v = mPlainTextEdit->toPlainText();
  }

  if ( mLineEdit )
  {
    v = mLineEdit->text();
  }

  if (( v.isEmpty() && ( field().type() == QVariant::Int || field().type() == QVariant::Double || field().type() == QVariant::LongLong || field().type() == QVariant::Date ) ) ||
      v == QSettings().value( "qgis/nullValue", "NULL" ).toString() )
    return QVariant( field().type() );

  if ( !defaultValue().isNull() && v == defaultValue().toString() )
  {
    return defaultValue();
  }

  QVariant res( v );
  if ( field().convertCompatible( res ) )
  {
    return res;
  }
  else if ( field().type() == QVariant::String && field().length() > 0 )
  {
    // for string fields convertCompatible may return false due to field length limit - in this case just truncate
    // input rather then discarding it entirely
    return QVariant( v.left( field().length() ) );
  }
  else
  {
    return QVariant( field().type() );
  }
}

QWidget* QgsTextEditWrapper::createWidget( QWidget* parent )
{
  if ( config( "IsMultiline" ).toBool() )
  {
    if ( config( "UseHtml" ).toBool() )
    {
      return new QTextEdit( parent );
    }
    else
    {
      return new QPlainTextEdit( parent );
    }
  }
  else
  {
    return new QgsFilterLineEdit( parent );
  }
}

void QgsTextEditWrapper::initWidget( QWidget* editor )
{
  mTextEdit = qobject_cast<QTextEdit*>( editor );
  mPlainTextEdit = qobject_cast<QPlainTextEdit*>( editor );
  mLineEdit = qobject_cast<QLineEdit*>( editor );

  if ( mTextEdit )
    connect( mTextEdit, SIGNAL( textChanged() ), this, SLOT( valueChanged() ) );

  if ( mPlainTextEdit )
    connect( mPlainTextEdit, SIGNAL( textChanged() ), this, SLOT( valueChanged() ) );

  if ( mLineEdit )
  {
    mLineEdit->setValidator( new QgsFieldValidator( mLineEdit, field(), defaultValue().toString() ) );

    QVariant defVal = defaultValue();
    if ( defVal.isNull() )
    {
      defVal = QSettings().value( "qgis/nullValue", "NULL" );
    }

    QgsFilterLineEdit *fle = qobject_cast<QgsFilterLineEdit*>( mLineEdit );
    if ( field().type() == QVariant::Int || field().type() == QVariant::Double || field().type() == QVariant::LongLong || field().type() == QVariant::Date )
    {
      mPlaceholderText = defVal.toString();
      mLineEdit->setPlaceholderText( mPlaceholderText );
    }
    else if ( fle )
    {
      fle->setNullValue( defVal.toString() );
    }

    connect( mLineEdit, SIGNAL( textChanged( QString ) ), this, SLOT( valueChanged( QString ) ) );
    connect( mLineEdit, SIGNAL( textChanged( QString ) ), this, SLOT( textChanged( QString ) ) );

    mWritablePalette = mLineEdit->palette();
    mReadOnlyPalette = mLineEdit->palette();
    mReadOnlyPalette.setColor( QPalette::Text, mWritablePalette.color( QPalette::Disabled, QPalette::Text ) );
  }
}

bool QgsTextEditWrapper::valid() const
{
  return mLineEdit || mTextEdit || mPlainTextEdit;
}

void QgsTextEditWrapper::showIndeterminateState()
{
  //note - this is deliberately a zero length string, not a null string!
  if ( mTextEdit )
    mTextEdit->blockSignals( true );
  if ( mPlainTextEdit )
    mPlainTextEdit->blockSignals( true );
  if ( mLineEdit )
  {
    mLineEdit->blockSignals( true );
    // for interdeminate state we need to clear the placeholder text - we want an empty line edit, not
    // one showing the default value (eg "NULL")
    mLineEdit->setPlaceholderText( QString() );
  }

  setWidgetValue( QString( "" ) );

  if ( mTextEdit )
    mTextEdit->blockSignals( false );
  if ( mPlainTextEdit )
    mPlainTextEdit->blockSignals( false );
  if ( mLineEdit )
    mLineEdit->blockSignals( false );
}

void QgsTextEditWrapper::setValue( const QVariant& val )
{
  if ( mLineEdit )
  {
    //restore placeholder text, which may have been removed by showIndeterminateState()
    mLineEdit->setPlaceholderText( mPlaceholderText );
  }
  setWidgetValue( val );
}

void QgsTextEditWrapper::setEnabled( bool enabled )
{
  if ( mTextEdit )
    mTextEdit->setReadOnly( !enabled );

  if ( mPlainTextEdit )
    mPlainTextEdit->setReadOnly( !enabled );

  if ( mLineEdit )
  {
    mLineEdit->setReadOnly( !enabled );
    if ( enabled )
      mLineEdit->setPalette( mWritablePalette );
    else
      mLineEdit->setPalette( mReadOnlyPalette );
  }
}

void QgsTextEditWrapper::textChanged( const QString& )
{
  if ( mLineEdit )
  {
    //restore placeholder text, which may have been removed by showIndeterminateState()
    mLineEdit->setPlaceholderText( mPlaceholderText );
  }
}

void QgsTextEditWrapper::setWidgetValue( const QVariant& val )
{
  QString v;
  if ( val.isNull() )
  {
    if ( !( field().type() == QVariant::Int || field().type() == QVariant::Double || field().type() == QVariant::LongLong || field().type() == QVariant::Date ) )
      v = QSettings().value( "qgis/nullValue", "NULL" ).toString();
  }
  else
    v = val.toString();

  if ( mTextEdit )
  {
    if ( val != value() )
    {
      if ( config( "UseHtml" ).toBool() )
        mTextEdit->setHtml( v );
      else
        mTextEdit->setPlainText( v );
    }
  }

  if ( mPlainTextEdit )
  {
    if ( val != value() )
      mPlainTextEdit->setPlainText( v );
  }

  if ( mLineEdit )
    mLineEdit->setText( v );
}
