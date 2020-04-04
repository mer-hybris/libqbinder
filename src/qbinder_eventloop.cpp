/*
 * Copyright (C) 2020 Jolla Ltd.
 * Copyright (C) 2020 Slava Monich <slava.monich@jolla.com>
 *
 * You may use this file under the terms of BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the names of the copyright holders nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "qbinder_eventloop.h"

#include <gbinder_eventloop.h>

#include <QTimer>
#include <QAtomicInt>
#include <QAtomicPointer>

struct QBinderEventLoopTimeout : public QTimer {
    Q_OBJECT

public:
    QBinderEventLoopTimeout(int, GSourceFunc, gpointer);

    static QBinderEventLoopTimeout* from(GBinderEventLoopTimeout*);

protected:
    void timerEvent(QTimerEvent*) Q_DECL_OVERRIDE;

public:
    GBinderEventLoopTimeout iTimeout;
    GSourceFunc iFunction;
    gpointer iData;
};

class QBinderEventLoopCallback {
public:
    typedef void (DestroyNotify)(gpointer);
    enum State { INITIAL, INVOKED, CANCELED };

    QBinderEventLoopCallback(GBinderEventLoopCallbackFunc, gpointer, GDestroyNotify);
    ~QBinderEventLoopCallback();

    static QBinderEventLoopCallback* from(GBinderEventLoopCallback*);

public:
    void ref();
    void unref();
    void invoke();
    void cancel();
    void finalize();

public:
    GBinderEventLoopCallback iCallback;
    QAtomicInt iRef;
    QAtomicInt iState;
    GBinderEventLoopCallbackFunc iFunction;
    QAtomicPointer<DestroyNotify> iFinalize;
    gpointer iData;
};

class QBinderEventLoopCallbackRef {
public:
    QBinderEventLoopCallbackRef& operator=(const QBinderEventLoopCallbackRef& aRef);
    QBinderEventLoopCallbackRef(GBinderEventLoopCallback* aCallback);
    QBinderEventLoopCallbackRef(const QBinderEventLoopCallbackRef& aRef);
    QBinderEventLoopCallbackRef();
    ~QBinderEventLoopCallbackRef();

public:
    QBinderEventLoopCallback* iCallback;
};

class QBinderEventLoopIntegration : public QObject
{
    Q_OBJECT

public:
    // QBinderEventLoopIntegration callbacks
    static GBinderEventLoopTimeout* timeoutAdd(guint, GSourceFunc, gpointer);
    static void timeoutRemove(GBinderEventLoopTimeout* aTimeout);
    static GBinderEventLoopCallback* callbackNew(GBinderEventLoopCallbackFunc,
        gpointer, GDestroyNotify);
    static void callbackRef(GBinderEventLoopCallback*);
    static void callbackUnref(GBinderEventLoopCallback*);
    static void callbackSchedule(GBinderEventLoopCallback*);
    static void callbackCancel(GBinderEventLoopCallback*);
    static void cleanup();

public Q_SLOTS:
    void onCallback(QBinderEventLoopCallbackRef);

public:
    static QBinderEventLoopIntegration* gInstance;
    static const GBinderEventLoopIntegration gCallbacks;
};

Q_DECLARE_METATYPE(QBinderEventLoopCallbackRef)

// ==========================================================================
// QBinderEventLoopTimeout
// ==========================================================================

QBinderEventLoopTimeout::QBinderEventLoopTimeout(int aMsec,
    GSourceFunc aFunction, gpointer aData) :
    QTimer(QBinderEventLoopIntegration::gInstance),
    iFunction(aFunction),
    iData(aData)
{
    iTimeout.eventloop = &QBinderEventLoopIntegration::gCallbacks;
    start(aMsec);
}

QBinderEventLoopTimeout* QBinderEventLoopTimeout::from(
    GBinderEventLoopTimeout* aTimeout)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
    return (QBinderEventLoopTimeout*)((uchar*)(aTimeout) -
        G_STRUCT_OFFSET(QBinderEventLoopTimeout, iTimeout));
#pragma GCC diagnostic pop
}

void QBinderEventLoopTimeout::timerEvent(QTimerEvent* aEvent)
{
    if (timerId() == aEvent->timerId()) {
        if (iFunction(iData) == G_SOURCE_REMOVE) {
            stop();
            deleteLater();
            return;
        }
    }
    QTimer::timerEvent(aEvent);
}

// ==========================================================================
// QBinderEventLoopCallback
// ==========================================================================

QBinderEventLoopCallback::QBinderEventLoopCallback(GBinderEventLoopCallbackFunc aFunction,
    gpointer aData, GDestroyNotify aFinalize) :
    iRef(1),
    iState(INITIAL),
    iFunction(aFunction),
    iFinalize(aFinalize),
    iData(aData)
{
    iCallback.eventloop = &QBinderEventLoopIntegration::gCallbacks;
}

QBinderEventLoopCallback::~QBinderEventLoopCallback()
{
    finalize();
}

QBinderEventLoopCallback* QBinderEventLoopCallback::from(
    GBinderEventLoopCallback* aCallback)
{
    return (QBinderEventLoopCallback*)((uchar*)(aCallback) -
        G_STRUCT_OFFSET(QBinderEventLoopCallback, iCallback));
}


void QBinderEventLoopCallback::ref()
{
    iRef.ref();
}

void QBinderEventLoopCallback::unref()
{
    if (!iRef.deref()) {
        delete this;
    }
}

void QBinderEventLoopCallback::invoke()
{
    if (iState.testAndSetOrdered(INITIAL, INVOKED)) {
        iFunction(iData);
        finalize();
    }
}

void QBinderEventLoopCallback::cancel()
{
    if (iState.testAndSetOrdered(INITIAL, CANCELED)) {
        finalize();
    }
}

void QBinderEventLoopCallback::finalize()
{
    GDestroyNotify finalize = iFinalize.fetchAndStoreOrdered(Q_NULLPTR);
    if (finalize) {
        finalize(iData);
    }
}

// ==========================================================================
// QBinderEventLoopCallbackRef
// ==========================================================================

QBinderEventLoopCallbackRef::QBinderEventLoopCallbackRef() :
    iCallback(Q_NULLPTR)
{
}

QBinderEventLoopCallbackRef::QBinderEventLoopCallbackRef(
    GBinderEventLoopCallback* aCallback)
{
    if (aCallback) {
        iCallback = QBinderEventLoopCallback::from(aCallback);
        iCallback->ref();
    } else {
        iCallback = Q_NULLPTR;
    }
}

QBinderEventLoopCallbackRef::QBinderEventLoopCallbackRef(
    const QBinderEventLoopCallbackRef& aRef) :
    iCallback(aRef.iCallback)
{
    if (iCallback) {
        iCallback->ref();
    }
}

QBinderEventLoopCallbackRef::~QBinderEventLoopCallbackRef()
{
    if (iCallback) {
        iCallback->unref();
    }
}

QBinderEventLoopCallbackRef& QBinderEventLoopCallbackRef::operator=(
    const QBinderEventLoopCallbackRef& aRef)
{
    if (iCallback != aRef.iCallback) {
        if (iCallback) {
            iCallback->unref();
        }
        iCallback = aRef.iCallback;
        if (iCallback) {
            iCallback->ref();
        }
    }
    return *this;
}

// ==========================================================================
// QBinderEventLoopIntegration
// ==========================================================================

QBinderEventLoopIntegration* QBinderEventLoopIntegration::gInstance = Q_NULLPTR;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
const GBinderEventLoopIntegration QBinderEventLoopIntegration::gCallbacks = {
    QBinderEventLoopIntegration::timeoutAdd,
    QBinderEventLoopIntegration::timeoutRemove,
    QBinderEventLoopIntegration::callbackNew,
    QBinderEventLoopIntegration::callbackRef,
    QBinderEventLoopIntegration::callbackUnref,
    QBinderEventLoopIntegration::callbackSchedule,
    QBinderEventLoopIntegration::callbackCancel,
    QBinderEventLoopIntegration::cleanup
};
#pragma GCC diagnostic pop

GBinderEventLoopTimeout* QBinderEventLoopIntegration::timeoutAdd(guint aInterval,
    GSourceFunc aFunction, gpointer aData)
{
    return &(new QBinderEventLoopTimeout(aInterval, aFunction, aData))->iTimeout;
}

void QBinderEventLoopIntegration::timeoutRemove(GBinderEventLoopTimeout* aTimeout)
{
    delete QBinderEventLoopTimeout::from(aTimeout);
}

GBinderEventLoopCallback* QBinderEventLoopIntegration::callbackNew(
    GBinderEventLoopCallbackFunc aFunction, gpointer aData,
    GDestroyNotify aFinalize)
{
    return &(new QBinderEventLoopCallback(aFunction, aData, aFinalize))->iCallback;
}

void QBinderEventLoopIntegration::callbackRef(GBinderEventLoopCallback* aCallback)
{
    QBinderEventLoopCallback::from(aCallback)->ref();
}

void QBinderEventLoopIntegration::callbackUnref(GBinderEventLoopCallback* aCallback)
{
    QBinderEventLoopCallback::from(aCallback)->unref();
}

void QBinderEventLoopIntegration::callbackCancel(GBinderEventLoopCallback* aCallback)
{
    QBinderEventLoopCallback::from(aCallback)->cancel();
}

void QBinderEventLoopIntegration::callbackSchedule(GBinderEventLoopCallback* aCallback)
{
    QMetaObject::invokeMethod(gInstance, "onCallback", Qt::QueuedConnection,
        Q_ARG(QBinderEventLoopCallbackRef, QBinderEventLoopCallbackRef(aCallback)));
}

void QBinderEventLoopIntegration::cleanup()
{
    delete gInstance;
    gInstance = Q_NULLPTR;
}

void QBinderEventLoopIntegration::onCallback(QBinderEventLoopCallbackRef aRef)
{
    aRef.iCallback->invoke();
}

// ==========================================================================
// QBinder
// ==========================================================================

void QBinder::registerEventLoop()
{
    qRegisterMetaType<QBinderEventLoopCallbackRef>("QBinderEventLoopCallbackRef");
    QBinderEventLoopIntegration::gInstance = new QBinderEventLoopIntegration;
    gbinder_eventloop_set(&QBinderEventLoopIntegration::gCallbacks);
}

#include "qbinder_eventloop.moc"
