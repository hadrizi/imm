#define SWISS_IMPL
#include "core.h"

#if defined(USE_CORE_LIB_SHARED)
    #include <libloaderapi.h>
    #include <stdio.h>

    #define MAX_FILEPATH_LENGTH 512

    typedef enum {
        OK = 0,
        FILE_NOT_FOUND,
        FAILED_TO_GET_MODULE_FILENAME,
        FAILED_TO_TRIM_PATH,
        FAILED_TO_APPEND_PATH,
        FAILED_TO_COPY_LIBCORE,
        FAILED_TO_LOAD_LIBCORE,
        FAILED_TO_UNLOAD_LIBCORE,
        FAILED_TO_LOAD_LIBCORE_FUNCTIONS,
        FAILED_TO_BUILD_COPY_PATH,
        FAILED_TO_BUILD_ABSOLUTE_PATH
    } BootstrapError;

    static BootstrapError error_code = OK;

    typedef struct {
        HINSTANCE   dll_handle;
        i64         src_write_time;  // last write time of libcore.dll, NOT COPY

        // core function pointers
        core_create_window_fn       create_window;
        core_prepare_fn             prepare;
        core_loop_fn                loop;
        core_window_should_close_fn window_should_close;
        core_close_window_fn        close_window;
        core_get_state_fn           get_state;
        core_set_state_fn           set_state;
    } LibCoreAPI;

    static bool get_absolute_path(lstring rel_path, string abs_path) {
        if (!rel_path || !abs_path) {
            error_code = FAILED_TO_BUILD_ABSOLUTE_PATH;
            return false;
        }

        lstring base = GetApplicationDirectory();
        if (!base || !base[0]) base = GetWorkingDirectory();
        if (!base || !base[0]) {
            error_code = FAILED_TO_GET_MODULE_FILENAME;
            return false;
        }

        lstring rp = rel_path;
        if ((rp[0] == '.' && (rp[1] == '/'))) rp += 2;

        size_t base_len = strlen(base);
        bool need_sep = base_len > 0 && base[base_len - 1] != '\\';

        i32 n = snprintf(abs_path, MAX_FILEPATH_LENGTH, "%s%s%s",
                         base, need_sep ? "\\" : "", rp);
        if (n <= 0 || (size_t)n >= MAX_FILEPATH_LENGTH) {
            error_code = FAILED_TO_BUILD_ABSOLUTE_PATH;
            return false;
        }

        return true;
    }

    static bool copy_to_unique(lstring src, string dst_out, size_t dst_cap) {
        static unsigned counter = 0;

        lstring app_dir = GetApplicationDirectory();
        if (!app_dir || !app_dir[0]) {
            error_code = FAILED_TO_GET_MODULE_FILENAME;
            return false;
        }

        char dst_path[MAX_FILEPATH_LENGTH] = {0};
        const size_t app_len = strlen(app_dir);
        const bool need_sep = app_len > 0 && (app_dir[app_len - 1] != '\\');

        i32 n = snprintf(dst_path, sizeof(dst_path),
                         "%s%slibcore_%u.dll",
                         app_dir, need_sep ? "\\" : "", ++counter);
        if (n <= 0 || (size_t)n >= sizeof(dst_path)) {
            error_code = FAILED_TO_BUILD_COPY_PATH;
            return false;
        }

        if (dst_out && dst_cap) {
            strncpy(dst_out, dst_path, dst_cap);
            if (dst_cap > 0) dst_out[dst_cap - 1] = '\0';
        }

        i32 data_size = 0;
        u8* data = LoadFileData(src, &data_size);
        if (!data) {
            error_code = FAILED_TO_COPY_LIBCORE;
            swiss_log_error("%s", src);
            return false;
        }

        bool is_save_ok = SaveFileData(dst_path, data, (int)data_size);
        UnloadFileData(data);

        if (!is_save_ok) {
            error_code = FAILED_TO_COPY_LIBCORE;
            swiss_log_error("%s", dst_path);
            return false;
        }

        return true;
    }

    static bool load_core(LibCoreAPI* api, lstring src_dll) {
        char abs_src_path[MAX_FILEPATH_LENGTH] = {0};
        if (!get_absolute_path(src_dll, abs_src_path)) return false;

        if (!FileExists(abs_src_path)) {
            error_code = FILE_NOT_FOUND;
            swiss_log_error("%s", abs_src_path);
            return false;
        }

        char copied[MAX_FILEPATH_LENGTH];
        if (!copy_to_unique(abs_src_path, copied, sizeof(copied))) return false;

        HINSTANCE dll_handle = LoadLibraryExA(copied, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!dll_handle) {
            error_code = FAILED_TO_LOAD_LIBCORE;
            return false;
        };

        api->prepare             = (void*)GetProcAddress(dll_handle, "core_prepare");
        api->create_window       = (void*)GetProcAddress(dll_handle, "core_create_window");
        api->loop                = (void*)GetProcAddress(dll_handle, "core_loop");
        api->window_should_close = (void*)GetProcAddress(dll_handle, "core_window_should_close");
        api->close_window        = (void*)GetProcAddress(dll_handle, "core_close_window");
        api->set_state           = (void*)GetProcAddress(dll_handle, "core_set_state");
        api->get_state           = (void*)GetProcAddress(dll_handle, "core_get_state");
        
        if (
            !api->prepare ||
            !api->create_window ||
            !api->loop ||
            !api->window_should_close ||
            !api->close_window ||
            !api->set_state ||
            !api->get_state
        ) {
            FreeLibrary(dll_handle);
            error_code = FAILED_TO_LOAD_LIBCORE_FUNCTIONS;
            return false;
        }

        api->dll_handle = dll_handle;
        api->src_write_time = GetFileModTime(abs_src_path);
        swiss_log_info("LIBCORE: %s: loaded successfully", copied);
        swiss_log_info("LIBCORE: %s: write time is %d", abs_src_path, api->src_write_time);
        
        return true;
    }

    static bool unload_core(LibCoreAPI* api) {
        bool status = true;

        if (!api) return false;

        if (api->dll_handle) {
            if (!FreeLibrary(api->dll_handle)) {
                error_code = FAILED_TO_UNLOAD_LIBCORE;
                status = false;
            }
            api->dll_handle = NULL;
        }

        api->src_write_time = 0;
        api->create_window = NULL;
        api->prepare = NULL;
        api->loop = NULL;
        api->window_should_close = NULL;
        api->close_window = NULL;
        api->get_state = NULL;
        api->set_state = NULL;

        return status;
    }

    static bool reload_if_changed(LibCoreAPI* api, lstring src_dll) {
        char abs_src_path[MAX_FILEPATH_LENGTH] = {0};
        if (!get_absolute_path(src_dll, abs_src_path)) return false;

        if (!FileExists(abs_src_path)) {
            error_code = FILE_NOT_FOUND;
            swiss_log_error("%s", abs_src_path);
            return false;
        }

        i64 now_time = GetFileModTime(abs_src_path);
        if (now_time == api->src_write_time) {  // unchanged
            error_code = OK;
            return false;
        }

        GameState prev_state = api->get_state();
        
        if (!unload_core(api)) return false;
        bool load_status = load_core(api, src_dll);

        if (!load_status) return false;

        api->set_state(prev_state);
        return true;
    }

    static void log_bootstrap_error(LibCoreAPI* core_api) {
        switch (error_code) {
        case FAILED_TO_GET_MODULE_FILENAME:
            swiss_log_error("BOOTSTRAP: failed to get module file name");
            break;
        case FAILED_TO_COPY_LIBCORE:
            swiss_log_error("BOOTSTRAP: failed to make a copy of libcore");
            break;
        case FAILED_TO_LOAD_LIBCORE:
            swiss_log_error("BOOTSTRAP: failed to load libcore copy");
            break;
        case FAILED_TO_UNLOAD_LIBCORE:
            swiss_log_error("BOOTSTRAP: failed to unload libcore copy");
            break;
        case FAILED_TO_LOAD_LIBCORE_FUNCTIONS:
            swiss_log_error(
                "BOOTSTRAP: failed to load libcore functions\n"
                "\tcore_prepare: %p\n"
                "\tcore_create_window: %p\n"
                "\tcore_loop: %p\n"
                "\tcore_window_should_close: %p\n"
                "\tcore_close_window: %p\n"
                "\tcore_get_state: %p\n"
                "\tcore_set_state: %p",
                (void*)core_api->prepare,
                (void*)core_api->create_window,
                (void*)core_api->loop,
                (void*)core_api->window_should_close,
                (void*)core_api->close_window,
                (void*)core_api->get_state,
                (void*)core_api->set_state
            );
            break;
        case FAILED_TO_TRIM_PATH:
            swiss_log_error("BOOTSTRAP: failed to trim path");
            break;
        case FAILED_TO_APPEND_PATH:
            swiss_log_error("BOOTSTRAP: failed to append path");
            break;
        case FILE_NOT_FOUND:
            swiss_log_error("BOOTSTRAP: file not found");
            break;
        case FAILED_TO_BUILD_COPY_PATH:
            swiss_log_error("BOOTSTRAP: failed to build copy path");
            break;
            case FAILED_TO_BUILD_ABSOLUTE_PATH:
            swiss_log_error("BOOTSTRAP: failed to build absolute path");
            break;
        default:
            break; // OK
        }
    }
#endif

int main(void) {
    #if defined(USE_CORE_LIB_SHARED)
        LibCoreAPI core_api = {0};
        lstring src_dll = "./libcore.dll";

        if (!load_core(&core_api, src_dll)) {
            log_bootstrap_error(&core_api);
            exit(1);
        }

        if (core_api.prepare() != 0) {
            swiss_log_error("GAME: failed to prepare");
            exit(1);
        }
        swiss_log_info("GAME: prepared successfully");
        core_api.create_window();

        while (!core_api.window_should_close()) {
            core_api.loop();

            if (reload_if_changed(&core_api, src_dll)) {
                swiss_log_info("LIBCORE: reloaded");
            } else if (error_code != OK) {
                log_bootstrap_error(&core_api);
                exit(1);
            }
        }
        
        core_api.close_window();
        if (!unload_core(&core_api)) {
            swiss_log_error("LIBCORE: failed to unload");
            exit(1);
        };
    #else
        if (core_prepare() != 0) {
            swiss_log_error("GAME: failed to prepare");
            exit(1);
        };
        swiss_log_info("GAME: prepared successfully");
        
        core_create_window();
        while (!core_window_should_close()) {
            core_loop();
        }
        core_close_window();
    #endif

    return 0;
}
