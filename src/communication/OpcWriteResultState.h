#pragma once
#include <QDateTime>
#include <QString>

// Protocol-neutral confirmation state; transports retain their own item/value mapping.
struct OpcWriteResultState {
    QString pointId;
    QDateTime time, successfulTime, failedTime;
    bool success = false;
    QString message, successfulMessage, failedMessage;
    int successfulCount = 0;
    int failedCount = 0;

    void record(const QString& point, bool succeeded, const QString& detail) {
        pointId = point;
        success = succeeded;
        message = detail;
        time = QDateTime::currentDateTimeUtc();
        if (succeeded) {
            successfulTime = time;
            successfulMessage = detail;
            ++successfulCount;
        } else {
            failedTime = time;
            failedMessage = detail;
            ++failedCount;
        }
    }
};
