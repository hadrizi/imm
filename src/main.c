#define SWISS_IMPL
#include "swiss.h"

#if defined(USE_CORE_LIB_SHARED) 
    #include <windef.h>
    #include <winbase.h>
    #include <winuser.h>
    #include <shlwapi.h>
    #include <libloaderapi.h>
    #include <fileapi.h>
    #include <stdio.h>

    typedef enum {
        OK = 0,
        FAILED_TO_GET_MODULE_FILENAME,
        FAILED_TO_GET_FILE_ATTRIBUTES,
        FAILED_TO_TRIM_PATH,
        FAILED_TO_APPEND_PATH,
        FAILED_TO_COPY_LIBCORE,
        FAILED_TO_LOAD_LIBCORE,
        FAILED_TO_UNLOAD_LIBCORE,
        FAILED_TO_LOAD_LIBCORE_FUNCTIONS
    } BootstrapError;

    static BootstrapError error_code = OK;

    typedef struct {
        HINSTANCE dll_handle;
        FILETIME  src_write_time;  // last write time of libcore.dll, NOT COPY

        // core function pointers
        // TODO: add type aliases to core.h and use them
        i64  (*prepare)();
        void (*create_window)();
        void (*loop)();
        bool (*window_should_close)();
        void (*close_window)();
    } LibCoreAPI;

    static BOOL get_absolute_path(lstring rel_path, string absoulute_path) {
        GetModuleFileNameA(NULL, absoulute_path, MAX_PATH);
        if (!PathRemoveFileSpecA(absoulute_path)) {
            error_code = FAILED_TO_TRIM_PATH;
            return FALSE;
        }
        if (!PathAppendA(absoulute_path, rel_path)) {
            error_code = FAILED_TO_APPEND_PATH;
            return FALSE;
        }

        return TRUE;
    }

    static BOOL copy_to_unique(lstring src, string dst_out, size_t dst_cap) {
        static u32 counter = 0;
        char tmp[MAX_PATH];
        
        DWORD len = GetModuleFileNameA(NULL, tmp, MAX_PATH);
        if (len == 0 || len >= MAX_PATH) {
            error_code = FAILED_TO_GET_MODULE_FILENAME;
            return FALSE;
        };
        // strip filename
        for (int i = (int)len - 1; i >= 0 && tmp[i] != '\\' && tmp[i] != '/'; --i) tmp[i] = '\0';
        
        snprintf(tmp + strlen(tmp), MAX_PATH - strlen(tmp), "libcore_%u.dll", ++counter);
        if (dst_out && dst_cap) strncpy(dst_out, tmp, dst_cap);
        
        char absoulute_src[MAX_PATH];
        if (!get_absolute_path(src, absoulute_src)) return FALSE;
        
        if (!CopyFileA(absoulute_src, tmp, FALSE)) {
            error_code = FAILED_TO_COPY_LIBCORE;
            swiss_log_error("%s <- %s", absoulute_src, tmp);
            return FALSE;
        };
        
        swiss_log_info("copied libcore to %s", tmp);
        return TRUE;
    }

    static BOOL get_file_time(lstring path, FILETIME* t) {
        char absoulute_path[MAX_PATH];
        if (!get_absolute_path(path, absoulute_path)) return FALSE;

        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (!GetFileAttributesExA(absoulute_path, GetFileExInfoStandard, &fad)) {
            error_code = FAILED_TO_GET_FILE_ATTRIBUTES;
            swiss_log_error("%s", absoulute_path);
            return FALSE;
        };
        *t = fad.ftLastWriteTime;
        return TRUE;
    }

    static BOOL load_core(LibCoreAPI* api, lstring src_dll) {
        char copied[MAX_PATH];
        if (!copy_to_unique(src_dll, copied, sizeof(copied))) return FALSE;

        HINSTANCE dll_handle = LoadLibraryExA(copied, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!dll_handle) {
            error_code = FAILED_TO_LOAD_LIBCORE;
            return FALSE;
        };

        api->prepare             = (void*)GetProcAddress(dll_handle, "core_prepare");
        api->create_window       = (void*)GetProcAddress(dll_handle, "core_create_window");
        api->loop                = (void*)GetProcAddress(dll_handle, "core_loop");
        api->window_should_close = (void*)GetProcAddress(dll_handle, "core_window_should_close");
        api->close_window        = (void*)GetProcAddress(dll_handle, "core_close_window");
        
        if (
            !api->prepare ||
            !api->create_window ||
            !api->loop ||
            !api->window_should_close ||
            !api->close_window
        ) {
            FreeLibrary(dll_handle);
            error_code = FAILED_TO_LOAD_LIBCORE_FUNCTIONS;
            return FALSE;
        }

        api->dll_handle = dll_handle;
        if (!get_file_time(src_dll, &api->src_write_time)) return FALSE;
        return TRUE;
    }

    static BOOL unload_core(LibCoreAPI* api) {
        BOOL unload_status = TRUE;
        if (api->dll_handle) {
            unload_status = FreeLibrary(api->dll_handle);
            api->dll_handle = NULL;
        }
        ZeroMemory(api, sizeof(*api));

        if (!unload_status) {
            error_code = FAILED_TO_UNLOAD_LIBCORE;
            return FALSE;
        }
        return TRUE;
    }

    static BOOL reload_if_changed(LibCoreAPI* api, lstring src_dll) {
        FILETIME now_time;
        if (!get_file_time(src_dll, &now_time)) return FALSE;
        if (CompareFileTime(&now_time, &api->src_write_time) <= 0) {  // unchanged
            error_code = OK;
            return FALSE;
        }

        if (!unload_core(api)) return FALSE;
        return load_core(api, src_dll);
    }

    static void log_system_error() { 
        LPVOID lpMsgBuf;
        DWORD dw = GetLastError(); 

        if (!FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                dw,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR) &lpMsgBuf,
                0,
                NULL
            )
        ) {
            swiss_log_error("FormatMessage failed to translate system error");
            return;
        }

        swiss_log_error("SYSTEM: %s", lpMsgBuf);
        LocalFree(lpMsgBuf);
    }

    static void log_bootstrap_error(LibCoreAPI* core_api) {
        log_system_error();
        switch (error_code) {
        case FAILED_TO_GET_MODULE_FILENAME:
            swiss_log_error("failed to get module file name");
            break;
        case FAILED_TO_GET_FILE_ATTRIBUTES:
            swiss_log_error("failed to get file attributes");
            break;
        case FAILED_TO_COPY_LIBCORE:
            swiss_log_error("failed to make a copy of libcore");
            break;
        case FAILED_TO_LOAD_LIBCORE:
            swiss_log_error("failed to load libcore copy");
            break;
        case FAILED_TO_UNLOAD_LIBCORE:
            swiss_log_error("failed to unload libcore copy");
            break;
        case FAILED_TO_LOAD_LIBCORE_FUNCTIONS:
            swiss_log_error(
                "failed to load libcore functions\n"
                "\tcore_prepare: %p\n"
                "\tcore_create_window: %p\n"
                "\tcore_loop: %p\n"
                "\tcore_window_should_close: %p\n"
                "\tcore_close_window: %p\n",
                (void*)core_api->prepare,
                (void*)core_api->create_window,
                (void*)core_api->loop,
                (void*)core_api->window_should_close,
                (void*)core_api->close_window
            );
            break;
        case FAILED_TO_TRIM_PATH:
            swiss_log_error("failed to trim path");
            break;
        case FAILED_TO_APPEND_PATH:
            swiss_log_error("failed to append path");
            break;
        default:
            break; // OK
        }
    }
