// SPDX-License-Identifier: GPL-3.0-or-later

#include <core/scripting/scriptregistry.h>

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>

#include <gtest/gtest.h>

int main(int argc, char* argv[])
{
    QTemporaryDir profile{QDir::tempPath() + QStringLiteral("/scrobbler-tests-XXXXXX")};
    if(!profile.isValid()) {
        return 1;
    }
    qputenv("XDG_CONFIG_HOME", (profile.path() + QStringLiteral("/config")).toUtf8());
    qputenv("XDG_CACHE_HOME", (profile.path() + QStringLiteral("/cache")).toUtf8());
    qputenv("XDG_DATA_HOME", (profile.path() + QStringLiteral("/data")).toUtf8());
    qputenv("XDG_STATE_HOME", (profile.path() + QStringLiteral("/state")).toUtf8());
    qunsetenv("HTTP_PROXY");
    qunsetenv("http_proxy");
    qunsetenv("HTTPS_PROXY");
    qunsetenv("https_proxy");
    qunsetenv("ALL_PROXY");
    qunsetenv("all_proxy");
    QCoreApplication app{argc, argv};
    QCoreApplication::setApplicationName(QStringLiteral("fooyin-scrobbler-tests"));
    Fooyin::ScriptRegistry::instance().finalise();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
