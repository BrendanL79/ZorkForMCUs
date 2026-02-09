/*
 * fizmo_bridge.cpp
 *
 * Desktop implementation of fizmo bridge using std::thread.
 */

#include "fizmo_bridge.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstring>
#include <cstdio>

// libfizmo headers
extern "C" {
#include "interpreter/fizmo.h"
#include "tools/filesys.h"
#include "tools/filesys_c.h"
#include "tools/types.h"
#include "filesys_interface/filesys_interface.h"
extern z_ucs last_savegame_filename[];
}

// Configuration
static const size_t OUTPUT_BUFFER_SIZE = 8192;
static const size_t INPUT_BUFFER_SIZE = 256;

// Output ring buffer
static uint32_t s_outputBuffer[OUTPUT_BUFFER_SIZE];
static std::atomic<size_t> s_outputHead{0};  // Write position
static std::atomic<size_t> s_outputTail{0};  // Read position
static std::mutex s_outputMutex;

// Input state
static char s_inputBuffer[INPUT_BUFFER_SIZE];
static std::atomic<bool> s_inputReady{false};
static std::atomic<bool> s_waitingForInput{false};
static std::atomic<bool> s_waitingForChar{false};
static std::atomic<uint32_t> s_inputChar{0};
static std::mutex s_inputMutex;
static std::condition_variable s_inputCv;

// Status line
static char s_statusRoom[64] = "";
static char s_statusScore[32] = "";
static std::mutex s_statusMutex;

// Interpreter state
static std::atomic<bool> s_gameExited{false};
static std::atomic<bool> s_running{false};
static std::thread s_fizmoThread;
static char s_storyPath[512] = "";
static z_file *s_storyFile = nullptr;

// Forward declarations
static void fizmo_thread_func();
static void push_output(const uint32_t *chars, size_t count);
static void push_output_char(uint32_t ch);

/*
 * Screen interface implementation
 */

static char* screen_get_interface_name() {
    fprintf(stderr, "[fizmo_bridge] get_interface_name called\n");
    fflush(stderr);
    return (char*)"ZorkForMCUs";
}

static bool screen_is_status_line_available() {
    fprintf(stderr, "[fizmo_bridge] is_status_line_available called\n");
    fflush(stderr);
    return true;
}
static bool screen_is_split_screen_available() { return false; }
static bool screen_is_variable_pitch_font_default() { return false; }
static bool screen_is_colour_available() { return false; }
static bool screen_is_picture_displaying_available() { return false; }
static bool screen_is_bold_face_available() { return false; }
static bool screen_is_italic_available() { return false; }
static bool screen_is_fixed_space_font_available() { return true; }
static bool screen_is_timed_keyboard_input_available() { return false; }
static bool screen_is_preloaded_input_available() { return false; }
static bool screen_is_character_graphics_font_availiable() { return false; }
static bool screen_is_picture_font_availiable() { return false; }

static uint16_t screen_get_screen_height_in_lines() { return 24; }
static uint16_t screen_get_screen_width_in_characters() { return 80; }
static uint16_t screen_get_screen_width_in_units() { return 80; }
static uint16_t screen_get_screen_height_in_units() { return 24; }
static uint8_t screen_get_font_width_in_units() { return 1; }
static uint8_t screen_get_font_height_in_units() { return 1; }

static z_colour screen_get_default_foreground_colour() {
    fprintf(stderr, "[fizmo_bridge] get_default_foreground_colour called\n");
    fflush(stderr);
    return Z_COLOUR_WHITE;
}
static z_colour screen_get_default_background_colour() {
    fprintf(stderr, "[fizmo_bridge] get_default_background_colour called\n");
    fflush(stderr);
    return Z_COLOUR_BLACK;
}

static uint8_t screen_get_total_width_in_pixels_of_text_sent_to_output_stream_3() {
    return 0;
}

static int screen_parse_config_parameter(char *key, char *value) {
    (void)key; (void)value;
    return 1; // not handled
}

static char* screen_get_config_value(char *key) {
    (void)key;
    return nullptr;
}

static char** screen_get_config_option_names() {
    return nullptr;
}

