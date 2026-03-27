#include "esp_console.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

static int cmd_hexdump(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: hexdump <addr> <len>\n");
        return 1;
    }

    uint8_t *addr = (uint8_t *)strtol(argv[1], NULL, 0);
    int len = atoi(argv[2]);

    if (len <= 0) {
        printf("Invalid length\n");
        return 1;
    }

    for (int i = 0; i < len; i += 16) {
        printf("%08x  ", (unsigned int)(addr + i));

        // hex part
        for (int j = 0; j < 16; j++) {
            if (i + j < len)
                printf("%02x ", addr[i + j]);
            else
                printf("   ");
        }

        printf(" ");

        // ascii part
        for (int j = 0; j < 16 && (i + j < len); j++) {
            uint8_t c = addr[i + j];
            printf("%c", isprint(c) ? c : '.');
        }

        printf("\n");
    }

    return 0;
}

void cli_register_mem(void)
{
    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "hexdump",
        .help = "Dump memory: hexdump <addr> <len>",
        .func = cmd_hexdump,
    });
}