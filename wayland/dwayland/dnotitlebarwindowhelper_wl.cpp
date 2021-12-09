/*
 * Copyright (C) 2017 ~ 2019 Uniontech Technology Co., Ltd.
 *
 * Author:     WangPeng <wangpenga@uniontech.com>
 *
 * Maintainer: AlexOne  <993381@qq.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "dnotitlebarwindowhelper_wl.h"
#include "vtablehook.h"

#define protected public
#include <QWindow>
#undef protected
#include <QMouseEvent>
#include <QGuiApplication>
#include <QTimer>
#include <QMetaProperty>
#include <QScreen>
#include <qpa/qplatformwindow.h>

#define private public
#include "QtWaylandClient/private/qwaylandintegration_p.h"
#include "QtWaylandClient/private/qwaylandshellsurface_p.h"
#include "QtWaylandClient/private/qwaylandwindow_p.h"
#include "QtWaylandClient/private/qwaylandcursor_p.h"
#undef private

#include <private/qguiapplication_p.h>

DPP_BEGIN_NAMESPACE

QHash<const QWindow*, DNoTitlebarWlWindowHelper*> DNoTitlebarWlWindowHelper::mapped;

DNoTitlebarWlWindowHelper::DNoTitlebarWlWindowHelper(QWindow *window)
    : QObject(window)
    , m_window(window)
{
    // 不允许设置窗口为无边框的
    if (window->flags().testFlag(Qt::FramelessWindowHint)) {
        window->setFlag(Qt::FramelessWindowHint, false);
    }

    mapped[window] = this;
}

DNoTitlebarWlWindowHelper::~DNoTitlebarWlWindowHelper()
{
    if (VtableHook::hasVtable(m_window)) {
        VtableHook::resetVtable(m_window);
    }

    mapped.remove(qobject_cast<QWindow*>(parent()));

    // TODO
    // if (m_window->handle()) { // 当本地窗口还存在时，移除设置过的窗口属性
    //     //! Utility::clearWindowProperty(m_windowID, Utility::internAtom(_DEEPIN_SCISSOR_WINDOW));
    //     DPlatformIntegration::clearNativeSettings(m_windowID);
    // }
}

void DNoTitlebarWlWindowHelper::setWindowProperty(QWindow *window, const char *name, const QVariant &value)
{
    const QVariant &old_value = window->property(name);

    if (old_value == value)
        return;

    if (value.typeName() == QByteArray("QPainterPath")) {
        const QPainterPath &old_path = qvariant_cast<QPainterPath>(old_value);
        const QPainterPath &new_path = qvariant_cast<QPainterPath>(value);

        if (old_path == new_path) {
            return;
        }
    }

    window->setProperty(name, value);

    if (DNoTitlebarWlWindowHelper *self = mapped.value(window)) {
        // 本地设置无效时不可更新窗口属性，否则会导致setProperty函数被循环调用
        // if (!self->m_nativeSettingsValid) {
        //     return;
        // }

        QByteArray name_array(name);

        if (!name_array.startsWith("_d_"))
            return;

        // to upper
        name_array[3] = name_array.at(3) & ~0x20;

        const QByteArray slot_name = "update" + name_array.mid(3) + "FromProperty";

        if (!QMetaObject::invokeMethod(self, slot_name.constData(), Qt::DirectConnection)) {
            qWarning() << "Failed to update property:" << slot_name;
        }
    }
}

void DNoTitlebarWlWindowHelper::popupSystemWindowMenu(quintptr wid)
{
    auto fromQtWinId = [](WId id) {
        QWindow *window = nullptr;

        for (auto win : qApp->allWindows()) {
            if (win->winId() == id) {
                window = win;
                break;
            }
        }
        return window;
    };

    QWindow *window = fromQtWinId(wid);
    if(!window || !window->handle())
        return;

    QtWaylandClient::QWaylandWindow *wl_window = static_cast<QtWaylandClient::QWaylandWindow *>(window->handle());
    if (!wl_window->shellSurface())
        return;

    if (QtWaylandClient::QWaylandShellSurface *s = wl_window->shellSurface()) {
        auto wl_integration = static_cast<QtWaylandClient::QWaylandIntegration *>(QGuiApplicationPrivate::platformIntegration());
        s->showWindowMenu(wl_integration->display()->defaultInputDevice());
    }
}

DPP_END_NAMESPACE