static void screen_link_interface_to_story(struct z_story *story) {
    (void)story;

    // Set default savegame filename to "zork1.sav"
    // This is used as the pre-filled default when user types SAVE/RESTORE
    const char *default_name = "zork1.sav";
    for (size_t i = 0; default_name[i] != '\0'; i++) {
        last_savegame_filename[i] = static_cast<z_ucs>(default_name[i]);
    }
    last_savegame_filename[strlen(default_name)] = 0;
}

static void screen_reset_interface() {
    // Clear output buffer
    s_outputHead.store(0);
    s_outputTail.store(0);
}

static int screen_close_interface(z_ucs *error_message) {
    if (error_message != nullptr) {
        // Push error message to output
        while (*error_message != 0) {
            push_output_char(*error_message);
            error_message++;
        }
        push_output_char('\n');
    }
    s_gameExited.store(true);
    return 0;
}

static void screen_set_buffer_mode(uint8_t new_buffer_mode) {
    (void)new_buffer_mode;
}

static void screen_z_ucs_output(z_ucs *output) {
    // Push output to ring buffer
    while (*output != 0) {
        push_output_char(*output);
        output++;
    }
}

static int16_t screen_read_line(zscii *dest, uint16_t maximum_length,
    uint16_t tenth_seconds, uint32_t verification_routine,
    uint8_t preloaded_input, int *tenth_seconds_elapsed,
    bool disable_command_history, bool return_on_escape)
{
    (void)tenth_seconds;
    (void)verification_routine;
    (void)preloaded_input;
    (void)tenth_seconds_elapsed;
    (void)disable_command_history;
    (void)return_on_escape;

    fprintf(stderr, "[fizmo_bridge] read_line called, max_length=%d\n", maximum_length);
    fflush(stderr);

    // Signal that we're waiting for input
    s_waitingForInput.store(true);
    s_inputReady.store(false);

    fprintf(stderr, "[fizmo_bridge] read_line waiting for input...\n");
    fflush(stderr);

    // Wait for input
    {
        std::unique_lock<std::mutex> lock(s_inputMutex);
        s_inputCv.wait(lock, []{ return s_inputReady.load() || !s_running.load(); });
    }

    fprintf(stderr, "[fizmo_bridge] read_line got input or shutdown\n");
    fflush(stderr);

    s_waitingForInput.store(false);

    if (!s_running.load()) {
        fprintf(stderr, "[fizmo_bridge] read_line: not running, returning 0\n");
        fflush(stderr);
        return 0;
    }

    // Copy input to destination
    size_t len = strlen(s_inputBuffer);
    if (len > maximum_length) {
        len = maximum_length;
    }
    for (size_t i = 0; i < len; i++) {
        dest[i] = static_cast<zscii>(s_inputBuffer[i]);
    }

    fprintf(stderr, "[fizmo_bridge] read_line returning %zu chars: '%s'\n", len, s_inputBuffer);
    fflush(stderr);

    return static_cast<int16_t>(len);
}

static int screen_read_char(uint16_t tenth_seconds, uint32_t verification_routine,
    int *tenth_seconds_elapsed)
{
    (void)tenth_seconds;
    (void)verification_routine;
    (void)tenth_seconds_elapsed;

    // Signal that we're waiting for a character
    s_waitingForChar.store(true);
    s_inputReady.store(false);

    // Wait for character input
    {
        std::unique_lock<std::mutex> lock(s_inputMutex);
        s_inputCv.wait(lock, []{ return s_inputReady.load() || !s_running.load(); });
    }

    s_waitingForChar.store(false);

    if (!s_running.load()) {
        return 0;
    }

    return static_cast<int>(s_inputChar.load());
}

static void screen_show_status(z_ucs *room_description, int status_line_mode,
    int16_t parameter1, int16_t parameter2)
{
    std::lock_guard<std::mutex> lock(s_statusMutex);

    // Convert room description to UTF-8
    if (room_description != nullptr) {
        char *p = s_statusRoom;
        char *end = s_statusRoom + sizeof(s_statusRoom) - 1;
        while (*room_description != 0 && p < end) {
            uint32_t ch = *room_description++;
            if (ch < 0x80) {
                *p++ = static_cast<char>(ch);
            }
            // Simplified - just handle ASCII for status
        }
        *p = '\0';
    }

    // Format score/time
    if (status_line_mode == SCORE_MODE_TIME) {
        snprintf(s_statusScore, sizeof(s_statusScore), "Time: %d:%02d",
                 parameter1, parameter2);
    } else {
        snprintf(s_statusScore, sizeof(s_statusScore), "Score: %d  Moves: %d",
                 parameter1, parameter2);
    }
}

