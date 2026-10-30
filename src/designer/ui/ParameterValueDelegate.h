#pragma once

#include <QComboBox>
#include <QLineEdit>
#include <QDoubleValidator>
#include <QLocale>
#include <QStyledItemDelegate>
#include <QKeyEvent>
#include <QTimer>
#include <QToolTip>
#include <functional>
#include <limits>

// The model supplies a point's actual definition in item roles. Numeric text is
// deliberately not narrowed to QSpinBox's signed-int range.
class ParameterValueDelegate final : public QStyledItemDelegate
{
public:
    enum Role {
        DataTypeRole = Qt::UserRole + 1,
        PointIdRole,
        InitialValueRole,
        MinValueRole,
        MaxValueRole,
        ReadOnlyRole
    };
    std::function<bool(const QString&, const QString&, QString*)> validate;
    std::function<void(const QString&, const QString&, bool, quint64)> submit;
    std::function<void(QWidget*)> editorCreated;
    explicit ParameterValueDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&,
                          const QModelIndex& index) const override
    {
        if (index.data(ReadOnlyRole).toBool()) return nullptr;
        QWidget* result = nullptr;
        if (index.data(DataTypeRole).toString().compare(QStringLiteral("BOOL"), Qt::CaseInsensitive) == 0) {
            auto* editor = new QComboBox(parent);
            editor->addItem(QStringLiteral("false"), QStringLiteral("0"));
            editor->addItem(QStringLiteral("true"), QStringLiteral("1"));
            result = editor;
        } else {
            auto* editor = new QLineEdit(parent);
            installValidator(editor, index);
            result = editor;
        }
        result->setProperty("parameterPointId", index.data(PointIdRole));
        if (editorCreated) editorCreated(result);
        return result;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        const QString value = index.data(InitialValueRole).toString();
        if (auto* combo = qobject_cast<QComboBox*>(editor)) {
            const QString normalized = value.trimmed().toLower();
            if (normalized == QStringLiteral("true") || normalized == QStringLiteral("1"))
                combo->setCurrentIndex(1);
            else if (normalized == QStringLiteral("false") || normalized == QStringLiteral("0"))
                combo->setCurrentIndex(0);
            else
                combo->setCurrentIndex(-1);
        } else if (auto* line = qobject_cast<QLineEdit*>(editor)) {
            line->setText(value);
            line->selectAll();
        }
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override
    {
        const QString value = editorValue(editor);
        const QString id = index.data(PointIdRole).toString();
        const bool apply = editor->property("explicitParameterApply").toBool();
        const quint64 generation = editor->property("parameterGeneration").toULongLong();
        model->setData(index, value, Qt::EditRole);
        if (submit) {
            const auto callback = submit;
            QTimer::singleShot(0, const_cast<ParameterValueDelegate*>(this),
                               [callback, id, value, apply, generation] { callback(id, value, apply, generation); });
        }
    }

    bool eventFilter(QObject* object, QEvent* event) override
    {
        auto* editor = qobject_cast<QWidget*>(object);
        if (!editor || !editor->property("parameterPointId").isValid())
            return QStyledItemDelegate::eventFilter(object, event);
        bool committing = event->type() == QEvent::FocusOut;
        bool enter = false;
        if (event->type() == QEvent::KeyPress) {
            const int key = static_cast<QKeyEvent*>(event)->key();
            enter = key == Qt::Key_Return || key == Qt::Key_Enter;
            committing = enter || key == Qt::Key_Tab || key == Qt::Key_Backtab;
        }
        if (committing && validate) {
            QString error;
            if (!validate(editor->property("parameterPointId").toString(), editorValue(editor), &error)) {
                editor->setToolTip(error);
                QToolTip::showText(editor->mapToGlobal(QPoint(0, editor->height())), error, editor);
                if (event->type() == QEvent::FocusOut)
                    QTimer::singleShot(0, editor, [editor] { editor->setFocus(); });
                return true;
            }
            if (enter) editor->setProperty("explicitParameterApply", true);
        }
        return QStyledItemDelegate::eventFilter(object, event);
    }

private:
    static void installValidator(QLineEdit* editor, const QModelIndex& index)
    {
        const QString type = index.data(DataTypeRole).toString().trimmed().toUpper();
        double bottom = 0.0;
        double top = 0.0;
        int decimals = 0;
        if (type == QStringLiteral("INT16")) {
            bottom = -32768.0; top = 32767.0;
        } else if (type == QStringLiteral("INT32")) {
            bottom = -2147483648.0; top = 2147483647.0;
        } else if (type == QStringLiteral("UINT16")) {
            bottom = 0.0; top = 65535.0;
        } else if (type == QStringLiteral("UINT32")) {
            bottom = 0.0; top = 4294967295.0;
        } else if (type == QStringLiteral("REAL")) {
            bottom = -std::numeric_limits<double>::max();
            top = std::numeric_limits<double>::max();
            decimals = -1;
        } else {
            return; // STRING and unknown types accept text; controller validates.
        }
        bool ok = false;
        const QString minText = index.data(MinValueRole).toString().trimmed();
        if (!minText.isEmpty()) {
            const double value = minText.toDouble(&ok);
            if (ok) bottom = qMax(bottom, value);
        }
        const QString maxText = index.data(MaxValueRole).toString().trimmed();
        if (!maxText.isEmpty()) {
            const double value = maxText.toDouble(&ok);
            if (ok) top = qMin(top, value);
        }
        auto* validator = new QDoubleValidator(bottom, top, decimals, editor);
        validator->setNotation(QDoubleValidator::ScientificNotation);
        validator->setLocale(QLocale::c());
        editor->setValidator(validator);
    }

    static QString editorValue(QWidget* editor)
    {
        if (auto* combo = qobject_cast<QComboBox*>(editor)) return combo->currentData().toString();
        if (auto* line = qobject_cast<QLineEdit*>(editor)) return line->text();
        return {};
    }
};
