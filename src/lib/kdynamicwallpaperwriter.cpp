/*
 * SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "kdynamicwallpaperwriter.h"
#include "kdynamicwallpapermetadata.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QImage>
#include <QThread>

#include <avif/avif.h>

/*!
 * \class KDynamicWallpaperWriter
 * \brief The KDynamicWallpaperWriter class provides a convenient way for writing dynamic
 * wallpapers.
 *
 * If any error occurs when writing an image, write() will return false. You can then call
 * error() to find the type of the error that occurred, or errorString() to get a human
 * readable description of what went wrong.
 */

class KDynamicWallpaperWriterPrivate
{
public:
    KDynamicWallpaperWriterPrivate();

    void flush(QIODevice *device);

    KDynamicWallpaperWriter::WallpaperWriterError wallpaperWriterError;
    QString errorString;
    QList<QImage> images;
    QList<KDynamicWallpaperMetaData> metaData;
};

KDynamicWallpaperWriterPrivate::KDynamicWallpaperWriterPrivate()
    : wallpaperWriterError(KDynamicWallpaperWriter::NoError)
{
}

static QByteArray serializeMetaData(const QList<KDynamicWallpaperMetaData> &metaData)
{
    QJsonArray array;
    for (const KDynamicWallpaperMetaData &md : metaData)
        array.append(md.toJson());

    QJsonDocument document;
    document.setArray(array);

    const QByteArray base64 = document.toJson(QJsonDocument::Compact).toBase64();
    QFile templateFile(QStringLiteral(":/kdynamicwallpaper/xmp/metadata.xml"));
    templateFile.open(QFile::ReadOnly);

    QByteArray xmp = templateFile.readAll();
    xmp.replace(QByteArrayLiteral("base64"), base64);
    return xmp;
}

void KDynamicWallpaperWriterPrivate::flush(QIODevice *device)
{
    const QByteArray xmp = serializeMetaData(metaData);
    avifEncoder *encoder = avifEncoderCreate();
    encoder->maxThreads = QThread::idealThreadCount();

    QList<avifImage *> avifImages;
    for (const QImage &image : images) {
        avifImage *avif = avifImageCreate(image.width(), image.height(), 8, AVIF_PIXEL_FORMAT_YUV444);
        avifImageSetMetadataXMP(avif, reinterpret_cast<const uint8_t *>(xmp.constData()), xmp.size());

        avifRGBImage rgb;
        avifRGBImageSetDefaults(&rgb, avif);

        rgb.format = AVIF_RGB_FORMAT_RGB;
        rgb.depth = 8;
        rgb.rowBytes = image.bytesPerLine();
        rgb.pixels = const_cast<uint8_t *>(image.constBits());

        // TODO: color space

        avifResult result = avifImageRGBToYUV(avif, &rgb);
        if (result != AVIF_RESULT_OK) {
            avifImageDestroy(avif);
            continue;
        }

        result = avifEncoderAddImage(encoder, avif, 0, AVIF_ADD_IMAGE_FLAG_NONE);
        if (result != AVIF_RESULT_OK) {
            avifImageDestroy(avif);
            continue;
        }

        avifImages.append(avif);
    }

    avifRWData output = AVIF_DATA_EMPTY;
    avifResult result = avifEncoderFinish(encoder, &output);
    if (result == AVIF_RESULT_OK) {
        device->write(reinterpret_cast<const char *>(output.data), output.size);
    } else {
        wallpaperWriterError = KDynamicWallpaperWriter::EncoderError;
        errorString = avifResultToString(result);
    }

    avifRWDataFree(&output);
    avifEncoderDestroy(encoder);
    for (avifImage *image : avifImages)
        avifImageDestroy(image);
}

/*!
 * Constructs an empty KDynamicWallpaperWriter object.
 */
KDynamicWallpaperWriter::KDynamicWallpaperWriter()
    : d(new KDynamicWallpaperWriterPrivate)
{
}

/*!
 * Destructs the KDynamicWallpaperWriter object.
 */
KDynamicWallpaperWriter::~KDynamicWallpaperWriter()
{
}

void KDynamicWallpaperWriter::setMetaData(const QList<KDynamicWallpaperMetaData> &metaData)
{
    d->metaData = metaData;
}

QList<KDynamicWallpaperMetaData> KDynamicWallpaperWriter::metaData() const
{
    return d->metaData;
}

void KDynamicWallpaperWriter::setImages(const QList<QImage> &images)
{
    QList<QImage> tmpImages;
    tmpImages.reserve(images.count());
    for (const QImage &image : images)
        tmpImages.append(image.convertToFormat(QImage::Format_RGB888));

    d->images = tmpImages;
}

QList<QImage> KDynamicWallpaperWriter::images() const
{
    return d->images;
}

/*!
 * Begins a write sequence to the device and returns \c true if successful; otherwise \c false is
 * returned. You must call this method before calling write() method.
 *
 * If the device is not already open, KDynamicWallpaperWriter will attempt to open the device
 * in QIODevice::WriteOnly mode by calling open().
 */
bool KDynamicWallpaperWriter::flush(QIODevice *device)
{
    if (device->isOpen()) {
        if (!(device->openMode() & QIODevice::WriteOnly)) {
            d->wallpaperWriterError = KDynamicWallpaperWriter::DeviceError;
            d->errorString = QStringLiteral("The device is not open for writing");
            return false;
        }
    } else {
        if (!device->open(QIODevice::WriteOnly)) {
            d->wallpaperWriterError = KDynamicWallpaperWriter::DeviceError;
            d->errorString = device->errorString();
            return false;
        }
    }

    d->flush(device);
    return true;
}

/*!
 * Begins a write sequence to the file \p fileName and returns \c true if successful; otherwise
 * \c false is returned. Internally, KDynamicWallpaperWriter will create a QFile object and open
 * it in QIODevice::WriteOnly mode, and use it when writing dynamic wallpapers.
 *
 * You must call this method before calling write() method.
 */
bool KDynamicWallpaperWriter::flush(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        d->wallpaperWriterError = KDynamicWallpaperWriter::DeviceError;
        d->errorString = file.errorString();
        return false;
    }

    d->flush(&file);
    return true;
}

/*!
 * Returns the type of the last error that occurred.
 */
KDynamicWallpaperWriter::WallpaperWriterError KDynamicWallpaperWriter::error() const
{
    return d->wallpaperWriterError;
}

/*!
 * Returns the human readable description of the last error that occurred.
 */
QString KDynamicWallpaperWriter::errorString() const
{
    if (d->wallpaperWriterError == NoError)
        return QStringLiteral("No error");
    return d->errorString;
}

/*!
 * Returns \c true if a dynamic wallpaper can be written to the specified @device; otherwise \c false
 * is returned.
 */
bool KDynamicWallpaperWriter::canWrite(QIODevice *device)
{
    return device->isWritable();
}

/*!
 * Returns \c true if a dynamic wallpaper can be written to a file with the specified file name
 * \p fileName; otherwise \c false is returned.
 */
bool KDynamicWallpaperWriter::canWrite(const QString &fileName)
{
    QFile file(fileName);
    return file.isWritable();
}
