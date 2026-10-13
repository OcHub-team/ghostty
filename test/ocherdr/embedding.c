// Native integration test for OcHerdr's manual mirror and leased Metal APIs.
// Run with test/ocherdr/run.sh after building GhosttyKit.xcframework.
#include "ghostty.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static pthread_mutex_t frames_lock = PTHREAD_MUTEX_INITIALIZER;
static ghostty_metal_external_frame_s frames[2];
static size_t frame_count;
static atomic_size_t input_bytes;
static void wakeup(void *ud) { (void)ud; }
static bool action(ghostty_app_t app, ghostty_target_s target, ghostty_action_s value) {
    (void)app; (void)target; (void)value;
    return false;
}
static ghostty_clipboard_read_result_e read_clipboard(void *ud, ghostty_clipboard_e clipboard,
    void *state, const char *const *mimes, size_t count, bool list) {
    (void)ud; (void)clipboard; (void)state; (void)mimes; (void)count; (void)list;
    return GHOSTTY_CLIPBOARD_READ_UNAVAILABLE;
}
static void confirm_clipboard(void *ud, const ghostty_clipboard_confirm_s *contents,
    void *state, ghostty_clipboard_request_e request) {
    (void)ud; (void)contents; (void)state; (void)request;
}
static void write_clipboard(void *ud, ghostty_clipboard_e clipboard,
    const ghostty_clipboard_content_s *contents, size_t count, bool confirm) {
    (void)ud; (void)clipboard; (void)contents; (void)count; (void)confirm;
}
static void write_input(void *ud, const char *bytes, uintptr_t len) {
    (void)ud; (void)bytes;
    atomic_fetch_add(&input_bytes, len);
}
static ghostty_metal_external_frame_disposition_e present(void *ud,
    const ghostty_metal_external_frame_s *frame) {
    (void)ud;
    pthread_mutex_lock(&frames_lock);
    bool acquire = frame->host_context == 42 && frame_count < 2;
    if (acquire) frames[frame_count++] = *frame;
    pthread_mutex_unlock(&frames_lock);
    return acquire ? GHOSTTY_METAL_EXTERNAL_FRAME_ACQUIRE : GHOSTTY_METAL_EXTERNAL_FRAME_DROP;
}
static void pump(ghostty_app_t app, int cycles) {
    for (int i = 0; i < cycles; ++i) {
        ghostty_app_tick(app);
        usleep(10000);
    }
}
static size_t acquired_count(void) {
    pthread_mutex_lock(&frames_lock);
    size_t count = frame_count;
    pthread_mutex_unlock(&frames_lock);
    return count;
}
int main(int argc, char **argv) {
    assert(ghostty_init((uintptr_t)argc, argv) == 0);
    ghostty_config_t config = ghostty_config_new();
    assert(config);
    const char settings[] = "font-size = 12\nwindow-padding-x = 0\nwindow-padding-y = 0\ncursor-style-blink = false\nwindow-vsync = false\n";
    ghostty_config_load_string(config, settings, strlen(settings));
    ghostty_config_finalize(config);
    assert(ghostty_config_diagnostics_count(config) == 0);
    ghostty_runtime_config_s runtime = {
        .wakeup_cb = wakeup, .action_cb = action,
        .read_clipboard_cb = read_clipboard, .confirm_read_clipboard_cb = confirm_clipboard,
        .write_clipboard_cb = write_clipboard,
    };
    ghostty_app_t app = ghostty_app_new(&runtime, config);
    assert(app);
    ghostty_config_free(config);
    ghostty_surface_config_s options = ghostty_surface_config_new();
    options.platform_tag = GHOSTTY_PLATFORM_METAL_EXTERNAL_LEASED;
    options.platform.metal_external_leased.present = present;
    options.platform.metal_external_leased.userdata = NULL;
    options.io_mode = GHOSTTY_SURFACE_IO_MANUAL_MIRROR;
    options.io_write_cb = write_input;
    ghostty_surface_t surface = ghostty_surface_new_with_scrollback_limit(app, &options, 1024 * 1024);
    assert(surface);
    ghostty_surface_size_s size = {0};
    assert(ghostty_surface_set_grid_size(surface, 80, 24, &size));
    assert(size.columns == 80 && size.rows == 24);
    assert(!ghostty_surface_set_grid_size(surface, 0, 24, NULL));
    pump(app, 10);
    ghostty_surface_set_external_frame_context(surface, 42);
    const char output[] = "hello from Herdr\r\nUTF-8: \xce\xbb\r\n\x1b[6n\x1b[c";
    ghostty_surface_process_output(surface, output, strlen(output));
    ghostty_surface_refresh(surface);
    pump(app, 20);
    // Herdr owns parser replies. Displaying DSR/DA must not write to the PTY.
    assert(atomic_load(&input_bytes) == 0);
    assert(ghostty_surface_select_viewport_rows(surface, 0, 1));
    ghostty_text_s text = {0};
    assert(ghostty_surface_read_selection(surface, &text));
    assert(strstr(text.text, "hello from Herdr"));
    assert(strstr(text.text, "UTF-8: \xce\xbb"));
    ghostty_surface_free_text(surface, &text);
    ghostty_surface_text(surface, "input", 5);
    for (int i = 0; i < 100 && atomic_load(&input_bytes) == 0; ++i) pump(app, 1);
    assert(atomic_load(&input_bytes) == 5);
    for (int i = 0; i < 100 && acquired_count() < 2; ++i) {
        ghostty_surface_process_output(surface, ".", 1);
        ghostty_surface_refresh(surface);
        pump(app, 1);
    }
    assert(acquired_count() == 2);
    assert(frames[0].iosurface && frames[1].iosurface);
    assert(frames[0].iosurface != frames[1].iosurface);
    assert(frames[0].frame_token != frames[1].frame_token);
    assert(frames[1].width_px == size.width_px && frames[1].height_px == size.height_px);
    // Release in reverse order; a duplicate or unknown token cannot free a slot.
    assert(ghostty_surface_release_external_frame(surface, frames[1].frame_token));
    assert(!ghostty_surface_release_external_frame(surface, frames[1].frame_token));
    assert(ghostty_surface_release_external_frame(surface, frames[0].frame_token));
    assert(!ghostty_surface_release_external_frame(surface, 0));
    pump(app, 10);
    ghostty_surface_free(surface);
    ghostty_app_free(app);
    puts("OcHerdr embedding: grid, UTF-8, mirror replies, input, Metal frames and lease teardown passed");
    return 0;
}
