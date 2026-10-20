#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <stdlib.h>
#include <wchar.h>

static wchar_t *environment_value(const wchar_t *name, int *present)
{
    DWORD capacity, length;
    wchar_t *value;

    SetLastError(ERROR_SUCCESS);
    capacity = GetEnvironmentVariableW(name, NULL, 0);
    if (!capacity) {
        if (GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
            *present = 0;
            return NULL;
        }
        /* An existing empty environment variable. */
        value = (wchar_t *)malloc(sizeof(wchar_t));
        if (!value)
            return NULL;
        value[0] = L'\0';
        *present = 1;
        return value;
    }
    value = (wchar_t *)malloc((size_t)capacity * sizeof(wchar_t));
    if (!value)
        return NULL;
    length = GetEnvironmentVariableW(name, value, capacity);
    if (length >= capacity) {
        free(value);
        return NULL;
    }
    *present = 1;
    return value;
}

static int write_utf8(HANDLE output, const wchar_t *text)
{
    int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                  text, -1, NULL, 0, NULL, NULL);
    char *bytes;
    DWORD offset = 0, written;
    if (size <= 0)
        return 0;
    bytes = (char *)malloc((size_t)size);
    if (!bytes)
        return 0;
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                            text, -1, bytes, size, NULL, NULL)) {
        free(bytes);
        return 0;
    }
    while (offset < (DWORD)(size - 1)) {
        if (!WriteFile(output, bytes + offset, (DWORD)(size - 1) - offset,
                       &written, NULL) || !written) {
            free(bytes);
            return 0;
        }
        offset += written;
    }
    free(bytes);
    return 1;
}

static int write_field(HANDLE output, const wchar_t *name,
                       const wchar_t *value)
{
    return write_utf8(output, name) && write_utf8(output, L"=") &&
           write_utf8(output, value) && write_utf8(output, L"\r\n");
}

int wmain(int argc, wchar_t **argv)
{
    int output_present = 0, test_present = 0, success = 1, index;
    wchar_t *output_path = environment_value(L"HD2_TEST_OUTPUT",
                                             &output_present);
    wchar_t *test_value = environment_value(L"HD2_LAUNCH_TEST",
                                            &test_present);
    wchar_t *cwd = NULL;
    wchar_t number[64];
    DWORD cwd_capacity, cwd_length;
    HANDLE output = INVALID_HANDLE_VALUE;
    int result = 38;

    if (!output_present || !output_path || !*output_path)
        goto cleanup;
    cwd_capacity = GetCurrentDirectoryW(0, NULL);
    if (!cwd_capacity)
        goto cleanup;
    cwd = (wchar_t *)malloc((size_t)cwd_capacity * sizeof(wchar_t));
    if (!cwd)
        goto cleanup;
    cwd_length = GetCurrentDirectoryW(cwd_capacity, cwd);
    if (!cwd_length || cwd_length >= cwd_capacity)
        goto cleanup;
    output = CreateFileW(output_path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                         CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (output == INVALID_HANDLE_VALUE)
        goto cleanup;
    success &= write_field(output, L"fixture", L"harmless_mock_game");
    swprintf(number, sizeof(number) / sizeof(number[0]), L"%d", argc);
    success &= write_field(output, L"argc", number);
    for (index = 0; index < argc; ++index) {
        swprintf(number, sizeof(number) / sizeof(number[0]),
                 L"argv[%d]", index);
        success &= write_field(output, number, argv[index]);
    }
    success &= write_field(output, L"cwd", cwd);
    success &= write_field(output, L"HD2_LAUNCH_TEST",
                           test_present && test_value ? test_value : L"<unset>");
    success &= write_field(output, L"sleep_ms", L"100");
    Sleep(100);
    success &= write_field(output, L"exit_code", L"37");
    if (success && FlushFileBuffers(output))
        result = 37;

cleanup:
    if (output != INVALID_HANDLE_VALUE)
        CloseHandle(output);
    free(cwd);
    free(test_value);
    free(output_path);
    return result;
}
