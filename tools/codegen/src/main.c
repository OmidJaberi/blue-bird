#include "manifest.h"
#include "generators/persist_schema.h"

#include <stdio.h>
#include <string.h>

static void print_usage(const char *argv0)
{
    fprintf(stderr,
        "usage:\n"
        "  %s generate --out <dir> <manifest.json> [manifest2.json ...]\n"
        "  %s check    --out <dir> <manifest.json> [manifest2.json ...]\n"
        "\n"
        "  generate   writes generated files into <dir>.\n"
        "  check      fails (nonzero exit) if generated files in <dir> are\n"
        "             missing or don't match what the manifest(s) would\n"
        "             produce right now -- use this in CI.\n"
        "\n"
        "To (re)generate every schema in a directory, expand the glob\n"
        "yourself, e.g.:\n"
        "  %s generate --out schemas/generated schemas/*.schema.json\n",
        argv0, argv0, argv0);
}

static void register_generators(void)
{
    bb_codegen_register_generator("persist.schema", &bb_codegen_persist_schema_generator);
    /* Future kinds (web.route, security.policy, ...) register here. */
}

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        print_usage(argv[0]);
        return 2;
    }

    const char *command = argv[1];
    int check_only;

    if (strcmp(command, "generate") == 0)
        check_only = 0;
    else if (strcmp(command, "check") == 0)
        check_only = 1;
    else
    {
        fprintf(stderr, "bb-codegen: unknown command '%s'\n\n", command);
        print_usage(argv[0]);
        return 2;
    }

    if (strcmp(argv[2], "--out") != 0)
    {
        fprintf(stderr, "bb-codegen: expected --out <dir>\n\n");
        print_usage(argv[0]);
        return 2;
    }

    const char *out_dir = argv[3];

    if (argc < 5)
    {
        fprintf(stderr, "bb-codegen: no manifest files given\n\n");
        print_usage(argv[0]);
        return 2;
    }

    register_generators();

    int failures = 0;

    for (int i = 4; i < argc; i++)
    {
        if (bb_codegen_run(argv[i], out_dir, check_only) != 0)
            failures++;
    }

    return failures > 0 ? 1 : 0;
}
