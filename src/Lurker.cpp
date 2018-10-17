/**
 * @file Lurker.cpp
 *
 * This module contains the implementation of the Lurker class.
 *
 * © 2018 by Richard Walters
 */

#include "Lurker.hpp"
#include "TimeKeeper.hpp"

#include <condition_variable>
#include <map>
#include <mutex>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <SystemAbstractions/DiagnosticsSender.hpp>
#include <SystemAbstractions/File.hpp>
#include <SystemAbstractions/StringExtensions.hpp>
#include <thread>
#include <time.h>
#include <Twitch/Messaging.hpp>
#include <TwitchNetworkTransport/Connection.hpp>

namespace {

    /**
     * This is the number of milliseconds to wait between rounds of polling
     * in the worker thread of the chat room.
     */
    constexpr unsigned int WORKER_POLLING_PERIOD_MILLISECONDS = 50;

    /**
     * This function constructs a human-readable timestamp from the given time
     * information.
     *
     * @param[in] time
     *     This is the time in seconds since the UNIX epoch.
     *
     * @param[in] milliseconds
     *     This is a fractional number of milliseconds of
     *     the time past the given number of seconds.
     *
     * @return
     *     A human-readable timestamp constructed from the given time
     *     information is returned.
     */
    std::string FormatTimestamp(
        time_t time,
        unsigned int milliseconds
    ) {
        char buffer[13];
        (void)strftime(buffer, sizeof(buffer), "%T", localtime(&time));
        (void)sprintf(buffer + 8, ".%03u", milliseconds);
        return buffer;
    }

}

/**
 * This contains the private properties of a Lurker class instance.
 */
