/*
 * SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "kdynamicwallpaper_export.h"

#include <QByteArray>
#include <QJsonObject>
#include <QSharedDataPointer>

class KDynamicWallpaperMetaDataPrivate;

class KDYNAMICWALLPAPER_EXPORT KDynamicWallpaperMetaData
{
public:
    enum CrossFadeMode {
        NoCrossFade,
        CrossFade,
    };

    enum MetaDataField {
        CrossFadeField = 1 << 0,
        TimeField = 1 << 1,
        SolarAzimuthField = 1 << 2,
        SolarElevationField = 1 << 3,
        IndexField = 1 << 4,
    };
    Q_DECLARE_FLAGS(MetaDataFields, MetaDataField)

    KDynamicWallpaperMetaData();
    KDynamicWallpaperMetaData(const KDynamicWallpaperMetaData &other);
    ~KDynamicWallpaperMetaData();

    KDynamicWallpaperMetaData &operator=(const KDynamicWallpaperMetaData &other);

    MetaDataFields fields() const;
    bool isValid() const;

    void setCrossFadeMode(CrossFadeMode mode);
    CrossFadeMode crossFadeMode() const;

    void setTime(qreal time);
    qreal time() const;

    void setSolarElevation(qreal elevation);
    qreal solarElevation() const;

    void setSolarAzimuth(qreal azimuth);
    qreal solarAzimuth() const;

    void setIndex(int index);
    int index() const;

    QJsonObject toJson() const;

    static KDynamicWallpaperMetaData fromJson(const QJsonObject &object);

private:
    QSharedDataPointer<KDynamicWallpaperMetaDataPrivate> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(KDynamicWallpaperMetaData::MetaDataFields)
