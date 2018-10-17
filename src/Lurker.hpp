#ifndef LURKER_HPP
#define LURKER_HPP

/**
 * @file Lurker.hpp
 *
 * This module declares the Lurker implementation.
 *
 * © 2018 by Richard Walters
 */

#include <memory>
#include <string>
#include <SystemAbstractions/DiagnosticsSender.hpp>
#include <vector>

/**
 * This represents the chat bot itself.  It handles any callbacks
 * received from the Twitch messaging interface.
 */
class Lurker {
    // Lifecycle Methods
public:
    ~Lurker() noexcept;
    Lurker(const Lurker&) = delete;
    Lurker(Lurker&&) noexcept = delete;
    Lurker& operator=(const Lurker&) = delete;
    Lurker& operator=(Lurker&&) noexcept = delete;

    // Public Methods
public:
    /**
     * This is the constructor of the class.
     */
    Lurker();

    /**
     * This method sets up the bot to interact with the app and with
     * Twitch chat.
     *
     * @param[in] diagnosticMessageDelegate
     *     This is the function to call to publish any diagnostic messages.
     */
    void Configure(
        SystemAbstractions::DiagnosticsSender::DiagnosticMessageDelegate diagnosticMessageDelegate
    );

    /**
     * This method is called to initiate logging into Twitch chat.
     *
     * @param[in] channels
     *     These are the channels in which to lurk in Twitch chat.
     */
    void InitiateLogIn(const std::vector< std::string >& channels);

    /**
     * This method is called to initiate logging out of Twitch chat.
     */
    void InitiateLogOut();

    /**
     * This method waits up to a quarter second for the bot to be
     * logged out of Twitch.
     *
     * @return
     *     An indication of whether or not the bot has been logged
     *     out of Twitch is returned.
     */
    bool AwaitLogOut();

    // Private properties
private:
    /**
     * This is the type of structure that contains the private
     * properties of the instance.  It is defined in the implementation
     * and declared here to ensure that it is scoped inside the class.
     */
    struct Impl;

    /**
     * This contains the private properties of the instance.
     */
    std::unique_ptr< Impl > impl_;
};

#endif /* LURKER_HPP */
