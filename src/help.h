#pragma once

static const char *HELP_CONTENT =
"Usage: sht [OPTIONS] [COMMAND [ARGS...]]\n"
"\n"
"A simple headless terminal that reads standard input and outputs the current screen whenever a newline is received."
"\n"
"\n"
"Options:\n"
"  -o, --output FILE     Write terminal screen output to FILE instead of stdout.\n"
"  -s, --size COLSxROWS  Set terminal size. Default: 80x24.\n"
"      --json            Output terminal screen contents as JSON.\n"
"  -h, --help            Show this help message and exit.\n"
"\n"
"Command:\n"
"  COMMAND [ARGS...]     Command to run inside the terminal.\n"
"                        Default: $SHELL\n"
"\n"
"Examples:\n"
"  sht\n"
"  sht --size 120x40\n"
"  sht --json\n"
"  sht -o screen.txt\n"
"  sht bash -i\n";
