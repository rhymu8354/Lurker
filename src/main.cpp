/**
 * @file main.cpp
 *
 * This module holds the main() function, which is the entrypoint
 * to the program.
 *
 * © 2018 by Richard Walters
 */

#include "Lurker.hpp"

#include <condition_variable>
#include <mutex>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <SystemAbstractions/DiagnosticsStreamReporter.hpp>
#include <SystemAbstractions/StringExtensions.hpp>
#include <thread>

namespace {

    /**
     * This function prints to the standard error stream information
     * about how to use this program.
     */
    void PrintUsageInformation() {
        fprintf(
            stderr,
            (
                "Usage: Lurker <CHANNEL>..\n"
                "\n"
                "Connect to Twitch chat and listen for messages on one or more channels.\n"
                "\n"
                "  CHANNEL     Name of a Twitch channel to join\n"
            )
        );
    }

    /**
     * This flag indicates whether or not the web client should shut down.
     */
    bool shutDown = false;

    /**
     * This contains variables set through the operating system environment
     * or the command-line arguments.
     */
    struct Environment {
        /**
         * These are the names of the Twitch channels to join.
         */
        std::vector< std::string > channels;
    };

    /**
     * This function is set up to be called when the SIGINT signal is
     * received by the program.  It just sets the "shutDown" flag
     * and relies on the program to be polling the flag to detect
     * when it's been set.
     *
     * @param[in] sig
     *     This is the signal for which this function was called.
     */
    void InterruptHandler(int) {
        shutDown = true;
    }

    /**
     * This function updates the program environment to incorporate
     * any applicable command-line arguments.
     *
     * @param[in] argc
     *     This is the number of command-line arguments given to the program.
     *
     * @param[in] argv
     *     This is the array of command-line arguments given to the program.
     *
     * @param[in,out] environment
     *     This is the environment to update.
     *
     * @param[in] diagnosticMessageDelegate
     *     This is the function to call to publish any diagnostic messages.
     *
     * @return
     *     An indication of whether or not the function succeeded is returned.
     */
    bool ProcessCommandLineArguments(
        int argc,
        char* argv[],
        Environment& environment,
        SystemAbstractions::DiagnosticsSender::DiagnosticMessageDelegate diagnosticMessageDelegate
    ) {
        size_t state = 0;
        for (int i = 1; i < argc; ++i) {
            const std::string arg(argv[i]);
            switch (state) {
                case 0: { // next argument
                    environment.channels.push_back(arg);
                    state = 0;
                } break;
            }
        }
        if (environment.channels.empty()) {
            diagnosticMessageDelegate(
                "Lurker",
                SystemAbstractions::DiagnosticsSender::Levels::ERROR,
                "no channels given"
            );
            return false;
        }
        return true;
    }

}

/**
 * This function is the entrypoint of the program.
 * It just sets up the bot and has it log into Twitch.  At that point, the
 * bot will interact with Twitch using its callbacks.  It registers the
 * SIGINT signal to know when the bot should be shut down.
 *
 * The program is terminated after the SIGINT signal is caught.
 *
 * @param[in] argc
 *     This is the number of command-line arguments given to the program.
 *
 * @param[in] argv
 *     This is the array of command-line arguments given to the program.
 */
int main(int argc, char* argv[]) {
#ifdef _WIN32
    //_crtBreakAlloc = 18;
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif /* _WIN32 */
    srand((unsigned int)time(NULL));
    const auto previousInterruptHandler = signal(SIGINT, InterruptHandler);
    Environment environment;
    (void)setbuf(stdout, NULL);
    const auto diagnosticsPublisher = SystemAbstractions::DiagnosticsStreamReporter(stdout, stderr);
    if (!ProcessCommandLineArguments(argc, argv, environment, diagnosticsPublisher)) {
        PrintUsageInformation();
        return EXIT_FAILURE;
    }
    const auto lurker = std::make_shared< Lurker >();
    lurker->Configure(diagnosticsPublisher);
    lurker->InitiateLogIn(environment.channels);
    while (!shutDown) {
        if (lurker->AwaitLogOut()) {
            break;
        }
    }
    (void)signal(SIGINT, previousInterruptHandler);
    lurker->InitiateLogOut();
    lurker->AwaitLogOut();
    return EXIT_SUCCESS;
}