struct Lurker::Impl
    : public Twitch::Messaging::User
{
    // Properties

    /**
     * This is a helper object used to generate and publish
     * diagnostic messages.
     */
    SystemAbstractions::DiagnosticsSender diagnosticsSender;

    /**
     * This is used to connect to Twitch chat and exchange messages
     * with it.
     */
    Twitch::Messaging tmi;

    /**
     * These are the names of the channels the bot should join.
     */
    std::vector< std::string > channelsToJoin;

    /**
     * This is used to track elapsed real time.
     */
    std::shared_ptr< TimeKeeper > timeKeeper = std::make_shared< TimeKeeper >();

    /**
     * This is used to synchronize access to the object.
     */
    std::mutex mutex;

    /**
     * This is used to signal when any condition for which the main thread
     * may be waiting has occurred.
     */
    std::condition_variable mainThreadEvent;

    /**
     * This flag is set when the Twitch messaging interface indicates
     * that the bot has been logged out of Twitch.
     */
    bool loggedOut = false;

    /**
     * This is used to notify the worker thread about
     * any change that should cause it to wake up.
     */
    std::condition_variable_any workerWakeCondition;

    /**
     * This is used to have the bot
     * take action at certain points in time.
     */
    std::thread workerThread;

    /**
     * This flag indicates whether or not the worker thread should stop.
     */
    bool stopWorker = false;

    // Methods

    /**
     * This is the default constructor.
     */
    Impl()
        : diagnosticsSender("Lurker")
    {
    }

   /**
     * This is method starts the worker thread if it isn't running.
     */
    void StartWorker() {
        if (workerThread.joinable()) {
            return;
        }
        stopWorker = false;
        workerThread = std::thread(&Impl::Worker, this);
    }

    /**
     * This method stops the worker thread if it's running.
     */
    void StopWorker() {
        if (!workerThread.joinable()) {
            return;
        }
        {
            std::lock_guard< decltype(mutex) > lock(mutex);
            stopWorker = true;
            workerWakeCondition.notify_all();
        }
        workerThread.join();
    }

    /**
     * This function is called in a separate thread to have the bot
     * take action at certain points in time.
     */
    void Worker() {
        std::unique_lock< decltype(mutex) > lock(mutex);
        while (!stopWorker) {
            workerWakeCondition.wait_for(
                lock,
                std::chrono::milliseconds(WORKER_POLLING_PERIOD_MILLISECONDS),
                [this]{ return stopWorker; }
            );
            const auto now = timeKeeper->GetCurrentTime();
        }
    }

    // Twitch::Messaging::User

    virtual void Doom() override {
        diagnosticsSender.SendDiagnosticInformationString(2, "** SERVER DISCONNECT IMMINENT **");
    }

    virtual void LogIn() override {
        diagnosticsSender.SendDiagnosticInformationString(1, "Logged in.");
        for (const auto& channel: channelsToJoin) {
            tmi.Join(channel);
        }
        StartWorker();
    }

    virtual void LogOut() override {
        if (loggedOut) {
            return;
        }
        StopWorker();
        diagnosticsSender.SendDiagnosticInformationString(1, "Logged out.");
        std::lock_guard< decltype(mutex) > lock(mutex);
        loggedOut = true;
        mainThreadEvent.notify_one();
    }

    virtual void Join(
        Twitch::Messaging::MembershipInfo&& membershipInfo
    ) override {
        diagnosticsSender.SendDiagnosticInformationFormatted(
            1,
            "[%s] +%s",
            membershipInfo.channel.c_str(),
            membershipInfo.user.c_str()
        );
    }

    virtual void Leave(
        Twitch::Messaging::MembershipInfo&& membershipInfo
    ) override {
        diagnosticsSender.SendDiagnosticInformationFormatted(
            1,
            "[%s] -%s",
            membershipInfo.channel.c_str(),
            membershipInfo.user.c_str()
        );
    }

    virtual void Message(
        Twitch::Messaging::MessageInfo&& messageInfo
    ) override {
        std::string userDisplayName;
        if (messageInfo.tags.displayName.empty()) {
            userDisplayName = messageInfo.user;
        } else {
            userDisplayName = messageInfo.tags.displayName;
        }
        const auto timestamp = FormatTimestamp(
            messageInfo.tags.timestamp,
            messageInfo.tags.timeMilliseconds
        );
        diagnosticsSender.SendDiagnosticInformationFormatted(
            1, "[%s %s] %s: %s",
            timestamp.c_str(),
            messageInfo.channel.c_str(),
            userDisplayName.c_str(),
            messageInfo.messageContent.c_str()
        );
    }

    virtual void Notice(
        Twitch::Messaging::NoticeInfo&& noticeInfo
    ) override {
        diagnosticsSender.SendDiagnosticInformationFormatted(
            1, "** Server NOTICE %s: %s **",
            noticeInfo.id.c_str(),
            noticeInfo.message.c_str()
        );
    }

    virtual void Host(
        Twitch::Messaging::HostInfo&& hostInfo
    ) override {
        if (hostInfo.on) {
            diagnosticsSender.SendDiagnosticInformationFormatted(
                1, "[%s] Now hosting %s (%zu viewers)",
                hostInfo.hosting.c_str(),
                hostInfo.beingHosted.c_str(),
                hostInfo.viewers
            );
        } else {
            diagnosticsSender.SendDiagnosticInformationFormatted(
                1, "[%s] No longer hosting anyone",
                hostInfo.hosting.c_str()
            );
        }
    }

    virtual void RoomModeChange(
        Twitch::Messaging::RoomModeChangeInfo&& roomModeChangeInfo
    ) override {
        diagnosticsSender.SendDiagnosticInformationFormatted(
            1, "[%s] Room mode %s: %d",
            roomModeChangeInfo.channelName.c_str(),
            roomModeChangeInfo.mode.c_str(),
            roomModeChangeInfo.parameter
        );
    }

    virtual void Clear(
        Twitch::Messaging::ClearInfo&& clearInfo
    ) {
        switch (clearInfo.type) {
            case Twitch::Messaging::ClearInfo::Type::ClearAll: {
                diagnosticsSender.SendDiagnosticInformationFormatted(
                    1, "[%s] ** CLEAR CHAT **",
                    clearInfo.channel.c_str()
                );
            } break;

            case Twitch::Messaging::ClearInfo::Type::ClearMessage: {
                diagnosticsSender.SendDiagnosticInformationFormatted(
                    1, "[%s] Message from %s has been deleted (was \"%s\")",
                    clearInfo.channel.c_str(),
                    clearInfo.user.c_str(),
                    clearInfo.offendingMessageContent.c_str()
                );
            } break;

            case Twitch::Messaging::ClearInfo::Type::Timeout: {
                diagnosticsSender.SendDiagnosticInformationFormatted(
                    1, "[%s] User %s has been timed out for %zu seconds; reason: %s",
                    clearInfo.channel.c_str(),
                    clearInfo.user.c_str(),
                    clearInfo.duration,
                    clearInfo.reason.c_str()
                );
            } break;

            case Twitch::Messaging::ClearInfo::Type::Ban: {
                diagnosticsSender.SendDiagnosticInformationFormatted(
                    1, "[%s] User %s has been banned from the channel; reason: %s",
                    clearInfo.channel.c_str(),
                    clearInfo.user.c_str(),
                    clearInfo.reason.c_str()
                );
            } break;

            default: {
                diagnosticsSender.SendDiagnosticInformationFormatted(
                    SystemAbstractions::DiagnosticsSender::Levels::ERROR,
                    "[%s] ** Unknown type of clear announcement **",
                    clearInfo.channel.c_str()
                );
            } break;
        }
    }

    virtual void Sub(
        Twitch::Messaging::SubInfo&& subInfo
    ) override {
        switch (subInfo.type) {
            case Twitch::Messaging::SubInfo::Type::Sub: {
                diagnosticsSender.SendDiagnosticInformationFormatted(
                    1, "[%s] SUB (new: %s) %s: %s [%s]",
                    subInfo.channel.c_str(),
                    subInfo.planName.c_str(),
                    subInfo.user.c_str(),
                    subInfo.systemMessage.c_str(),
                    subInfo.userMessage.c_str()
                );
            } break;

            case Twitch::Messaging::SubInfo::Type::Resub: {
                diagnosticsSender.SendDiagnosticInformationFormatted(
                    1, "[%s] SUB (renew %zu: %s) %s: %s [%s]",
                    subInfo.channel.c_str(),
                    subInfo.months,
                    subInfo.planName.c_str(),
                    subInfo.user.c_str(),
                    subInfo.systemMessage.c_str(),
                    subInfo.userMessage.c_str()
                );
            } break;

            case Twitch::Messaging::SubInfo::Type::Gifted: {
                diagnosticsSender.SendDiagnosticInformationFormatted(
                    1, "[%s] SUB (gift from %s [%zu sent total]: %s) %s: %s [%s]",
                    subInfo.channel.c_str(),
                    subInfo.user.c_str(),
                    subInfo.senderCount,
                    subInfo.planName.c_str(),
                    subInfo.recipientDisplayName.c_str(),
                    subInfo.systemMessage.c_str(),
                    subInfo.userMessage.c_str()
                );
            } break;

            case Twitch::Messaging::SubInfo::Type::MysteryGift: {
                diagnosticsSender.SendDiagnosticInformationFormatted(
                    1, "[%s] SUB (mystery gift to %zu users from %s [%zu sent total]) %s [%s]",
                    subInfo.channel.c_str(),
                    subInfo.massGiftCount,
                    subInfo.user.c_str(),
                    subInfo.senderCount,
                    subInfo.systemMessage.c_str(),
                    subInfo.userMessage.c_str()
                );
            } break;

            case Twitch::Messaging::SubInfo::Type::Unknown:
            default: {
                diagnosticsSender.SendDiagnosticInformationFormatted(
                    SystemAbstractions::DiagnosticsSender::Levels::ERROR,
                    "[%s] ** Unknown type of sub announcement **",
                    subInfo.channel.c_str()
                );
            } break;
        }
    }

};

