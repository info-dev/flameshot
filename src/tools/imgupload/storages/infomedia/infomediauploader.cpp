// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "infomediauploader.h"
#include "src/utils/confighandler.h"
#include "src/utils/filenamehandler.h"
#include "src/utils/history.h"
#include "src/widgets/loadspinner.h"
#include "src/widgets/notificationwidget.h"
#include <QBuffer>
#include <QDesktopServices>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QShortcut>
#include <cstdio>
#include <ios>
#include <qstringliteral.h>
#include <qurlquery.h>
#include <qvariant.h>

InfomediaUploader::InfomediaUploader(const QPixmap& capture, QWidget* parent)
  : ImgUploaderBase(capture, parent)
{
    m_NetworkAM = new QNetworkAccessManager(this);
    connect(m_NetworkAM,
            &QNetworkAccessManager::finished,
            this,
            &InfomediaUploader::handleReply);
}

void InfomediaUploader::handleReply(QNetworkReply* reply)
{
    spinner()->deleteLater();
    m_currentImageName.clear();

    if (reply->error() == QNetworkReply::NoError) {
        QString url = reply->readAll();
        setImageURL(url);

        // save history
        m_currentImageName = imageURL().toString();
        int lastSlash = m_currentImageName.lastIndexOf("/");
        if (lastSlash >= 0) {
            m_currentImageName = m_currentImageName.mid(lastSlash + 1);
        }

        // save image to history
        History history;
        m_currentImageName = history.packFileName(
          "infomedia", ConfigHandler().infomediaUserHash(), m_currentImageName);
        history.save(pixmap(), m_currentImageName);

        emit uploadOk(imageURL());
    } else {
        setInfoLabelText(reply->errorString());
    }

    new QShortcut(Qt::Key_Escape, this, SLOT(close()));
}

void InfomediaUploader::upload()
{
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    pixmap().save(&buffer, "PNG");

    QUrl url(QStringLiteral(INFOMEDIA_API_URL));
    QHttpMultiPart* http = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart reqTypePart;
    reqTypePart.setHeader(QNetworkRequest::ContentTypeHeader,
                          QVariant("text/plain"));
    reqTypePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                          QVariant("form-data; name=\"reqtype\""));
    reqTypePart.setBody("fileupload");
    http->append(reqTypePart);

    QHttpPart userPart;
    userPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"userhash\""));
    userPart.setHeader(QNetworkRequest::ContentTypeHeader,
                       QVariant("text/plain"));
    userPart.setBody(ConfigHandler().infomediaUserHash().toUtf8());
    http->append(userPart);

    QHttpPart secretPart;
    secretPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                         QVariant("form-data; name=\"secret\""));
    secretPart.setHeader(QNetworkRequest::ContentTypeHeader,
                         QVariant("text/plain"));
    secretPart.setBody(ConfigHandler().infomediaApiToken().toUtf8());
    http->append(secretPart);

    QHttpPart filePart;
    filePart.setHeader(
      QNetworkRequest::ContentDispositionHeader,
      QVariant("form-data; name=\"file\"; filename=\"upload.png\""));
    filePart.setBody(byteArray);
    http->append(filePart);

    QNetworkRequest request(url);
    request.setRawHeader("Cookie",
                         QStringLiteral("PHPSESSID %1")
                           .arg(ConfigHandler().infomediaUserHash())
                           .toUtf8());

    QNetworkReply* reply = m_NetworkAM->post(request, http);
    http->setParent(reply);
}

void InfomediaUploader::deleteImage(const QString& fileName,
                                    const QString& deleteToken)
{
    Q_UNUSED(fileName)
    m_NetworkAM = new QNetworkAccessManager(this);

    QUrl url(QStringLiteral(INFOMEDIA_API_URL));
    QHttpMultiPart* http = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart reqTypePart;
    reqTypePart.setHeader(QNetworkRequest::ContentTypeHeader,
                          QVariant("text/plain"));
    reqTypePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                          QVariant("form-data; name=\"reqtype\""));
    reqTypePart.setBody("deletefiles");
    http->append(reqTypePart);

    QHttpPart userPart;
    userPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"userhash\""));
    userPart.setHeader(QNetworkRequest::ContentTypeHeader,
                       QVariant("text/plain"));
    userPart.setBody(deleteToken.toUtf8());
    http->append(userPart);

    QHttpPart secretPart;
    secretPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                         QVariant("form-data; name=\"secret\""));
    secretPart.setHeader(QNetworkRequest::ContentTypeHeader,
                         QVariant("text/plain"));
    secretPart.setBody(ConfigHandler().infomediaApiToken().toUtf8());
    http->append(secretPart);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"file\";"));
    filePart.setBody(fileName.toUtf8());
    filePart.setHeader(QNetworkRequest::ContentTypeHeader,
                       QVariant("text/plain"));
    http->append(filePart);

    QNetworkRequest request(url);
    request.setRawHeader("Cookie",
                         QStringLiteral("PHPSESSID %1")
                           .arg(ConfigHandler().infomediaUserHash())
                           .toUtf8());

    QNetworkReply* reply = m_NetworkAM->post(request, http);
    http->setParent(reply);

    if (reply->error() != QNetworkReply::NoError) {
        notification()->showMessage(tr("Unable to delete file."));
    }

    emit deleteOk();
}
