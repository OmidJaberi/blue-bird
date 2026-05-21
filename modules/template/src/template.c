#include "template_internal.h"

#include <stdlib.h>

void bb_template_destroy(bb_template_t *tpl)
{
    if (!tpl)
    {
        return;
    }
    bb_template_ast_destroy(&tpl->ast);
    free(tpl);
}