Lurker::~Lurker() noexcept = default;

Lurker::Lurker()
    : impl_(new Impl())
{
}

void Lurker::Configure(
    SystemAbstractions::DiagnosticsSender::DiagnosticMessageDelegate diagnosticMessageDelegate
) {
    impl_->diagnosticsSender.SubscribeToDiagnostics(diagnosticMessageDelegate, 0);
    impl_->tmi.SubscribeToDiagnostics(impl_->diagnosticsSender.Chain(), 0);
    impl_->tmi.SetConnectionFactory(
        [diagnosticMessageDelegate]() -> std::shared_ptr< Twitch::Connection > {
            auto connection = std::make_shared< TwitchNetworkTransport::Connection >();
            connection->SubscribeToDiagnostics(diagnosticMessageDelegate, 0);
            SystemAbstractions::File caCertsFile(
                SystemAbstractions::File::GetExeParentDirectory()
                + "/cert.pem"
            );
            if (!caCertsFile.Open()) {
                diagnosticMessageDelegate(
                    "Lurker",
                    SystemAbstractions::DiagnosticsSender::Levels::ERROR,
                    SystemAbstractions::sprintf(
                        "unable to open root CA certificates file '%s'",
                        caCertsFile.GetPath().c_str()
                    )
                );
                return nullptr;
            }
            std::vector< uint8_t > caCertsBuffer(caCertsFile.GetSize());
            if (caCertsFile.Read(caCertsBuffer) != caCertsBuffer.size()) {
                diagnosticMessageDelegate(
                    "Lurker",
                    SystemAbstractions::DiagnosticsSender::Levels::ERROR,
                    "unable to read root CA certificates file"
                );
                return nullptr;
            }
            const std::string caCerts(
                (const char*)caCertsBuffer.data(),
                caCertsBuffer.size()
            );
            connection->SetCaCerts(caCerts);
            return connection;
        }
    );
    impl_->tmi.SetTimeKeeper(impl_->timeKeeper);
    impl_->tmi.SetUser(
        std::shared_ptr< Twitch::Messaging::User >(
            impl_.get(),
            [](Twitch::Messaging::User*){}
        )
    );
    impl_->diagnosticsSender.SendDiagnosticInformationString(3, "Configured.");
}

void Lurker::InitiateLogIn(const std::vector< std::string >& channels) {
    impl_->channelsToJoin = channels;
    impl_->tmi.LogInAnonymously();
}

void Lurker::InitiateLogOut() {
    impl_->diagnosticsSender.SendDiagnosticInformationString(3, "Exiting...");
    impl_->tmi.LogOut("Bye! BibleThump");
}

bool Lurker::AwaitLogOut() {
    std::unique_lock< decltype(impl_->mutex) > lock(impl_->mutex);
    return impl_->mainThreadEvent.wait_for(
        lock,
        std::chrono::milliseconds(250),
        [this]{ return impl_->loggedOut; }
    );
}
