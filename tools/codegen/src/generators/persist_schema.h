#ifndef BB_CODEGEN_PERSIST_SCHEMA_H
#define BB_CODEGEN_PERSIST_SCHEMA_H

#include "manifest.h"

/* Generator for "kind": "persist.schema" manifests -- emits a
 * <Name>_schema.generated.h/.c pair: a plain C struct plus the
 * bb_field_t[]/bb_schema_t that describes it, replacing the
 * hand-written offsetof() boilerplate this used to require. */
extern const bb_codegen_generator_t bb_codegen_persist_schema_generator;

#endif //BB_CODEGEN_PERSIST_SCHEMA_H
