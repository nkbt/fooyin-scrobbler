// SPDX-License-Identifier: GPL-3.0-or-later

#include "scrobbler.h"
#include "scrobblercache.h"
#include "settings/scrobblersettings.h"

#include <core/coresettings.h>
#include <core/library/musiclibrary.h>
#include <core/network/networkaccessmanager.h>
#include <core/player/playercontroller.h>
#include <utils/database/dbconnectionhandler.h>
#include <utils/database/dbconnectionpool.h>
#include <utils/database/dbconnectionprovider.h>
#include <utils/database/dbquery.h>
#include <utils/fypaths.h>
#include <utils/settings/settingsmanager.h>

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <memory>

using namespace Qt::StringLiterals;

namespace Fooyin::Scrobbler::Testing {
namespace {
class TestMusicLibrary : public MusicLibrary
{
public:
    bool hasLibrary() const override
    {
        return false;
    }
    std::optional<LibraryInfo> libraryInfo(int) const override
    {
        return {};
    }
    std::optional<LibraryInfo> libraryForPath(const QString&) const override
    {
        return {};
    }
    void loadAllTracks() override { }
    bool isEmpty() const override
    {
        return true;
    }
    void refreshAll() override { }
    void rescanAll() override { }
    ScanRequest refresh(const LibraryInfo&) override
    {
        return {};
    }
    ScanRequest rescan(const LibraryInfo&) override
    {
        return {};
    }
    void cancelScan(int) override { }
    ScanRequest scanTracks(const TrackList&) override
    {
        return {};
    }
    ScanRequest scanModifiedTracks(const TrackList&) override
    {
        return {};
    }
    ScanRequest scanFiles(const QList<QUrl>&) override
    {
        return {};
    }
    ScanRequest loadPlaylist(const QList<QUrl>&) override
    {
        return {};
    }
    TrackList tracks() const override
    {
        return {};
    }
    TrackList libraryTracks() const override
    {
        return {};
    }
    Track trackForId(int) const override
    {
        return {};
    }
    TrackList tracksForIds(const TrackIds&) const override
    {
        return {};
    }
    std::shared_ptr<TrackMetadataStore> metadataStore() const override
    {
        return {};
    }
    void updateTrack(const Track&) override { }
    void updateTracks(const TrackList&) override { }
    void updateTrackMetadata(const TrackList&) override { }
    WriteRequest writeTrackMetadata(const TrackList&) override
    {
        return {};
    }
    WriteRequest writeTrackCovers(const TrackCoverData&) override
    {
        return {};
    }
    PendingTrackCoverProvider* pendingTrackCoverProvider() const override
    {
        return nullptr;
    }
    void updateTrackStats(const TrackList&, Track::Stats) override { }
    void updateTrackStats(const Track&, Track::Stats) override { }
    WriteRequest removeUnavailbleTracks() override
    {
        return {};
    }
    WriteRequest deleteTracks(const TrackList&) override
    {
        return {};
    }
    std::optional<LibraryInfo> addExternalLibrary(const QString&, const QString&) override
    {
        return {};
    }
    void applyExternalLibrarySnapshot(const LibraryInfo&, const TrackList&, const QUuid&) override { }
};

class TestReply : public QNetworkReply
{
public:
    TestReply(const QNetworkRequest& request, QObject* parent)
        : QNetworkReply{parent}
    {
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        open(QIODevice::ReadOnly);
        QTimer::singleShot(0, this, [this]() {
            setFinished(true);
            Q_EMIT readyRead();
            Q_EMIT finished();
        });
    }
    void abort() override { }
    qint64 bytesAvailable() const override
    {
        return m_data.size() - m_position + QNetworkReply::bytesAvailable();
    }

protected:
    qint64 readData(char* data, qint64 maxSize) override
    {
        const qint64 size = std::min(maxSize, m_data.size() - m_position);
        if(size <= 0) {
            return -1;
        }
        std::memcpy(data, m_data.constData() + m_position, static_cast<size_t>(size));
        m_position += size;
        return size;
    }

private:
    QByteArray m_data{R"({"status":"ok"})"};
    qint64 m_position{0};
};

class TestNetwork : public NetworkAccessManager
{
public:
    explicit TestNetwork(SettingsManager* settings)
        : NetworkAccessManager{settings}
    { }

    std::vector<QJsonObject> requests;