static void screen_set_text_style(z_style text_style) { (void)text_style; }
static void screen_set_colour(z_colour foreground, z_colour background, int16_t window) {
    (void)foreground; (void)background; (void)window;
}
static void screen_set_font(z_font font_type) { (void)font_type; }
static void screen_split_window(int16_t nof_lines) { (void)nof_lines; }
static void screen_set_window(int16_t window_number) { (void)window_number; }
static void screen_erase_window(int16_t window_number) { (void)window_number; }
static void screen_set_cursor(int16_t line, int16_t column, int16_t window) {
    (void)line; (void)column; (void)window;
}
static uint16_t screen_get_cursor_row() { return 1; }
static uint16_t screen_get_cursor_column() { return 1; }
static void screen_erase_line_value(uint16_t start_position) { (void)start_position; }
static void screen_erase_line_pixels(uint16_t start_position) { (void)start_position; }
static void screen_output_interface_info() {}
static bool screen_input_must_be_repeated_by_story() { return false; }  // We echo in FizmoBackend::submitCommand()
static void screen_game_was_restored_and_history_modified() {}

// Storage for the save filename prompt
static char s_saveFilename[256] = "zork1.sav";
static const char* LASTFILE_NAME = ".zork_lastfile";

// Load last used filename from config file
static void load_last_filename() {
    FILE *f = fopen(LASTFILE_NAME, "r");
    if (f) {
        if (fgets(s_saveFilename, sizeof(s_saveFilename), f)) {
            // Remove trailing newline if present
            size_t len = strlen(s_saveFilename);
            if (len > 0 && s_saveFilename[len-1] == '\n') {
                s_saveFilename[len-1] = '\0';
            }
        }
        fclose(f);
        fprintf(stderr, "[fizmo_bridge] Loaded last filename: %s\n", s_saveFilename);
    }
}

// Save last used filename to config file
static void save_last_filename() {
    FILE *f = fopen(LASTFILE_NAME, "w");
    if (f) {
        fprintf(f, "%s\n", s_saveFilename);
        fclose(f);
    }
}
static int screen_prompt_for_filename(char *filename_suggestion, z_file **result_file,
    char *directory, int filetype_or_mode, int fileaccess)
{
    (void)filetype_or_mode;

    // Use suggestion if provided, otherwise use last saved filename
    const char *default_name = (filename_suggestion && filename_suggestion[0])
        ? filename_suggestion : s_saveFilename;

    // Build prompt message
    char prompt[512];
    snprintf(prompt, sizeof(prompt), "\nEnter filename (default: %s): ", default_name);

    // Output prompt
    z_ucs prompt_ucs[512];
    for (size_t i = 0; prompt[i] != '\0' && i < 511; i++) {
        prompt_ucs[i] = static_cast<z_ucs>(prompt[i]);
    }
    prompt_ucs[strlen(prompt)] = 0;
    screen_z_ucs_output(prompt_ucs);

    // Wait for user input (reuse the line input mechanism)
    s_waitingForInput.store(true);
    s_inputReady.store(false);

    {
        std::unique_lock<std::mutex> lock(s_inputMutex);
        s_inputCv.wait(lock, []{ return s_inputReady.load() || !s_running.load(); });
    }

    s_waitingForInput.store(false);

    if (!s_running.load()) {
        *result_file = nullptr;
        return -1;
    }

    // Get the entered filename (or use default if empty)
    char filename[256];
    if (s_inputBuffer[0] == '\0') {
        strncpy(filename, default_name, sizeof(filename) - 1);
    } else {
        strncpy(filename, s_inputBuffer, sizeof(filename) - 1);
    }
    filename[sizeof(filename) - 1] = '\0';

    // Remember this filename for next time and persist it
    strncpy(s_saveFilename, filename, sizeof(s_saveFilename) - 1);
    s_saveFilename[sizeof(s_saveFilename) - 1] = '\0';
    save_last_filename();

    // Build full path if directory provided
    char fullpath[512];
    if (directory && directory[0] != '\0') {
        snprintf(fullpath, sizeof(fullpath), "%s/%s", directory, filename);
    } else {
        strncpy(fullpath, filename, sizeof(fullpath) - 1);
        fullpath[sizeof(fullpath) - 1] = '\0';
    }

    // Open the file
    *result_file = fsi->openfile(fullpath, filetype_or_mode, fileaccess);

    if (*result_file == nullptr) {
        // Output error message
        char errmsg[256];
        snprintf(errmsg, sizeof(errmsg), "Could not open file: %s\n", fullpath);
        z_ucs err_ucs[256];
        for (size_t j = 0; errmsg[j] != '\0' && j < 255; j++) {
            err_ucs[j] = static_cast<z_ucs>(errmsg[j]);
        }
        err_ucs[strlen(errmsg)] = 0;
        screen_z_ucs_output(err_ucs);
        return -1;
    }

    return static_cast<int>(strlen(filename));
}

