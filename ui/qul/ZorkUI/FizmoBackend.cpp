/*
 * FizmoBackend.cpp
 *
 * Qt for MCUs backend implementation.
 * Delegates text buffer management and polling to ui/common modules.
 * This file contains only Qt-specific glue: Qul::Property updates,
 * Qul::Timer setup, Qul::EventQueue handling, and Qul::Private::String
 * conversions.
 */

#include "FizmoBackend.h"

#include <cstring>
#include <cstdlib>

#if defined(DESKTOP_STUB)
// Desktop stub: fizmo_stub.c provides all fizmo_*() functions.
// No additional includes needed — linked at build time.
#define IS_DESKTOP_BUILD 1
#define FIZMO_INPUT_BUFFER_SIZE 256
#elif defined(USE_FIZMO_BRIDGE)
extern "C" {
#include "fizmo_bridge.h"
}
#define IS_DESKTOP_BUILD 1
#define FIZMO_INPUT_BUFFER_SIZE 256
#else
extern "C" {
#include "fizmo_rtos_bridge.h"
}
#define IS_DESKTOP_BUILD 0
#endif

// Global event queue instance
static FizmoEventQueue s_eventQueue;

// Poll interval in milliseconds
static const int POLL_INTERVAL_MS = 50;

// Story file path - can be overridden via ZORK_STORY_PATH environment variable
#ifndef ZORK_STORY_PATH
#define ZORK_STORY_PATH "zork1.z3"
#endif

FizmoBackend::FizmoBackend()
    : outputVersion(0)
    , statusVersion(0)
    , commandVersion(0)
    , waitingForInput(false)
    , waitingForChar(false)
    , gameExited(false)
{
    fizmo_text_buffer_init(&m_textBuffer);
    fizmo_poller_init(&m_pollerState);

#if defined(USE_FIZMO_BRIDGE)
    const char *storyPath = ZORK_STORY_PATH;
    const char *envPath = getenv("ZORK_STORY_PATH");
    if (envPath != nullptr && envPath[0] != '\0') {
        storyPath = envPath;
    }
    if (fizmo_bridge_init(storyPath) == 0) {
        fizmo_start_interpreter();
    }
#endif

    // Set up polling timer
    m_pollTimer.setInterval(POLL_INTERVAL_MS);
    m_pollTimer.setSingleShot(false);
    m_pollTimer.onTimeout([this]() {
        pollFizmoOutput();
    });
    m_pollTimer.start();
}

const char* FizmoBackend::getOutputText() const
{
    return fizmo_text_buffer_get_output(&m_textBuffer);
}

const char* FizmoBackend::getStatusRoom() const
{
    return fizmo_text_buffer_get_status_room(&m_textBuffer);
}

const char* FizmoBackend::getStatusScore() const
{
    return fizmo_text_buffer_get_status_score(&m_textBuffer);
}

void FizmoBackend::submitLine(const Qul::Private::String &text)
{
    // Extract UTF-8 or Latin-1 from Qt string
    const char *utf8 = text.maybeUtf8();
    const char *latin1 = text.maybeLatin1();
    const char *str = utf8 ? utf8 : latin1;

    if (str == nullptr) {
        // Fallback for unusual string formats
        fizmo_submit_line("");
        return;
    }

    int len = text.rawLength();
    if (len >= FIZMO_INPUT_BUFFER_SIZE - 1) return;

    char buffer[FIZMO_INPUT_BUFFER_SIZE];
    if (len > 0) {
        memcpy(buffer, str, len);
    }
    buffer[len] = '\0';

    fizmo_poller_submit_line(&m_pollerState, &m_textBuffer, buffer,
                             IS_DESKTOP_BUILD != 0);

    outputVersion.setValue(outputVersion.value() + 1);
}

void FizmoBackend::submitChar(int ch)
{
    fizmo_poller_submit_char(&m_pollerState, static_cast<uint32_t>(ch));
}