    int listenCount(const QString& listenType) const
    {
        return static_cast<int>(std::ranges::count_if(requests, [&listenType](const QJsonObject& request) {
            return request.value(u"listen_type"_s).toString() == listenType;
        }));
    }

protected:
    QNetworkReply* createRequest(Operation, const QNetworkRequest& request, QIODevice* outgoingData) override
    {
        if(outgoingData) {
            requests.push_back(QJsonDocument::fromJson(outgoingData->readAll()).object());
        }
        return new TestReply(request, this);
    }
};

class RemotePlaybackTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_dir.isValid());
        ASSERT_TRUE(QDir{Utils::cachePath()}.removeRecursively());
        m_settings.createSetting<Settings::Core::PlayMode>(0, u"Playback/PlayMode"_s);
        m_settings.createSetting<Settings::Core::StopAfterCurrent>(false, u"Playback/StopAfterCurrent"_s);
        m_settings.createSetting<Settings::Core::ResetStopAfterCurrent>(false, u"Playback/ResetStopAfterCurrent"_s);
        m_settings.createSetting<Settings::Core::PlayedThreshold>(0.5, u"Playback/PlayedThreshold"_s);
        m_settings.createSetting<Settings::Core::PlayedThresholdTime>(240000, u"Playback/PlayedThresholdTime"_s);
        m_settings.createSetting<Settings::Core::RewindPreviousTrack>(false, u"Playlist/RewindPreviousTrack"_s);
        m_settings.createSetting<Settings::Core::PlaybackQueueStopWhenFinished>(
            false, u"Playback/PlaybackQueueStopWhenFinished"_s);
        m_settings.createSetting<Settings::Core::FollowPlaybackQueue>(false, u"Playback/FollowPlaybackQueue"_s);
        m_settings.createSetting<Settings::Core::PlaybackQueueMode>(0, u"Playback/PlaybackQueueMode"_s);
        m_settings.createSetting<Settings::Core::PlaybackQueueHistoryLimit>(-1,
                                                                            u"Playback/PlaybackQueueHistoryLimit"_s);
        m_settings.createSetting<Settings::Core::ClearPlaybackQueueOnStartup>(
            false, u"Playback/ClearPlaybackQueueOnStartup"_s);
        m_settings.createSetting<Settings::Core::ShuffleAlbumsGroupScript>(u"%album%"_s,
                                                                           u"Playback/ShuffleAlbumsGroupScript"_s);
        m_settings.createSetting<Settings::Core::ShuffleAlbumsSortScript>(u"%track%"_s,
                                                                          u"Playback/ShuffleAlbumsSortScript"_s);
        m_settings.createTempSetting<Settings::Core::ActiveTrack>(QVariant{});
        m_settings.createTempSetting<Settings::Core::ActiveTrackId>(-2);
        m_settings.set<Settings::Scrobbler::ScrobblingEnabled>(true);
        m_settings.set<Settings::Scrobbler::ScrobblingDelay>(10000);

        DbConnection::DbParams params;
        params.type     = u"QSQLITE"_s;
        params.filePath = m_dir.filePath(u"accounts.sqlite"_s);
        m_dbPool        = DbConnectionPool::create(params, u"scrobbler_test"_s);
        ASSERT_TRUE(m_dbPool);
        m_dbConnection = std::make_unique<DbConnectionHandler>(m_dbPool);
        ASSERT_TRUE(m_dbConnection->hasConnection());
        const DbConnectionProvider dbProvider{m_dbPool};
        DbQuery createSettings{dbProvider.db(), u"CREATE TABLE Settings (Name TEXT PRIMARY KEY, Value TEXT);"_s};
        ASSERT_TRUE(createSettings.exec());
        m_network    = std::make_shared<TestNetwork>(&m_settings);
        m_controller = std::make_unique<PlayerController>(&m_settings, nullptr);
        m_scrobbler  = std::make_unique<Scrobbler>(m_controller.get(), &m_library, m_network, &m_settings, m_dbPool);
        m_service    = m_scrobbler->addCustomService({.name       = u"Synthetic"_s,
                                                      .url        = QUrl{u"https://scrobble.example.test"_s},
                                                      .token      = u"synthetic-token"_s,
                                                      .customType = ServiceDetails::CustomType::ListenBrainz,
                                                      .isEnabled  = true});
        ASSERT_NE(m_service, nullptr);
        m_cache = m_service->findChild<ScrobblerCache*>();
        ASSERT_NE(m_cache, nullptr);
    }

    Track makeTrack(const QString& path, int id, uint64_t durationMs)
    {
        Track track{path, 0};
        track.setId(id);
        track.setDuration(durationMs);
        track.setTitle(u"Synthetic title"_s);
        track.setArtists({u"Synthetic artist"_s});
        track.generateHash();
        return track;
    }

    void play(const Track& track)
    {
        m_controller->commitCurrentTrack(track);
        m_controller->syncPlayStateFromEngine(Player::PlayState::Playing);
    }

    QTemporaryDir m_dir;
    SettingsManager m_settings{m_dir.filePath(u"settings.ini"_s)};
    ScrobblerSettings m_scrobblerSettings{&m_settings};
    DbConnectionPoolPtr m_dbPool;
    std::unique_ptr<DbConnectionHandler> m_dbConnection;
    TestMusicLibrary m_library;
    std::shared_ptr<TestNetwork> m_network;
    std::unique_ptr<PlayerController> m_controller;
    std::unique_ptr<Scrobbler> m_scrobbler;
    ScrobblerService* m_service{nullptr};
    ScrobblerCache* m_cache{nullptr};
};

