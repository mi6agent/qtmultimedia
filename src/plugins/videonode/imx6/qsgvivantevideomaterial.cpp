/****************************************************************************
**
** Copyright (C) 2014 Pelagicore AG
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "qsgvivantevideomaterial.h"
#include "qsgvivantevideomaterialshader.h"
#include "qsgvivantevideonode.h"

#include <QOpenGLContext>
#include <QThread>

#include <unistd.h>


//#define QT_VIVANTE_VIDEO_DEBUG

QSGVivanteVideoMaterial::QSGVivanteVideoMaterial() :
    mOpacity(1.0),
    mWidth(0),
    mHeight(0),
    mFormat(QVideoFrame::Format_Invalid),
    mCurrentTexture(0),
    mMappable(true),
    mTexDirectTexture(0)
{
#ifdef QT_VIVANTE_VIDEO_DEBUG
    qDebug() << Q_FUNC_INFO;
#endif

    setFlag(Blending, false);
}

QSGVivanteVideoMaterial::~QSGVivanteVideoMaterial()
{
    clearTextures();
}

QSGMaterialType *QSGVivanteVideoMaterial::type() const {
    static QSGMaterialType theType;
    return &theType;
}

QSGMaterialShader *QSGVivanteVideoMaterial::createShader() const {
    return new QSGVivanteVideoMaterialShader;
}

int QSGVivanteVideoMaterial::compare(const QSGMaterial *other) const {
    if (this->type() == other->type()) {
        const QSGVivanteVideoMaterial *m = static_cast<const QSGVivanteVideoMaterial *>(other);
        if (this->mBitsToTextureMap == m->mBitsToTextureMap)
            return 0;
        else
            return 1;
    }
    return 1;
}

void QSGVivanteVideoMaterial::updateBlending() {
    setFlag(Blending, qFuzzyCompare(mOpacity, qreal(1.0)) ? false : true);
}

void QSGVivanteVideoMaterial::setCurrentFrame(const QVideoFrame &frame, QSGVideoNode::FrameFlags flags)
{
    QMutexLocker lock(&mFrameMutex);
    mNextFrame = frame;
    mMappable = !flags.testFlag(QSGVideoNode::FrameFiltered);

#ifdef QT_VIVANTE_VIDEO_DEBUG
    qDebug() << Q_FUNC_INFO << " new frame: " << frame;
#endif
}

void QSGVivanteVideoMaterial::bind()
{
    QOpenGLContext *glcontext = QOpenGLContext::currentContext();
    if (glcontext == 0) {
        qWarning() << Q_FUNC_INFO << "no QOpenGLContext::currentContext() => return";
        return;
    }

    QMutexLocker lock(&mFrameMutex);
    if (mNextFrame.isValid()) {
        mCurrentFrame.unmap();

        mCurrentFrame = mNextFrame;
        mCurrentTexture = vivanteMapping(mNextFrame);
    }
    else
        glBindTexture(GL_TEXTURE_2D, mCurrentTexture);
}

void QSGVivanteVideoMaterial::clearTextures()
{
    Q_FOREACH (GLuint id, mBitsToTextureMap.values()) {
#ifdef QT_VIVANTE_VIDEO_DEBUG
        qDebug() << "delete texture: " << id;
#endif
        glDeleteTextures(1, &id);
    }
    mBitsToTextureMap.clear();

    if (mTexDirectTexture) {
        glDeleteTextures(1, &mTexDirectTexture);
        mTexDirectTexture = 0;
    }
}

GLuint QSGVivanteVideoMaterial::vivanteMapping(QVideoFrame vF)
{
    QOpenGLContext *glcontext = QOpenGLContext::currentContext();
    if (glcontext == 0) {
        qWarning() << Q_FUNC_INFO << "no QOpenGLContext::currentContext() => return 0";
        return 0;
    }

    static PFNGLTEXDIRECTVIVPROC glTexDirectVIV_LOCAL = 0;
    static PFNGLTEXDIRECTVIVMAPPROC glTexDirectVIVMap_LOCAL = 0;
    static PFNGLTEXDIRECTINVALIDATEVIVPROC glTexDirectInvalidateVIV_LOCAL = 0;

    if (glTexDirectVIV_LOCAL == 0 || glTexDirectVIVMap_LOCAL == 0 || glTexDirectInvalidateVIV_LOCAL == 0) {
        glTexDirectVIV_LOCAL = reinterpret_cast<PFNGLTEXDIRECTVIVPROC>(glcontext->getProcAddress("glTexDirectVIV"));
        glTexDirectVIVMap_LOCAL = reinterpret_cast<PFNGLTEXDIRECTVIVMAPPROC>(glcontext->getProcAddress("glTexDirectVIVMap"));
        glTexDirectInvalidateVIV_LOCAL = reinterpret_cast<PFNGLTEXDIRECTINVALIDATEVIVPROC>(glcontext->getProcAddress("glTexDirectInvalidateVIV"));
    }
    if (glTexDirectVIV_LOCAL == 0 || glTexDirectVIVMap_LOCAL == 0 || glTexDirectInvalidateVIV_LOCAL == 0) {
        qWarning() << Q_FUNC_INFO << "couldn't find \"glTexDirectVIVMap\" and/or \"glTexDirectInvalidateVIV\" => do nothing and return";
        return 0;
    }

    if (mWidth != vF.width() || mHeight != vF.height() || mFormat != vF.pixelFormat()) {
        mWidth = vF.width();
        mHeight = vF.height();
        mFormat = vF.pixelFormat();
        clearTextures();
    }

    if (vF.map(QAbstractVideoBuffer::ReadOnly)) {

        if (mMappable) {
            if (!mBitsToTextureMap.contains(vF.bits())) {
                // Haven't yet seen this logical address: map to texture.
                GLuint tmpTexId;
                glGenTextures(1, &tmpTexId);
                mBitsToTextureMap.insert(vF.bits(), tmpTexId);

                const uchar *constBits = vF.bits();
                void *bits = (void*)constBits;

#ifdef QT_VIVANTE_VIDEO_DEBUG
                qDebug() << Q_FUNC_INFO << "new texture, texId: " << tmpTexId << "; constBits: " << constBits;
#endif

                GLuint physical = ~0U;

                glBindTexture(GL_TEXTURE_2D, tmpTexId);
                glTexDirectVIVMap_LOCAL(GL_TEXTURE_2D,
                                        vF.width(), vF.height(),
                                        QSGVivanteVideoNode::getVideoFormat2GLFormatMap().value(vF.pixelFormat()),
                                        &bits, &physical);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexDirectInvalidateVIV_LOCAL(GL_TEXTURE_2D);

                return tmpTexId;
            } else {
                // Fastest path: already seen this logical address. Just
                // indicate that the data belonging to the texture has changed.
                glBindTexture(GL_TEXTURE_2D, mBitsToTextureMap.value(vF.bits()));
                glTexDirectInvalidateVIV_LOCAL(GL_TEXTURE_2D);
                return mBitsToTextureMap.value(vF.bits());
            }
        } else {
            // Cannot map. So copy.
            if (!mTexDirectTexture) {
                glGenTextures(1, &mTexDirectTexture);
                glBindTexture(GL_TEXTURE_2D, mTexDirectTexture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexDirectVIV_LOCAL(GL_TEXTURE_2D, mCurrentFrame.width(), mCurrentFrame.height(),
                                     QSGVivanteVideoNode::getVideoFormat2GLFormatMap().value(mCurrentFrame.pixelFormat()),
                                     (GLvoid **) &mTexDirectPlanes);
            } else {
                glBindTexture(GL_TEXTURE_2D, mTexDirectTexture);
            }
            switch (mCurrentFrame.pixelFormat()) {
            case QVideoFrame::Format_YUV420P:
            case QVideoFrame::Format_YV12:
                memcpy(mTexDirectPlanes[0], mCurrentFrame.bits(0), mCurrentFrame.height() * mCurrentFrame.bytesPerLine(0));
                memcpy(mTexDirectPlanes[1], mCurrentFrame.bits(1), mCurrentFrame.height() * mCurrentFrame.bytesPerLine(1));
                memcpy(mTexDirectPlanes[2], mCurrentFrame.bits(2), mCurrentFrame.height() * mCurrentFrame.bytesPerLine(2));
                break;
            case QVideoFrame::Format_NV12:
            case QVideoFrame::Format_NV21:
                memcpy(mTexDirectPlanes[0], mCurrentFrame.bits(0), mCurrentFrame.height() * mCurrentFrame.bytesPerLine(0));
                memcpy(mTexDirectPlanes[1], mCurrentFrame.bits(1), mCurrentFrame.height() / 2 * mCurrentFrame.bytesPerLine(1));
                break;
            default:
                memcpy(mTexDirectPlanes[0], mCurrentFrame.bits(), mCurrentFrame.height() * mCurrentFrame.bytesPerLine());
                break;
            }
            glTexDirectInvalidateVIV_LOCAL(GL_TEXTURE_2D);
            return mTexDirectTexture;
        }
    }
    else {
#ifdef QT_VIVANTE_VIDEO_DEBUG
        qWarning() << " couldn't map the QVideoFrame vF: " << vF;
#endif
        return 0;
    }

    Q_ASSERT(false); // should never reach this line!;
    return 0;
}
