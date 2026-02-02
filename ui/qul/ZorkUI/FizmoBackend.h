/*
 * FizmoBackend.h
 *
 * Qt for MCUs singleton that bridges QML UI to the fizmo interpreter.
 * Uses Qul::EventQueue for thread-safe updates from the fizmo FreeRTOS task.
 *
 * Text buffer management and polling logic are delegated to the shared
 * ui/common modules (FizmoTextBuffer, FizmoPoller).
 *
 * Note: Qt for MCUs doesn't support Qul::String in Property<T> directly.
 * We use a version counter pattern - QML rebinds when version changes
 * and calls methods to get string content.
 */

#ifndef FIZMOBACKEND_H
#define FIZMOBACKEND_H

#include <qul/singleton.h>
#include <qul/property.h>
#include <qul/signal.h>
#include <qul/eventqueue.h>
#include <qul/timer.h>
#include <qul/private/unicodestring.h>

#include "DisplayConfig.h"
#include "FizmoTextBuffer.h"
#include "FizmoPoller.h"

/*
 * Event types for communication from fizmo task to Qt task
 */
enum class FizmoEventType {
    OutputText,      // New text output available
    InputRequested,  // Fizmo wants line input
    CharRequested,   // Fizmo wants single character
    StatusUpdate,    // Status line changed
    GameExited       // Game has ended
};

struct FizmoEvent {
    FizmoEventType type;
    // For OutputText: UTF-8 text chunk (small buffer, events posted frequently)
    char text[64];
    // For StatusUpdate
    char statusRoom[48];
    char statusScore[24];
};

/*
 * FizmoBackend - singleton exposed to QML
 *
 * QML usage:
 *   // Trigger rebind when outputVersion changes, call getOutputText()
 *   property int outVer: FizmoBackend.outputVersion
 *   Text { text: FizmoBackend.getOutputText() }
 *
 *   TextInput { onAccepted: FizmoBackend.submitLine(text) }
 *   visible: FizmoBackend.waitingForInput
 */
class FizmoBackend : public Qul::Singleton<FizmoBackend>
{
public:
    FizmoBackend();

    /*
     * Display configuration properties (read-only, set at build time)
     */
    Qul::Property<int> screenWidth{DISPLAY_WIDTH};
    Qul::Property<int> screenHeight{DISPLAY_HEIGHT};
    Qul::Property<bool> showVirtualKeyboard{DISPLAY_SHOW_VKEYBOARD != 0};
    Qul::Property<bool> vkeyboardAlways{DISPLAY_VKEYBOARD_ALWAYS != 0};
    Qul::Property<int> fontSize{DISPLAY_FONT_SIZE};
    Qul::Property<int> statusHeight{DISPLAY_STATUS_HEIGHT};
    Qul::Property<int> inputHeight{DISPLAY_INPUT_HEIGHT};
    Qul::Property<int> margin{DISPLAY_MARGIN};

    /*
     * Properties exposed to QML
     */

    // Version counter - incremented when output text changes
    Qul::Property<int> outputVersion;

    // Version counter for status line changes
    Qul::Property<int> statusVersion;

    // Version counter for command text changes (triggers QML rebind)
    Qul::Property<int> commandVersion;

    // True when fizmo is waiting for line input
    Qul::Property<bool> waitingForInput;

    // True when fizmo is waiting for single character
    Qul::Property<bool> waitingForChar;

    // True when game has ended
    Qul::Property<bool> gameExited;

    /*
     * Signals
     */

    // Emitted when new output text is appended
    Qul::Signal<void()> outputAppended;

    /*
     * Methods callable from QML - string getters
     */

    const char* getOutputText() const;
    const char* getStatusRoom() const;
    const char* getStatusScore() const;

    /*
     * Methods callable from QML - input submission
     */

    void submitLine(const Qul::Private::String &text);
    void submitChar(int ch);

    // Helper for backspace - Qt for MCUs has limited string methods
    Qul::Private::String removeLastChar(const Qul::Private::String &text);

    // Clear the output text buffer
    void clearOutput();

    // Command text management (to avoid QML string concatenation issues)
    void appendCommandChar(const Qul::Private::String &key);
    void commandBackspace();
    void submitCommand();
    const char* getCommandText() const;

    /*
     * Methods called from C++ (fizmo task via event queue)
     */

    // Post an event from the fizmo task (thread-safe)
    static void postEvent(const FizmoEvent &event);

    // Append text to output (called internally and from event handler)
    void appendOutput(const char *text);

private:
    friend class FizmoEventQueue;

    // Shared text buffer (output, status, command) — from ui/common
    FizmoTextBuffer m_textBuffer;

    // Shared poller state — from ui/common
    FizmoPollerState m_pollerState;

    // Timer for polling fizmo output queue
    Qul::Timer m_pollTimer;

    // Poll for output from fizmo (called by timer)
    void pollFizmoOutput();
};

/*
 * Event queue for thread-safe communication from fizmo task
 */
class FizmoEventQueue : public Qul::EventQueue<FizmoEvent>
{
public:
    void onEvent(const FizmoEvent &event) override;
};

#endif // FIZMOBACKEND_H