TEST_F(RemotePlaybackTest, RemoteListenQueuesAndSubmitsWithoutCoreTrackPlayed)
{
    const Track track = makeTrack(u"https://music.example.test/track.flac"_s, 1, 120000);
    QSignalSpy corePlayed{m_controller.get(), &PlayerController::trackPlayed};
    play(track);
    EXPECT_EQ(m_network->listenCount(u"playing_now"_s), 1);
    m_controller->setCurrentPosition(59000);
    EXPECT_EQ(m_cache->count(), 0);
    m_controller->setCurrentPosition(60000);
    m_controller->setCurrentPosition(90000);
    EXPECT_EQ(corePlayed.count(), 0);
    ASSERT_EQ(m_cache->count(), 1);
    const auto* lastFmCache = m_scrobbler->service(u"LastFM"_s)->findChild<ScrobblerCache*>();
    ASSERT_NE(lastFmCache, nullptr);
    EXPECT_EQ(lastFmCache->count(), 1);
    const auto* libreFmCache = m_scrobbler->service(u"LibreFM"_s)->findChild<ScrobblerCache*>();
    ASSERT_NE(libreFmCache, nullptr);
    EXPECT_EQ(libreFmCache->count(), 1);
    const auto* listenBrainzCache = m_scrobbler->service(u"ListenBrainz"_s)->findChild<ScrobblerCache*>();
    ASSERT_NE(listenBrainzCache, nullptr);
    EXPECT_EQ(listenBrainzCache->count(), 1);
    m_service->submit();
    ASSERT_EQ(m_network->listenCount(u"import"_s), 1);
    const QJsonObject listen = m_network->requests.back().value(u"payload"_s).toArray().first().toObject();
    EXPECT_EQ(listen.value(u"track_metadata"_s).toObject().value(u"track_name"_s).toString(), track.title());
    ASSERT_TRUE(QTest::qWaitFor([this]() { return m_cache->count() == 0; }, 1000));
}

TEST_F(RemotePlaybackTest, AdditionalCoreSignalDoesNotDuplicateRemoteListen)
{
    const Track track = makeTrack(u"https://music.example.test/track.flac"_s, 2, 120000);
    play(track);
    m_controller->setCurrentPosition(60000);
    Q_EMIT m_controller->trackPlayed(track);
    m_controller->setCurrentPosition(90000);
    EXPECT_EQ(m_cache->count(), 1);
}

TEST_F(RemotePlaybackTest, NativeLibraryUriScrobblesWithoutSchemeSpecificHandling)
{
    play(makeTrack(u"jellyfin://synthetic-server/synthetic-user/item"_s, 10, 120000));
    EXPECT_EQ(m_network->listenCount(u"playing_now"_s), 1);
    m_controller->setCurrentPosition(60000);
    m_controller->setCurrentPosition(90000);
    EXPECT_EQ(m_cache->count(), 1);
}

TEST_F(RemotePlaybackTest, LocalListenKeepsTheCoreTrackPlayedPath)
{
    const Track track = makeTrack(u"/synthetic/local.flac"_s, 3, 120000);
    QSignalSpy corePlayed{m_controller.get(), &PlayerController::trackPlayed};
    play(track);
    m_controller->setCurrentPosition(60000);
    m_controller->setCurrentPosition(90000);
    EXPECT_EQ(corePlayed.count(), 1);
    EXPECT_EQ(m_cache->count(), 1);
}

TEST_F(RemotePlaybackTest, SeekingDoesNotCountSkippedAudio)
{
    play(makeTrack(u"https://music.example.test/seek.flac"_s, 4, 120000));
    m_controller->setCurrentTrackSeekable(true);
    m_controller->setCurrentPosition(10000);
    m_controller->seek(90000);
    m_controller->setCurrentPosition(90000);
    EXPECT_EQ(m_cache->count(), 0);
    m_controller->seek(10000);
    m_controller->setCurrentPosition(10000);
    m_controller->setCurrentPosition(60000);
    EXPECT_EQ(m_cache->count(), 1);
}

