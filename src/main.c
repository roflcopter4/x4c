#include "Common.h"
#include <getopt.h>

static noreturn void show_help(int status);
static          int handle_options(int argc, char **argv);

extern void decompile_main(const char *fname, const char *out_fname);
extern void recompile_main(const char *fname, const char *out_fname);

static const char          sopts[] = "o:dch";
static const struct option lopts[] = {
        { "output",    1, NULL, 'o' },
        { "decompile", 0, NULL, 'd' },
        { "compile",   0, NULL, 'c' },
        { "help",      0, NULL, 'h' },
        { NULL, 0, NULL, 0 },
};

static int   operation = 0;
static char *out_fname = NULL;

/*======================================================================================*/

int
main(int argc, char **argv)
{
        if (argc == 1)
                show_help(1);
        int ret = handle_options(argc, argv);
        argv   += ret;
        eprintf("Using fname %s\n", out_fname);
        
        switch (operation) {
        case 1:  decompile_main(argv[1], out_fname); break;
        case 2:  recompile_main(argv[1], out_fname); break;
        default: show_help(1);
        }

        free(out_fname);
        exit(EXIT_SUCCESS);
}

static int
handle_options(int argc, char **argv)
{
        int ch;

        while ((ch = getopt_long(argc, argv, sopts, lopts, NULL)) != (-1)) {
                switch (ch) {
                case 'o': out_fname = strdup(optarg); break;
                case 'd': operation = 1;              break;
                case 'c': operation = 2;              break;
                case 'h': show_help(0);
                default:  show_help(1);
                }
        }

        return (optind - 1);
}

static void
show_help(const int status)
{
        printf("Usage: %s -[%s] [filename]\n"
               "If no filename is given data is read from the standard input.\n"
               "All output is to the standard output.\n"
               "One of the options '-d' or '-c' is required.\n"
               "Options:\n"
               "  o, output:    Set output filename (default stdout)\n"
               "  d, decompile: Decompile xml source\n"
               "  c, compile:   Recompile previously decompiled source to xml\n"
               "  h, help:      Show this help and exit\n",
               program_invocation_short_name, sopts);

        exit(status);
}
