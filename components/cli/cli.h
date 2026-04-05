#pragma once

void cli_init(void);
void cli_start(void);
void cli_init_all(void);

// register group
void cli_register_system(void);
void cli_register_fs(void);
void cli_register_mem(void);
void cli_register_i2c(void);
void cli_register_gpio(void);
void cli_register_log(void);
void cli_register_mpu(void);
void cli_register_fall(void);