Qul::Private::String FizmoBackend::removeLastChar(const Qul::Private::String &text)
{
    int len = text.rawLength();
    if (len <= 0) {
        return Qul::Private::String();
    }

    const char *utf8 = text.maybeUtf8();
    if (utf8 != nullptr) {
        int newLen = len - 1;
        while (newLen > 0 && (utf8[newLen] & 0xC0) == 0x80) {
            newLen--;
        }
        return Qul::Private::String(utf8, newLen);
    }

    const char *latin1 = text.maybeLatin1();
    if (latin1 != nullptr) {
        return Qul::Private::String(latin1, len - 1);
    }

    return Qul::Private::String();
}

void FizmoBackend::clearOutput()
{
    fizmo_text_buffer_clear_output(&m_textBuffer);
    outputVersion.setValue(outputVersion.value() + 1);
}

void FizmoBackend::postEvent(const FizmoEvent &event)
{
    s_eventQueue.postEvent(event);
}

void FizmoBackend::appendOutput(const char *text)
{
    if (fizmo_text_buffer_append_output(&m_textBuffer, text)) {
        outputVersion.setValue(outputVersion.value() + 1);
    }
}

void FizmoBackend::pollFizmoOutput()
{
    int changed = fizmo_poller_poll(&m_pollerState, &m_textBuffer);

    if (changed & FIZMO_CHANGED_OUTPUT) {
        outputVersion.setValue(outputVersion.value() + 1);
    }

    if (changed & FIZMO_CHANGED_INPUT) {
        waitingForInput.setValue(m_pollerState.waitingForInput);
    }

    if (changed & FIZMO_CHANGED_CHAR) {
        waitingForChar.setValue(m_pollerState.waitingForChar);
    }

    if (changed & FIZMO_CHANGED_STATUS) {
        statusVersion.setValue(statusVersion.value() + 1);
    }

    if (changed & FIZMO_CHANGED_EXITED) {
        gameExited.setValue(true);
    }
}

/*
 * Event queue handler - processes events posted from fizmo task
 */
void FizmoEventQueue::onEvent(const FizmoEvent &event)
{
    FizmoBackend &backend = FizmoBackend::instance();

    switch (event.type) {
        case FizmoEventType::OutputText:
            backend.appendOutput(event.text);
            break;

        case FizmoEventType::InputRequested:
            backend.waitingForInput.setValue(true);
            backend.m_pollerState.waitingForInput = true;
            break;

        case FizmoEventType::CharRequested:
            backend.waitingForChar.setValue(true);
            backend.m_pollerState.waitingForChar = true;
            break;

        case FizmoEventType::StatusUpdate:
            fizmo_text_buffer_set_status(&backend.m_textBuffer,
                                         event.statusRoom, event.statusScore);
            backend.statusVersion.setValue(backend.statusVersion.value() + 1);
            break;

        case FizmoEventType::GameExited:
            backend.gameExited.setValue(true);
            backend.m_pollerState.gameExited = true;
            break;
    }
}

void FizmoBackend::appendCommandChar(const Qul::Private::String &key)
{
    const char *utf8 = key.maybeUtf8();
    const char *latin1 = key.maybeLatin1();
    const char *str = utf8 ? utf8 : latin1;

    if (str) {
        int len = key.rawLength();
        fizmo_text_buffer_append_command_char(&m_textBuffer, str, len);
        commandVersion.setValue(commandVersion.value() + 1);
    }
}

void FizmoBackend::commandBackspace()
{
    fizmo_text_buffer_command_backspace(&m_textBuffer);
    commandVersion.setValue(commandVersion.value() + 1);
}

void FizmoBackend::submitCommand()
{
    fizmo_text_buffer_mark_output_start(&m_textBuffer);

    const char *cmd = fizmo_text_buffer_get_command(&m_textBuffer);
    int len = m_textBuffer.commandLength;

    // Echo the command to output
    if (len > 0) {
        char echo[FIZMO_COMMAND_BUFFER_SIZE + 4];
        echo[0] = ' ';
        memcpy(echo + 1, cmd, len);
        echo[len + 1] = '\n';
        echo[len + 2] = '\0';
        appendOutput(echo);
    }

    fizmo_submit_line(cmd);

    fizmo_text_buffer_clear_command(&m_textBuffer);
    commandVersion.setValue(commandVersion.value() + 1);
}

const char* FizmoBackend::getCommandText() const
{
    return fizmo_text_buffer_get_command(&m_textBuffer);
}

// Register the singleton
QUL_SINGLETON(FizmoBackend)
