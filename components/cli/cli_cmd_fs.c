// #include "esp_console.h"
// #include <dirent.h>
// #include <sys/stat.h>
// #include <stdio.h>
// #include <string.h>

// static int cmd_ls(int argc, char **argv)
// {
//     const char *path = (argc > 1) ? argv[1] : "/spiffs";

//     DIR *dir = opendir(path);
//     if (!dir) {
//         printf("Cannot open dir: %s\n", path);
//         return 1;
//     }

//     struct dirent *entry;
//     struct stat st;
//     char fullpath[256];

//     while ((entry = readdir(dir)) != NULL) {
//         int written = snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

//         if (written < 0 || written >= sizeof(fullpath)) {
//             printf("Path too long, skip: %s/%s\n", path, entry->d_name);
//             continue;
//         }

//         if (stat(fullpath, &st) == 0) {
//             printf("%c %8ld %s\n",
//                    (S_ISDIR(st.st_mode)) ? 'd' : '-',
//                    st.st_size,
//                    entry->d_name);
//         } else {
//             printf("?        %s\n", entry->d_name);
//         }
//     }

//     closedir(dir);
//     return 0;
// }

// static int cmd_cat(int argc, char **argv)
// {
//     if (argc < 2) {
//         printf("Usage: cat <file>\n");
//         return 1;
//     }

//     FILE *f = fopen(argv[1], "r");
//     if (!f) {
//         printf("Cannot open file: %s\n", argv[1]);
//         return 1;
//     }

//     char buf[128];
//     while (fgets(buf, sizeof(buf), f)) {
//         printf("%s", buf);
//     }

//     fclose(f);
//     return 0;
// }

// void cli_register_fs(void)
// {
//     esp_console_cmd_register(&(esp_console_cmd_t){
//         .command = "ls",
//         .help = "List files: ls [path]",
//         .func = cmd_ls,
//     });

//     esp_console_cmd_register(&(esp_console_cmd_t){
//         .command = "cat",
//         .help = "Show file content: cat <file>",
//         .func = cmd_cat,
//     });
// }


#include "esp_console.h"
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>

#define BASE_PATH "/fatfs"

#define MAX_PATH_LEN 128

static char cwd[128] = "/fatfs";

static int build_path(char *out, size_t size, const char *cwd, const char *name)
{
    int needed = snprintf(out, size, "%s/%s", cwd, name);

    if (needed < 0 || needed >= size) {
        printf("Path too long\n");
        return -1;
    }

    return 0;
}

static int cmd_ls(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : cwd;

    DIR *dir = opendir(path);
    if (!dir) {
        printf("Cannot open %s\n", path);
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        printf("%s\n", entry->d_name);
    }

    closedir(dir);
    return 0;
}

static int cmd_pwd(int argc, char **argv)
{
    printf("%s\n", cwd);
    return 0;
}

static int cmd_cd(int argc, char **argv)
{
    if (argc < 2) return 1;

    char newpath[MAX_PATH_LEN];

    if (argv[1][0] == '/') {
        strncpy(newpath, argv[1], sizeof(newpath));
    } else {
        if (build_path(newpath, sizeof(newpath), cwd, argv[1]) != 0) {
            return 1;
        }
    }

    DIR *dir = opendir(newpath);
    if (!dir) {
        printf("No such dir\n");
        return 1;
    }

    closedir(dir);

    strncpy(cwd, newpath, sizeof(cwd) - 1);
    cwd[sizeof(cwd) - 1] = '\0';

    return 0;
}

static int cmd_touch(int argc, char **argv)
{
    if (argc < 2) return 1;

    char path[128];
    if (build_path(path, sizeof(path), cwd, argv[1]) != 0) {
        return 1;
    }

    FILE *f = fopen(path, "w");
    if (!f) {
        printf("Create failed\n");
        return 1;
    }

    fclose(f);
    return 0;
}

static int cmd_cat(int argc, char **argv)
{
    if (argc < 2) return 1;

    char path[MAX_PATH_LEN];

    if (build_path(path, sizeof(path), cwd, argv[1]) != 0) {
        return 1;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("Open failed\n");
        return 1;
    }

    char buf[128];
    while (fgets(buf, sizeof(buf), f)) {
        printf("%s", buf);
    }

    fclose(f);
    return 0;
}
static int cmd_mkdir(int argc, char **argv)
{
    if (argc < 2) return 1;

    char path[128];
    if (build_path(path, sizeof(path), cwd, argv[1]) != 0) {
        return 1;
    }

    mkdir(path, 0777);
    return 0;
}

static int cmd_rm(int argc, char **argv)
{
    if (argc < 2) return 1;

    char path[128];
    if (build_path(path, sizeof(path), cwd, argv[1]) != 0) {
        return 1;
    }

    unlink(path);
    return 0;
}

void cli_register_fs(void)
{
    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "ls",
        .func = cmd_ls,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "pwd",
        .func = cmd_pwd,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "cd",
        .func = cmd_cd,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "touch",
        .func = cmd_touch,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "cat",
        .func = cmd_cat,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "mkdir",
        .func = cmd_mkdir,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "rm",
        .func = cmd_rm,
    });
}