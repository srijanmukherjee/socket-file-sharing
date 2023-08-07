#ifndef __H_ARG_SETTINGS__
#define __H_ARG_SETTINGS__

#include <stdlib.h>
#include <string.h>

#include "../argp-standalone/include/argp-standalone/argp.h"
#include "constants.h"

const char* program_version = "share 1.0";
const char* program_bug_address = "<emailofsrijan@gmail.com>";

struct arguments {
    char *args[3];
    int verbose;
    int port;
};

// Order of fields: {NAME, KEY, ARG, FLAGS, DOC}
static struct argp_option options[] = {
    {"verbose", 'v', 0, OPTION_ARG_OPTIONAL, "Enable verbose output"},
    {"port", 'p', "PORTNO", OPTION_ARG_OPTIONAL, "Set socket port"},
    {0}    
};

static error_t parse_opt(int key, char* arg, struct argp_state* state) {
    struct arguments* arguments = state->input;

    switch(key) {
        case 'v':
            arguments->verbose = 1;
            break;
        case 'p': {
            if (arg[0] == '=') arg++;
            
            arguments->port = strtol(arg, NULL, 10);
            if (arguments->port <= 0) {
                fprintf(stderr, "[WARN] falling back to default port (%d)\n", DEFAULT_PORT);
                arguments->port = DEFAULT_PORT;
            }
            break;
        }
        case ARGP_KEY_ARG: {
            // first argument must be recv or send
            if (state->arg_num == 0 && ! (strcmp(arg, "recv") == 0 || strcmp(arg, "send") == 0))
                argp_usage(state);
            
            if (state->arg_num > 0) {
                int mode = arguments->args[0][0] == 'r';
                
                // mode=1 recv
                // mode=0 send <ip> <file>
                int num_args = mode == 1 ? 1 : 3;
                if (state->arg_num >= num_args)
                    argp_usage(state);    
            }

            arguments->args[state->arg_num] = arg;
            break;
        }
        case ARGP_KEY_END: {
            if (state->arg_num == 0)
                argp_usage(state);
            
            int num_args = arguments->args[0][0] == 'r' ? 1 : 3;
            if (state->arg_num < num_args)
                argp_usage(state);
            
            break;
        }
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static char arg_doc[] = "ARG1 [ARG2 ARG3]";

static char doc[] = "share -- A program to share file between linux computers.";

static struct argp argp = { options, parse_opt, arg_doc, doc };

#endif