#pragma once
#include "AppLogging.h"
#include <QUuid>
#include <atomic>

namespace Core {
// Shared by asynchronous callbacks. Exactly one terminal event survives races,
// cancellation and initiating-object destruction.
class BusinessOperation {
public:
    BusinessOperation(QString name, QString target, QVariantMap fields)
        : m_name(std::move(name)), m_target(std::move(target)), m_fields(std::move(fields)),
          m_id(QUuid::createUuid().toString(QUuid::WithoutBraces)) {
        AppLogging::writeBusinessEvent(m_name + "_started", QtInfoMsg, m_id,
            "admission", "started", 0, m_target, m_fields);
    }
    ~BusinessOperation() { finish("canceled", "Operation context was released"); }
    void finish(const QString& status, const QString& message = {}) {
        if (m_finished.exchange(true)) return;
        auto fields = m_fields;
        fields.insert("message", message);
        AppLogging::writeBusinessEvent(m_name + "_finished",
            status == "succeeded" ? QtInfoMsg : QtWarningMsg, m_id,
            "finish", status, status == "succeeded" ? 0 : -1, m_target, fields);
    }
private:
    QString m_name, m_target;
    QVariantMap m_fields;
    QString m_id;
    std::atomic_bool m_finished{false};
};
}
