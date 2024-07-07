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

    uint32_t file_size = GetFileSize(file, 0);
    uint32_t bytes_read = 0;

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

void search_data_for_key(const char *file_name, char *buffer, size_t buffer_size, const char *search_query) {
    size_t search_query_length = strlen(search_query);

    int line = 1;
    int column = 1;

    char *line_start = buffer;
    char *head = buffer;

    while (head - buffer < buffer_size) {
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

        bool found = (strncmp(head, search_query, search_query_length) == 0);
        if (found) {
            // We split the line up into before the key and after the key so we can
            // do nice color stuff

            char line_string_before[MAX_LINE_LENGTH] = {0};
            char line_string_after[MAX_LINE_LENGTH] = {0};

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
            printf("%s", search_query);
            SetConsoleTextAttribute(console, CONSOLE_MINOR);
            printf("%s\n", line_string_after);
        }

        head++;
    }

    SetConsoleTextAttribute(console, CONSOLE_WHITE);
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

        WIN32_FIND_DATAA find_data = {0};

        HANDLE handle = FindFirstFileA(pattern, &find_data);
        do {
            const char *file_name = find_data.cFileName;

            size_t buffer_size = 0;
            uint8_t *buffer = read_entire_file(file_name, &buffer_size);

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
