#ifndef COMMANDS_H
#define COMMANDS_H

#include <stddef.h>
#include "parser.h"

char* execute_command(RespArray*);
char* ping_command();
char* echo_command(RespData*);
char* get_command(RespData*);
char* set_command(RespData*, RespData*, RespData*, RespData*);
char* info_command(RespData*);
char* replconf_command(RespData*, RespData*);
char* psync_command(RespData*, RespData*);

#endif /* COMMANDS_H */
