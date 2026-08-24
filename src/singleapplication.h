#ifndef SINGLEAPPLICATION_H
#define SINGLEAPPLICATION_H

#include <QDir>
#include <QLockFile>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QCryptographicHash>

class SingleApplication : public QObject
{
    Q_OBJECT

public:
    explicit SingleApplication(QObject *parent = nullptr)
        : QObject(parent),
          m_lockFile(getLockFilePath())
    {
        if (!m_lockFile.isLocked()) {
            m_lockFile.tryLock(100);
        }
    }

    Q_INVOKABLE bool isRunning() { return !m_lockFile.isLocked(); }

private:
    static QString getLockFilePath() {
        QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        if (runtimeDir.isEmpty()) {
            runtimeDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        }
        QString appPath = QCoreApplication::applicationFilePath();
        QString userKey = QString::fromLatin1(QCryptographicHash::hash((qgetenv("USER") + ":" + appPath.toUtf8()), QCryptographicHash::Sha1).toHex());
        return runtimeDir + "/streammatrix-" + userKey + ".lock";
    }

    QLockFile m_lockFile;
};

#endif // SINGLEAPPLICATION_H