// Screen interface struct
static struct z_screen_interface s_screenInterface = {
    &screen_get_interface_name,
    &screen_is_status_line_available,
    &screen_is_split_screen_available,
    &screen_is_variable_pitch_font_default,
    &screen_is_colour_available,
    &screen_is_picture_displaying_available,
    &screen_is_bold_face_available,
    &screen_is_italic_available,
    &screen_is_fixed_space_font_available,
    &screen_is_timed_keyboard_input_available,
    &screen_is_preloaded_input_available,
    &screen_is_character_graphics_font_availiable,
    &screen_is_picture_font_availiable,
    &screen_get_screen_height_in_lines,
    &screen_get_screen_width_in_characters,
    &screen_get_screen_width_in_units,
    &screen_get_screen_height_in_units,
    &screen_get_font_width_in_units,
    &screen_get_font_height_in_units,
    &screen_get_default_foreground_colour,
    &screen_get_default_background_colour,
    &screen_get_total_width_in_pixels_of_text_sent_to_output_stream_3,
    &screen_parse_config_parameter,
    &screen_get_config_value,
    &screen_get_config_option_names,
    &screen_link_interface_to_story,
    &screen_reset_interface,
    &screen_close_interface,
    &screen_set_buffer_mode,
    &screen_z_ucs_output,
    &screen_read_line,
    &screen_read_char,
    &screen_show_status,
    &screen_set_text_style,
    &screen_set_colour,
    &screen_set_font,
    &screen_split_window,
    &screen_set_window,
    &screen_erase_window,
    &screen_set_cursor,
    &screen_get_cursor_row,
    &screen_get_cursor_column,
    &screen_erase_line_value,
    &screen_erase_line_pixels,
    &screen_output_interface_info,
    &screen_input_must_be_repeated_by_story,
    &screen_game_was_restored_and_history_modified,
    &screen_prompt_for_filename,
    nullptr, // do_autosave
    nullptr  // restore_autosave
};

/*
 * Helper functions
 */

static void push_output_char(uint32_t ch) {
    std::lock_guard<std::mutex> lock(s_outputMutex);

    size_t head = s_outputHead.load();
    size_t next_head = (head + 1) % OUTPUT_BUFFER_SIZE;

    // If buffer is full, drop oldest character
    if (next_head == s_outputTail.load()) {
        s_outputTail.store((s_outputTail.load() + 1) % OUTPUT_BUFFER_SIZE);
    }

    s_outputBuffer[head] = ch;
    s_outputHead.store(next_head);
}

static void push_output(const uint32_t *chars, size_t count) {
    for (size_t i = 0; i < count; i++) {
        push_output_char(chars[i]);
    }
}

/*
 * Fizmo thread function
 */

