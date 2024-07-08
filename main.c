#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <windows.h>

#define MAX_LINE_LENGTH 8192

#define CONSOLE_MINOR  6
#define CONSOLE_WHITE 15

HANDLE console;

uint8_t *read_entire_file(const char *file_name, size_t *buffer_size_out) {
    uint8_t *result = 0;

    HANDLE file = CreateFileA(file_name,
                              GENERIC_READ,
                              FILE_SHARE_READ,
                              NULL,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL);

    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD file_size = GetFileSize(file, 0);
    DWORD bytes_read = 0;

    result = calloc(file_size+1, 1);

    int ok = ReadFile(file, result, file_size, &bytes_read, NULL);
    if (!ok) {
        return 0;
    }

    assert(file_size == bytes_read);

    if (buffer_size_out) {
        *buffer_size_out = file_size;
    }

    return result;
}

bool string_contains_string_case_insensitive(const char *a, const char *b) {
    size_t a_length = strlen(a);
    size_t b_length = strlen(b);

    if (a_length < b_length) return false;

    if (b_length == 0) return false;

    for (size_t i = 0; i < b_length; i++) {
        if (tolower(a[i]) != tolower(b[i]))
            return false;
    }

    return true;
}

void search_data_for_key(const char *file_name, char *buffer, size_t buffer_size, const char *search_query) {
    size_t search_query_length = strlen(search_query);

    int line = 1;
    int column = 1;

    char *line_start = buffer;
    char *head = buffer;

    while ((size_t)(head - buffer) < buffer_size) {
        // Handle new lines
        if (*head == '\r') { // CRLF or just CR
            line++;
            head++;
            column = 1;

            if (*head == '\n') {
                head++;
            }

            line_start = head;
        } else if (*head == '\n') { // LF
            head++;
            line++;
            column = 1;

            line_start = head;
        } else {
            column++;
        }

        bool found = string_contains_string_case_insensitive(head, search_query);
        if (found) {
            // We split the line up into before the key and after the key so we can
            // do nice color stuff

            char line_string_before[MAX_LINE_LENGTH] = {0};
            char line_string_after[MAX_LINE_LENGTH] = {0};
            char found_search[MAX_LINE_LENGTH] = {0};

            size_t before_length = head - line_start;

            {
                char *s = line_start;
                int idx = 0;
                while (idx < before_length && *s && *s != '\r' && *s != '\n') {
                    line_string_before[idx++] = *s;
                    ++s;
                }
            }
            {
                char *s = line_start + before_length;
                for (int i = 0; i < search_query_length; i++) {
                    found_search[i] = s[i];
                }
            }
            {
                char *s = line_start + before_length + search_query_length;
                int idx = 0;
                while (*s && *s != '\r' && *s != '\n') {
                    line_string_after[idx++] = *s;
                    ++s;
                }
            }

            SetConsoleTextAttribute(console, CONSOLE_WHITE);
            printf("%s(", file_name);
            SetConsoleTextAttribute(console, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            printf("%d", line);
            SetConsoleTextAttribute(console, CONSOLE_WHITE);
            printf(",");
            SetConsoleTextAttribute(console, FOREGROUND_GREEN);
            printf("%d", column);
            SetConsoleTextAttribute(console, CONSOLE_WHITE);
            printf("): ");

            SetConsoleTextAttribute(console, CONSOLE_MINOR);
            printf("%s", line_string_before);
            SetConsoleTextAttribute(console, FOREGROUND_RED);
            printf("%s", found_search);
            SetConsoleTextAttribute(console, CONSOLE_MINOR);
            printf("%s\n", line_string_after);
        }

        head++;
    }

    SetConsoleTextAttribute(console, CONSOLE_WHITE);
}

void get_directory_and_file_from_filepath(const char *filepath, char *directory, char *file) {
    int slash_index = 0;
    for (slash_index = (int)strlen(filepath)-1;
         slash_index >= 0 && filepath[slash_index] != '/' && filepath[slash_index] != '\\';
         slash_index--);

    // No slash found?
    if (slash_index == -1) {
        directory[0] = '.';
        strcpy(file, filepath);
    } else {
        strncpy(directory, filepath, slash_index+1);
        strcpy(file, filepath+slash_index+1);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        puts("Usage: search \"search query\" <files>");
        return 1;
    }

    console = GetStdHandle(STD_OUTPUT_HANDLE);

    char *search_query = argv[1];

    char **files = argv+2;
    int file_count = argc-2;

    for (int i = 0; i < file_count; i++) {
        char *pattern = files[i];

        // First we find the directory and CD into it.
        char directory[MAX_PATH] = {0};
        char file[MAX_PATH] = {0};

        get_directory_and_file_from_filepath(pattern, directory, file);

        int ok = SetCurrentDirectory(directory);
        if (!ok) {
            fprintf(stderr, "Error trying to open directory %s\n", directory);
            continue;
        }

        WIN32_FIND_DATAA find_data = {0};

        HANDLE handle = FindFirstFileA(file, &find_data);
        do {
            const char *file_name = find_data.cFileName;

            size_t buffer_size = 0;
            char *buffer = (char*) read_entire_file(file_name, &buffer_size);

            if (!buffer) {
                fprintf(stderr, "Error trying to open the file %s\n", file_name);
            } else {
                search_data_for_key(file_name, buffer, buffer_size,
                                    search_query);
            }

            free(buffer);
        } while (FindNextFileA(handle, &find_data) != 0);
    }

    return 0;
}
