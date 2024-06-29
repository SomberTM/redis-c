#ifndef COMMANDS_H
#define COMMANDS_H

#include <stddef.h>
#include "parser.h"

const char* execute_command(RespArray*);
const char* ping_command();
const char* echo_command(RespData*);
const char* get_command(RespData*);
const char* set_command(RespData*, RespData*, RespData*, RespData*);
const char* info_command(RespData*);
const char* replconf_command(RespData*, RespData*);
const char* psync_command(RespData*, RespData*);

#endif /* COMMANDS_H */
