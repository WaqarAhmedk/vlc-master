/*****************************************************************************
 * roundimage.hpp: Custom widgets
 ****************************************************************************
 * Copyright (C) 2021 the VideoLAN team
 *
 * Authors: Prince Gupta <guptaprince8832@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_QT_ROUNDIMAGE_HPP
#define VLC_QT_ROUNDIMAGE_HPP

#include "qt.hpp"

#include "util/asynctask.hpp"

#include <QImage>
#include <QQuickItem>
#include <QUrl>

class RoundImage : public QQuickItem
{
    Q_OBJECT

    // url of the image
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged FINAL)

    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged FINAL)

public:
    RoundImage(QQuickItem *parent = nullptr);

    void componentComplete() override;

    QUrl source() const;
    qreal radius() const;

public slots:
    void setSource(const QUrl& source);
    void setRadius(qreal radius);

signals:
    void sourceChanged(const QUrl&);
    void radiusChanged(qreal);

protected:
    void itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &value) override;
    QSGNode* updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *) override;

private:
    class RoundImageGenerator : public AsyncTask<QImage>
    {
    public:
        RoundImageGenerator(const QUrl &source, qreal width, qreal height, qreal radius);

        QImage execute();

    private:
        QUrl source;
        qreal width;
        qreal height;
        qreal radius;
    };

    void setDPR(qreal value);
    void regenerateRoundImage();

    QUrl m_source;
    qreal m_radius = 0.0;
    qreal m_dpr = 1.0; // device pixel ratio

    QImage m_roundImage;
    bool m_dirty = false;

    TaskHandle<RoundImageGenerator> m_roundImageGenerator {};

    bool m_enqueuedGeneration = false;
};

#endif