#else
    #include "core.h"
#endif

int main(void) {
    #if defined(USE_CORE_LIB_SHARED)
        LibCoreAPI core_api = {0};
        const char* src_dll = "./libcore.dll";

        if (!load_core(&core_api, src_dll)) {
            log_bootstrap_error(&core_api);
            exit(1);
        }

        if (core_api.prepare() != 0) {
            swiss_log_error("failed to prepare game");
            exit(1);
        }
        swiss_log_info("game prepared successfully");
        core_api.create_window();

        while (!core_api.window_should_close()) {
            core_api.loop();

            if (reload_if_changed(&core_api, src_dll)) {
                // TODO: currenly the state of the game is RESET when dll is reloaded
                //       this is not good since we need to KEEP the state between reloads
                //       should implement core_get_state and core_set_state in order to transfer it
                if (core_api.prepare() != 0) {
                    swiss_log_error("failed to prepare game");
                    exit(1);
                }
                swiss_log_info("game prepared successfully");
                swiss_log_info("libcore reloaded");
            } else if (error_code != OK) {
                log_bootstrap_error(&core_api);
                exit(1);
            }
        }
        
        core_api.close_window();
        if (!unload_core(&core_api)) {
            swiss_log_error("failed to prepare game");
            exit(1);
        };
    #else
        if (core_prepare() != 0) {
            swiss_log_error("failed to prepare game");
            exit(1);
        };
        swiss_log_info("game prepared successfully");
        
        core_create_window();
        while (!core_window_should_close()) {
            core_loop();
        }
        core_close_window();
    #endif

    return 0;
}
