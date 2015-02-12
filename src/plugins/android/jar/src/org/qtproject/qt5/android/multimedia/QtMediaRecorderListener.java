/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtMultimedia of the Qt Toolkit.
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

package org.qtproject.qt5.android.multimedia;

import android.media.MediaRecorder;

public class QtMediaRecorderListener implements MediaRecorder.OnErrorListener, MediaRecorder.OnInfoListener
{
    private long m_id = -1;

    public QtMediaRecorderListener(long id)
    {
        m_id = id;
    }

    @Override
    public void onError(MediaRecorder mr, int what, int extra)
    {
        notifyError(m_id, what, extra);
    }

    @Override
    public void onInfo(MediaRecorder mr, int what, int extra)
    {
        notifyInfo(m_id, what, extra);
    }

    private static native void notifyError(long id, int what, int extra);
    private static native void notifyInfo(long id, int what, int extra);
}
