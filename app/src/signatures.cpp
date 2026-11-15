// SPDX-License-Identifier: AGPL-3.0-or-later
#include "signatures.h"

#include <QStandardPaths>
#include <QVariantMap>

namespace {

const QString accountChoice = QStringLiteral("account");
const QString noneChoice = QStringLiteral("none");

QString fileName()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/signatures.ini");
}

} // namespace

Signatures::Signatures(QObject *parent)
    : QObject(parent)
    , m_settings(fileName(), QSettings::IniFormat)
{
    const int count = m_settings.beginReadArray(QStringLiteral("signatures"));
    for (int i = 0; i < count; ++i) {
        m_settings.setArrayIndex(i);
        Signature signature;
        signature.id = m_settings.value(QStringLiteral("id")).toString();
        signature.name = m_settings.value(QStringLiteral("name")).toString();
        signature.text = m_settings.value(QStringLiteral("text")).toString();
        if (!signature.id.isEmpty())
            m_signatures.append(signature);
    }
    m_settings.endArray();

    const int choices = m_settings.beginReadArray(QStringLiteral("choices"));
    for (int i = 0; i < choices; ++i) {
        m_settings.setArrayIndex(i);
        const QString address = m_settings.value(QStringLiteral("address")).toString().toLower();
        const QString choice = m_settings.value(QStringLiteral("choice")).toString();
        if (!address.isEmpty() && !choice.isEmpty())
            m_choices.insert(address, choice);
    }
    m_settings.endArray();

    m_nextId = qMax(1, m_settings.value(QStringLiteral("nextId"), 1).toInt());
}

QVariantList Signatures::list() const
{
    QVariantList result;
    for (const Signature &signature : m_signatures) {
        QVariantMap entry;
        entry.insert(QStringLiteral("id"), signature.id);
        entry.insert(QStringLiteral("name"), signature.name);
        entry.insert(QStringLiteral("text"), signature.text);
        result.append(entry);
    }
    return result;
}

QString Signatures::add(const QString &name, const QString &text)
{
    Signature signature;
    signature.id = QString::number(m_nextId++);
    signature.name = name.trimmed();
    signature.text = text;
    m_signatures.append(signature);
    store();
    return signature.id;
}

void Signatures::update(const QString &id, const QString &name, const QString &text)
{
    const int index = indexOf(id);
    if (index < 0)
        return;
    m_signatures[index].name = name.trimmed();
    m_signatures[index].text = text;
    store();
}

void Signatures::remove(const QString &id)
{
    const int index = indexOf(id);
    if (index < 0)
        return;
    m_signatures.removeAt(index);
    for (auto it = m_choices.begin(); it != m_choices.end();) {
        if (it.value() == id)
            it = m_choices.erase(it);
        else
            ++it;
    }
    store();
}

bool Signatures::exists(const QString &id) const
{
    return indexOf(id) >= 0;
}

QString Signatures::name(const QString &id) const
{
    const int index = indexOf(id);
    return index < 0 ? QString() : m_signatures.at(index).name;
}

QString Signatures::text(const QString &id) const
{
    const int index = indexOf(id);
    return index < 0 ? QString() : m_signatures.at(index).text;
}

QString Signatures::find(const QString &text) const
{
    const QString wanted = text.trimmed();
    if (wanted.isEmpty())
        return QString();
    for (const Signature &signature : m_signatures) {
        if (signature.text.trimmed() == wanted)
            return signature.id;
    }
    return QString();
}

QString Signatures::choiceFor(const QString &address) const
{
    const QString choice = m_choices.value(address.trimmed().toLower());
    if (choice == noneChoice || indexOf(choice) >= 0)
        return choice;
    return accountChoice;
}

void Signatures::setChoiceFor(const QString &address, const QString &choice)
{
    const QString key = address.trimmed().toLower();
    if (key.isEmpty())
        return;
    if (choice.isEmpty() || choice == accountChoice)
        m_choices.remove(key);
    else
        m_choices.insert(key, choice);
    store();
}

int Signatures::indexOf(const QString &id) const
{
    for (int i = 0; i < m_signatures.size(); ++i) {
        if (m_signatures.at(i).id == id)
            return i;
    }
    return -1;
}

void Signatures::store()
{
    m_settings.remove(QStringLiteral("signatures"));
    m_settings.beginWriteArray(QStringLiteral("signatures"), m_signatures.size());
    for (int i = 0; i < m_signatures.size(); ++i) {
        m_settings.setArrayIndex(i);
        m_settings.setValue(QStringLiteral("id"), m_signatures.at(i).id);
        m_settings.setValue(QStringLiteral("name"), m_signatures.at(i).name);
        m_settings.setValue(QStringLiteral("text"), m_signatures.at(i).text);
    }
    m_settings.endArray();

    m_settings.remove(QStringLiteral("choices"));
    m_settings.beginWriteArray(QStringLiteral("choices"), m_choices.size());
    int i = 0;
    for (auto it = m_choices.constBegin(); it != m_choices.constEnd(); ++it, ++i) {
        m_settings.setArrayIndex(i);
        m_settings.setValue(QStringLiteral("address"), it.key());
        m_settings.setValue(QStringLiteral("choice"), it.value());
    }
    m_settings.endArray();

    m_settings.setValue(QStringLiteral("nextId"), m_nextId);
    m_settings.sync();
    ++m_revision;
    emit changed();
}
