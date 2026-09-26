// SPDX-License-Identifier: MIT
#include <StorageKit/Dir.h>

#include "internal_p.h"

namespace StorageKit {

Dir::Dir(const QUrl &rootUrl)
    : d(createBackend(rootUrl))
{
}

Dir::Dir(const Dir &other) = default;
Dir &Dir::operator=(const Dir &other) = default;
Dir::~Dir() = default;

bool Dir::exists() const
{
    if (!d->isValid())
        return false;
    if (m_path.isEmpty())
        return true;
    Stat st;
    return d->statOne(m_path, &st) == 0 && st.type == Stat::Dir;
}

QString Dir::dirName() const
{
    const int slash = m_path.lastIndexOf(u'/');
    if (!m_path.isEmpty())
        return slash < 0 ? m_path : m_path.mid(slash + 1);
    return urlInfo(d->root()).fileName();
}

QUrl Dir::rootUrl() const
{
    return d->root();
}

QString Dir::path() const
{
    return m_path;
}

bool Dir::cd(const QString &dirName)
{
    if (dirName == u"..")
        return cdUp();
    const QString target = filePath(dirName);
    if (target.isNull() && !dirName.isEmpty())
        return false;
    Stat st;
    if (d->statOne(target, &st) != 0 || st.type != Stat::Dir)
        return false;
    m_path = target;
    return true;
}

bool Dir::cdUp()
{
    if (m_path.isEmpty())
        return false; // the granted root is the ceiling
    const int slash = m_path.lastIndexOf(u'/');
    m_path = slash < 0 ? QString{} : m_path.left(slash);
    return true;
}

FileInfoList Dir::entryInfoList(const QStringList &nameFilters, QDir::Filters filters,
                                QDir::SortFlags sort) const
{
    FileInfoList raw;
    if (d->rawList(m_path, &raw) != 0)
        return {};
    return internal::filterAndSort(raw, nameFilters, filters, sort);
}

bool Dir::exists(const QString &name) const
{
    const QString path = filePath(name);
    return !path.isNull() && d->exists(path);
}

bool Dir::mkdir(const QString &name)
{
    const QString path = filePath(name);
    return !path.isNull() && d->mkdir(path) == 0;
}

bool Dir::rmdir(const QString &name)
{
    const QString path = filePath(name);
    return !path.isNull() && d->rmdir(path) == 0;
}

bool Dir::remove(const QString &name)
{
    const QString path = filePath(name);
    return !path.isNull() && d->unlink(path) == 0;
}

bool Dir::rename(const QString &from, const QString &to)
{
    const QString fromPath = filePath(from);
    const QString toPath = filePath(to);
    return !fromPath.isNull() && !toPath.isNull() && d->rename(fromPath, toPath) == 0;
}

// Validated path of `name` relative to the granted root; null QString when
// the name would escape the capability.
QString Dir::filePath(const QString &name) const
{
    QString clean;
    if (!internal::cleanPath(name, &clean))
        return QString{};
    const QString joined = internal::joinPath(m_path, clean);
    // joinPath("", "") is "" (the current dir); keep "" distinguishable from null
    return joined.isNull() ? QStringLiteral("") : joined;
}

} // namespace StorageKit
