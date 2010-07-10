/*
    Copyright (C) 2010 Casey Link <unnamedrambler@gmail.com>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU Library General Public License as published by
    the Free Software Foundation; either version 3 of the License, or (at your
    option) any later version.

    This library is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
    License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to the
    Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301, USA.
*/

#include "ImageModel.h"
extern "C" {
    #include "npiet.h"
}

#include <QtGui>
#include <QDebug>

ImageModel::ImageModel( QObject *parent ) :
        QAbstractTableModel( parent ), mPixelSize( 1 )
{
}

ImageModel::~ImageModel()
{

}

// helper function to check codel size
void
cc_check (int i, QRgb c, QRgb *last_c, int *last_p, int *min_w)
{
  if (i == 0) {
    *last_c = c;
    *last_p = i;
  } else if (*last_c != c) {
    int w = i - *last_p;
    if (w < *min_w) {
      *min_w = w;
    }
    *last_c = c;
    *last_p = i;
  }
}

void ImageModel::setImage( const QImage& _image, int codel_size )
{
    QImage image = _image.convertToFormat(QImage::Format_ARGB32);
    if( codel_size < 0 ) {
        /* Begin: modified part taken from npiet.c, cleanup_input () */
        // (C) 2010 Erik Schoenfelder <schoenfr@web.de>
        int i, j, last_p;
        QRgb last_cc;
        int min_w = image.width() + 1;
        int min_ww = image.width() + 1;
        int *o_cells;

        QRgb *cells = (QRgb*) image.bits();
        // scan image to guess codel size
        /* left to right: */
        for (j = 0; j < image.height(); j++) {
            for (i = 0; i < image.width(); i++) {
                cc_check (i, cells [j * image.width() + i], &last_cc, &last_p, &min_w);
            }
            cc_check (i, c_mark_index, &last_cc, &last_p, &min_w);
        }
        qDebug() << "his min_w" << min_w;
        /* top to bottom: */
        for (i = 0; i < image.width(); i++) {
            for (j = 0; j < image.height(); j++) {
                cc_check (j, cells [j * image.width() + i], &last_cc, &last_p, &min_w);
            }
            cc_check (j, c_mark_index, &last_cc, &last_p, &min_w);
        }
        codel_size = min_w;
        /* End: part taken from npiet.c, cleanup_input () */

        qDebug() << "Guessed codel size: " << codel_size;
    }
    // scale image so 1 codel == 1 pixel
    if( codel_size > 1 ) {
      int width = image.width() / codel_size;
      int height = image.height() / codel_size;
      mImage = image.scaled( width, height );
    } else
      mImage = image;
    reset();
}

QImage ImageModel::image() const
{
    return mImage;
}

int ImageModel::columnCount( const QModelIndex& parent ) const
{
    Q_UNUSED( parent )
    return mImage.width();
}

int ImageModel::rowCount( const QModelIndex& parent ) const
{
    Q_UNUSED( parent )
    return mImage.height();
}


QVariant ImageModel::data( const QModelIndex& index, int role ) const
{
    if ( !index.isValid() )
        return QVariant();

    switch( role ) {
      case Qt::DisplayRole: {
        QColor c;
        c.setRgb( mImage.pixel( index.column(), index.row() ) );
        return c;
      }
      case Qt::StatusTipRole:
        return statusString( index );
      default:
        return QVariant();
    }
}

QVariant ImageModel::headerData( int section, Qt::Orientation orientation, int role ) const
{
    Q_UNUSED( orientation )
    switch ( role ) {
    case Qt::SizeHintRole:
        return QSize( mPixelSize, mPixelSize );
    case Qt::DisplayRole:
        return QString::number( section );
    default:
        return QVariant();
    }
}

bool ImageModel::setData( const QModelIndex& index, const QVariant& value, int role )
{
    if ( role != Qt::DisplayRole )
        return false;
    if ( !value.canConvert<QColor>() )
        return false;
    QColor c = value.value<QColor>();
    mImage.setPixel( index.column(), index.row(), c.rgb() );
    emit dataChanged( index, index );
    return true;
}


void ImageModel::slotPixelSizeChange( int size )
{
    mPixelSize = size;
}

QString ImageModel::statusString(QModelIndex index) const
{
    QString coords;
    coords = QString( "X: %1 Y: %2" ).arg( index.column(), 3 ).arg( index.row(), 3 );

    int connected = contiguousBlocks( index.column(), index.row() );

    return QString("%1, contiguous: %2").arg(coords).arg(connected);
}

int ImageModel::contiguousBlocks( int x, int y ) const
{
      if( x < 0 || x >= mImage.width() || y < 0 || y >= mImage.height() )
          return 0;

      // array used to mark pixels as visited.
      // the image is mapped in row major fashion
      QBitArray markedArray( mImage.width() * mImage.height() );

      markedArray[x*mImage.height()+y] = 1;
      QRgb color = mImage.pixel(x, y);
      int result = 1; // 1 for the current block
      result += contiguousBlocks( x + 1, y, color, markedArray );
      result += contiguousBlocks( x - 1, y, color, markedArray );
      result += contiguousBlocks( x, y + 1, color, markedArray );
      result += contiguousBlocks( x, y - 1, color, markedArray );
      return result;
}

int ImageModel::contiguousBlocks( int x, int y, QRgb color, QBitArray &markedArray ) const
{
      if( x < 0 || x >= mImage.width() || y < 0 || y >= mImage.height() )
          return 0;
      if( markedArray[x*mImage.height()+y] || mImage.pixel( x, y ) != color )
          return 0;

      markedArray[x*mImage.height()+y] = 1;
      int result = 1; // 1 for the current block
      result += contiguousBlocks( x + 1, y, color, markedArray );
      result += contiguousBlocks( x - 1, y, color, markedArray );
      result += contiguousBlocks( x, y + 1, color, markedArray );
      result += contiguousBlocks( x, y - 1, color, markedArray );
      return result;
}



#include "ImageModel.moc"