TEST_F(RemotePlaybackTest, PauseAndReplayKeepOneListenPerPlayback)
{
    play(makeTrack(u"https://music.example.test/replay.flac"_s, 5, 120000));
    m_controller->setCurrentPosition(30000);
    m_controller->syncPlayStateFromEngine(Player::PlayState::Paused);
    Q_EMIT m_controller->positionChanged(30000);
    EXPECT_EQ(m_cache->count(), 0);
    m_controller->syncPlayStateFromEngine(Player::PlayState::Playing);
    m_controller->setCurrentPosition(60000);
    EXPECT_EQ(m_cache->count(), 1);
    m_controller->syncPlayStateFromEngine(Player::PlayState::Stopped);
    m_controller->syncPlayStateFromEngine(Player::PlayState::Playing);
    m_controller->setCurrentPosition(60000);
    EXPECT_EQ(m_cache->count(), 2);
}

TEST_F(RemotePlaybackTest, ThresholdCrossedAfterPauseRequestIsSubmittedOnce)
{
    QSignalSpy corePlayed{m_controller.get(), &PlayerController::trackPlayed};
    play(makeTrack(u"https://music.example.test/pause.flac"_s, 11, 120000));
    m_controller->setCurrentPosition(59000);
    m_controller->pause();
    m_controller->setCurrentPosition(60000);
    m_controller->syncPlayStateFromEngine(Player::PlayState::Paused);
    m_controller->play();
    m_controller->setCurrentPosition(90000);
    EXPECT_EQ(corePlayed.count(), 0);
    EXPECT_EQ(m_cache->count(), 1);
    const auto* lastFmCache = m_scrobbler->service(u"LastFM"_s)->findChild<ScrobblerCache*>();
    ASSERT_NE(lastFmCache, nullptr);
    EXPECT_EQ(lastFmCache->count(), 1);
    const auto* libreFmCache = m_scrobbler->service(u"LibreFM"_s)->findChild<ScrobblerCache*>();
    ASSERT_NE(libreFmCache, nullptr);
    EXPECT_EQ(libreFmCache->count(), 1);
    const auto* listenBrainzCache = m_scrobbler->service(u"ListenBrainz"_s)->findChild<ScrobblerCache*>();
    ASSERT_NE(listenBrainzCache, nullptr);
    EXPECT_EQ(listenBrainzCache->count(), 1);
}

TEST_F(RemotePlaybackTest, RestoredPausedPlaybackIsNotSubmittedAfterResume)
{
    const Track track = makeTrack(u"https://music.example.test/restored-paused.flac"_s, 12, 120000);
    m_controller->commitCurrentTrack(track);
    m_controller->syncPlayStateFromEngine(Player::PlayState::Paused);
    m_controller->restorePlaybackProgress(90000, 90000);
    m_controller->play();
    m_controller->setCurrentPosition(100000);
    EXPECT_EQ(m_cache->count(), 0);
}

TEST_F(RemotePlaybackTest, RestoredCompletedPlaybackIsNotSubmittedAgain)
{
    const Track track = makeTrack(u"https://music.example.test/restored.flac"_s, 6, 120000);
    m_controller->commitCurrentTrack(track);
    m_controller->restorePlaybackProgress(90000, 90000);
    EXPECT_EQ(m_cache->count(), 0);
    m_controller->syncPlayStateFromEngine(Player::PlayState::Playing);
    m_controller->setCurrentPosition(100000);
    EXPECT_EQ(m_cache->count(), 0);
}

TEST_F(RemotePlaybackTest, EnablingAfterThresholdDoesNotRetroactivelySubmit)
{
    m_settings.set<Settings::Scrobbler::ScrobblingEnabled>(false);
    play(makeTrack(u"https://music.example.test/disabled.flac"_s, 7, 120000));
    m_controller->setCurrentPosition(60000);
    m_settings.set<Settings::Scrobbler::ScrobblingEnabled>(true);
    m_controller->setCurrentPosition(90000);
    EXPECT_EQ(m_cache->count(), 0);
}

TEST_F(RemotePlaybackTest, ShortTracksAndMissingArtistsRemainIneligible)
{
    play(makeTrack(u"https://music.example.test/short.flac"_s, 8, 29999));
    m_controller->setCurrentPosition(20000);
    EXPECT_EQ(m_cache->count(), 0);
    Track track = makeTrack(u"https://music.example.test/untagged.flac"_s, 9, 120000);
    track.setArtists({});
    play(track);
    m_controller->setCurrentPosition(60000);
    EXPECT_EQ(m_cache->count(), 0);
    EXPECT_EQ(m_network->listenCount(u"playing_now"_s), 0);
}
} // namespace
} // namespace Fooyin::Scrobbler::Testing
