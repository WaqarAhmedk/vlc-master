/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "covergenerator.hpp"

// VLC includes
#include "qt.hpp"

// MediaLibrary includes
#include "medialibrary/mlhelper.hpp"

// Qt includes
#include <QDir>
#include <QImageReader>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsBlurEffect>
#include <QUrl>
#include <QQmlFile>

// Qt private exported function
QT_BEGIN_NAMESPACE
extern void VLC_WEAK qt_blurImage(QImage &blurImage, qreal radius, bool quality, int transposed = 0);
QT_END_NAMESPACE

//-------------------------------------------------------------------------------------------------
// Static variables

static const QString COVERGENERATOR_STORAGE = "/art/qt-covers";

static const int COVERGENERATOR_COUNT = 2;

static const QString COVERGENERATOR_DEFAULT = ":/noart_albumCover.svg";

//-------------------------------------------------------------------------------------------------
// Ctor / dtor
//-------------------------------------------------------------------------------------------------

CoverGenerator::CoverGenerator(vlc_medialibrary_t * ml, const MLItemId & itemId)
    : m_ml(ml)
    , m_id(itemId)
    , m_countX(COVERGENERATOR_COUNT)
    , m_countY(COVERGENERATOR_COUNT)
    , m_split(Divide)
    , m_smooth(true)
    , m_blur(0)
    , m_default(COVERGENERATOR_DEFAULT) {}

//-------------------------------------------------------------------------------------------------
// Interface
//-------------------------------------------------------------------------------------------------

MLItemId CoverGenerator::getId()
{
    return m_id;
}

//-------------------------------------------------------------------------------------------------

void CoverGenerator::setSize(const QSize & size)
{
    m_size = size;
}

void CoverGenerator::setCountX(int x)
{
    m_countX = x;
}

void CoverGenerator::setCountY(int y)
{
    m_countY = y;
}

void CoverGenerator::setSplit(Split split)
{
    m_split = split;
}

void CoverGenerator::setSmooth(bool enabled)
{
    m_smooth = enabled;
}

void CoverGenerator::setBlur(int radius)
{
    m_blur = radius;
}

void CoverGenerator::setDefaultThumbnail(const QString & fileName)
{
    m_default = fileName;
}

void CoverGenerator::setPrefix(const QString & prefix)
{
    m_prefix = prefix;
}

bool CoverGenerator::cachedFileAvailable() const
{
    return QFile::exists(fileName());
}

QString CoverGenerator::cachedFileURL() const
{
    return QUrl::fromLocalFile(fileName()).toString();
}

QString CoverGenerator::fileName() const
{
    QDir dir(config_GetUserDir(VLC_CACHE_DIR) + COVERGENERATOR_STORAGE);
    return dir.absoluteFilePath(QString("%1_thumbnail_%2_%3x%4.jpg")
                                .arg((m_prefix.isEmpty() ? getPrefix(m_id.type) : m_prefix)
                                     , QString::number(m_id.id)
                                     , QString::number(m_size.width())
                                     , QString::number(m_size.height())));
}

//-------------------------------------------------------------------------------------------------
// QRunnable implementation
//-------------------------------------------------------------------------------------------------

QString CoverGenerator::execute() /* override */
{
    QDir dir(config_GetUserDir(VLC_CACHE_DIR) + COVERGENERATOR_STORAGE);

    dir.mkpath(dir.absolutePath());

    vlc_ml_parent_type type = m_id.type;

    int64_t id = m_id.id;

    QString fileName = this->fileName();
    if (dir.exists(fileName))
    {
        return QUrl::fromLocalFile(fileName).toString();
    }

    QStringList thumbnails;

    int count = m_countX * m_countY;

    if (type == VLC_ML_PARENT_GENRE)
        thumbnails = getGenre(count, id);
    else
        thumbnails = getMedias(count, id, type);

    int countX;
    int countY;

    if (thumbnails.isEmpty())
    {
        if (m_split == CoverGenerator::Duplicate)
        {
            while (thumbnails.count() != count)
            {
                thumbnails.append(m_default);
            }

            countX = m_countX;
            countY = m_countY;
        }
        else
        {
            thumbnails.append(m_default);

            countX = 1;
            countY = 1;
        }
    }
    else if (m_split == CoverGenerator::Duplicate)
    {
        int index = 0;

        while (thumbnails.count() != count)
        {
            thumbnails.append(thumbnails.at(index));

            index++;
        }

        countX = m_countX;
        countY = m_countY;
    }
    else // if (m_split == CoverGenerator::Divide)
    {
        countX = m_countX;

        // NOTE: We try to divide thumbnails as far as we can based on their total count.
        countY = std::ceil((qreal) thumbnails.count() / m_countX);
    }

    QImage image(m_size, QImage::Format_RGB32);

    image.fill(Qt::white);

    QPainter painter;

    painter.begin(&image);

    draw(painter, thumbnails, countX, countY);

    painter.end();

    if (m_blur > 0)
        blur(image);

    image.save(fileName, "jpg");

    return QUrl::fromLocalFile(fileName).toString();
}

//-------------------------------------------------------------------------------------------------
// Private functions
//-------------------------------------------------------------------------------------------------

void CoverGenerator::draw(QPainter & painter,
                          const QStringList & fileNames, int countX, int countY)
{
    int count = fileNames.count();

    int width  = m_size.width()  / countX;
    int height = m_size.height() / countY;

    for (int y = 0; y < countY; y++)
    {
        for (int x = 0; x < countX; x++)
        {
            int index = countX * y + x;

            if (index == count) return;

            QRect rect;

            // NOTE: This handles the wider thumbnail case (e.g. for a 2x1 grid).
            if (index == count - 1 && x != countX - 1)
            {
                rect = QRect(width * x, height * y, width * countX - x, height);
            }
            else
                rect = QRect(width * x, height * y, width, height);

            QString fileName = fileNames.at(index);

            if (fileName.isEmpty())
                drawImage(painter, m_default, rect);
            else
                drawImage(painter, fileName, rect);
        }
    }
}