static void fizmo_thread_func() {
    fprintf(stderr, "[fizmo_bridge] Thread started\n");
    fflush(stderr);

    // Register file system interface (standard C I/O)
    fprintf(stderr, "[fizmo_bridge] Registering filesystem interface\n");
    fflush(stderr);
    fizmo_register_filesys_interface(&z_filesys_interface_c);

    // Register screen interface
    fprintf(stderr, "[fizmo_bridge] Registering screen interface\n");
    fflush(stderr);
    if (fizmo_register_screen_interface(&s_screenInterface) != 0) {
        fprintf(stderr, "[fizmo_bridge] Failed to register screen interface\n");
        fflush(stderr);
        s_gameExited.store(true);
        return;
    }

    // Open the story file
    fprintf(stderr, "[fizmo_bridge] Opening story file: %s\n", s_storyPath);
    fflush(stderr);
    s_storyFile = fsi->openfile(s_storyPath, FILETYPE_DATA, FILEACCESS_READ);
    if (s_storyFile == nullptr) {
        fprintf(stderr, "[fizmo_bridge] Failed to open story file: %s\n", s_storyPath);
        fflush(stderr);
        s_gameExited.store(true);
        return;
    }
    fprintf(stderr, "[fizmo_bridge] Story file opened successfully\n");
    fflush(stderr);

    // Start fizmo - this blocks until game ends
    fprintf(stderr, "[fizmo_bridge] Calling fizmo_start\n");
    fflush(stderr);
    fizmo_start(s_storyFile, nullptr, nullptr);
    fprintf(stderr, "[fizmo_bridge] fizmo_start returned\n");
    fflush(stderr);

    // Clean up
    if (s_storyFile != nullptr) {
        fsi->closefile(s_storyFile);
        s_storyFile = nullptr;
    }

    s_gameExited.store(true);
    fprintf(stderr, "[fizmo_bridge] Thread exiting\n");
    fflush(stderr);
}

/*
 * Public API implementation
 */

extern "C" {

int fizmo_bridge_init(const char *story_path) {
    if (story_path == nullptr) {
        return -1;
    }

    strncpy(s_storyPath, story_path, sizeof(s_storyPath) - 1);
    s_storyPath[sizeof(s_storyPath) - 1] = '\0';

    // Reset state
    s_outputHead.store(0);
    s_outputTail.store(0);
    s_inputReady.store(false);
    s_waitingForInput.store(false);
    s_waitingForChar.store(false);
    s_gameExited.store(false);
    s_running.store(false);

    // Load last used save filename from config
    load_last_filename();

    return 0;
}

int fizmo_start_interpreter(void) {
    if (s_running.load()) {
        return -1; // Already running
    }

    s_running.store(true);
    s_fizmoThread = std::thread(fizmo_thread_func);

    return 0;
}

void fizmo_bridge_shutdown(void) {
    s_running.store(false);

    // Wake up any waiting input
    {
        std::lock_guard<std::mutex> lock(s_inputMutex);
        s_inputReady.store(true);
    }
    s_inputCv.notify_all();

    if (s_fizmoThread.joinable()) {
        s_fizmoThread.join();
    }
}

size_t fizmo_output_available(void) {
    size_t head = s_outputHead.load();
    size_t tail = s_outputTail.load();

    if (head >= tail) {
        return head - tail;
    } else {
        return OUTPUT_BUFFER_SIZE - tail + head;
    }
}

size_t fizmo_output_read(uint32_t *buffer, size_t max_chars) {
    std::lock_guard<std::mutex> lock(s_outputMutex);

    size_t count = 0;
    while (count < max_chars && s_outputTail.load() != s_outputHead.load()) {
        buffer[count++] = s_outputBuffer[s_outputTail.load()];
        s_outputTail.store((s_outputTail.load() + 1) % OUTPUT_BUFFER_SIZE);
    }

    return count;
}

bool fizmo_waiting_for_input(void) {
    return s_waitingForInput.load();
}

bool fizmo_waiting_for_char(void) {
    return s_waitingForChar.load();
}

bool fizmo_has_exited(void) {
    return s_gameExited.load();
}

bool fizmo_get_status_line(char *room, size_t room_size,
                           char *score_or_time, size_t score_size)
{
    std::lock_guard<std::mutex> lock(s_statusMutex);

    strncpy(room, s_statusRoom, room_size - 1);
    room[room_size - 1] = '\0';

    strncpy(score_or_time, s_statusScore, score_size - 1);
    score_or_time[score_size - 1] = '\0';

    return true;
}

void fizmo_submit_line(const char *line) {
    {
        std::lock_guard<std::mutex> lock(s_inputMutex);
        strncpy(s_inputBuffer, line, INPUT_BUFFER_SIZE - 1);
        s_inputBuffer[INPUT_BUFFER_SIZE - 1] = '\0';
        s_inputReady.store(true);
    }
    s_inputCv.notify_all();
}

void fizmo_submit_char(uint32_t ch) {
    {
        std::lock_guard<std::mutex> lock(s_inputMutex);
        s_inputChar.store(ch);
        s_inputReady.store(true);
    }
    s_inputCv.notify_all();
}

} // extern "C"