void CoverGenerator::drawImage(QPainter & painter, const QString & fileName, const QRect & target)
{
    //QFile expect the :/ instead of qrc:// for resources files
    const QUrl fileURL {fileName};
    const QString adaptedFilename = QQmlFile::urlToLocalFileOrQrc(fileURL);
    QFile file(adaptedFilename.isEmpty() ? fileName : adaptedFilename);

    if (file.open(QIODevice::ReadOnly) == false)
    {
        // NOTE: This image does not seem valid so we paint the placeholder instead.
        if (fileName != m_default)
            drawImage(painter, m_default, target);

        return;
    }

    QImageReader reader(&file);

    if (reader.canRead() == false)
        return;

    QSize size = reader.size().scaled(target.width(),
                                      target.height(), Qt::KeepAspectRatioByExpanding);

    QImage image;

    if (fileName.endsWith(".svg", Qt::CaseInsensitive))
    {
        if (size.isEmpty() == false)
        {
            reader.setScaledSize(size);
        }

        if (reader.read(&image) == false)
            return;
    }
    else
    {
        if (reader.read(&image) == false)
            return;

        if (size.isEmpty() == false)
        {
            // NOTE: Should we use Qt::SmoothTransformation or favor efficiency ?
            if (m_smooth)
                image = image.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            else
                image = image.scaled(size, Qt::IgnoreAspectRatio);
        }
    }

    int x = (image.width () - target.width ()) / 2;
    int y = (image.height() - target.height()) / 2;

    QRect source(x, y, target.width(), target.height());

    painter.drawImage(target, image, source);
}

//-------------------------------------------------------------------------------------------------

void CoverGenerator::blur(QImage& image)
{
    if (Q_LIKELY(&qt_blurImage))
    {
        // A symbol is available for qt_blurImage()
        // Exported function can be used directly within a separate thread:
        qt_blurImage(image, 2.5 * (m_blur + 1), true);
    }
    else
    {
        const auto blurImage = [&]() {
            QGraphicsScene scene;

            QGraphicsPixmapItem item(QPixmap::fromImage(image));

            QGraphicsBlurEffect effect;

            effect.setBlurRadius(m_blur);

            effect.setBlurHints(QGraphicsBlurEffect::QualityHint);

            item.setGraphicsEffect(&effect);

            scene.addItem(&item);

            QPainter painter(&image);

            scene.render(&painter);
        };

        if (qApp->thread() == QThread::currentThread())
        {
            blurImage();
        }
        else
        {
            // Not executing in Qt GUI thread, this is not supported.
            // Block this thread, and blur the image in the GUI thread instead:
            QMetaObject::invokeMethod(qApp, blurImage, Qt::BlockingQueuedConnection);
        }
    }
}

//-------------------------------------------------------------------------------------------------

QString CoverGenerator::getPrefix(vlc_ml_parent_type type) const
{
    switch (type)
    {
        case VLC_ML_PARENT_GENRE:
            return "genre";
        case VLC_ML_PARENT_GROUP:
            return "group";
        case VLC_ML_PARENT_FOLDER:
            return "folder";
        case VLC_ML_PARENT_PLAYLIST:
            return "playlist";
        default:
            return "unknown";
    }
}

//-------------------------------------------------------------------------------------------------

QStringList CoverGenerator::getGenre(int count, int64_t id) const
{
    QStringList thumbnails;

    vlc_ml_query_params_t params;

    memset(&params, 0, sizeof(vlc_ml_query_params_t));

    // NOTE: We retrieve twice the count to maximize our chances to get a valid thumbnail.
    params.i_nbResults = count * 2;

    ml_unique_ptr<vlc_ml_album_list_t> list(vlc_ml_list_genre_albums(m_ml, &params, id));

    for (const vlc_ml_album_t & album : ml_range_iterate<vlc_ml_album_t>(list))
    {
        if (album.thumbnails[VLC_ML_THUMBNAIL_SMALL].i_status != VLC_ML_THUMBNAIL_STATUS_AVAILABLE)
            continue;

        QUrl url(album.thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl);

        // NOTE: We only want local files to compose the cover.
        if (url.isLocalFile() == false)
            continue;

        thumbnails.append(url.toLocalFile());

        if (thumbnails.count() == count)
            return thumbnails;
    }

    return thumbnails;
}

QStringList CoverGenerator::getMedias(int count, int64_t id, vlc_ml_parent_type type) const
{
    QStringList thumbnails;

    vlc_ml_query_params_t params;

    memset(&params, 0, sizeof(vlc_ml_query_params_t));

    // NOTE: We retrieve twice the count to maximize our chances to get a valid thumbnail.
    params.i_nbResults = count * 2;

    ml_unique_ptr<vlc_ml_media_list_t> list(vlc_ml_list_media_of(m_ml, &params, type, id));

    for (const vlc_ml_media_t & media : ml_range_iterate<vlc_ml_media_t>(list))
    {
        if (media.thumbnails[VLC_ML_THUMBNAIL_SMALL].i_status != VLC_ML_THUMBNAIL_STATUS_AVAILABLE)
            continue;

        QUrl url(media.thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl);

        // NOTE: We only want local files to compose the cover.
        if (url.isLocalFile() == false)
            continue;

        thumbnails.append(url.toLocalFile());

        if (thumbnails.count() == count)
            return thumbnails;
    }

    return thumbnails;
}
