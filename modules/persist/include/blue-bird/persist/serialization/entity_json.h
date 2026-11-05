#ifndef BB_ENTITY_JSON_H
#define BB_ENTITY_JSON_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/persist/schema.h"
#include "blue-bird/utils/json.h"

bb_json_t *bb_entity_to_json(bb_schema_t *schema, void *entity);
/* Fills `out` (struct_size bytes, zeroed first) from a JSON object, field by
 * field according to `schema`. Keys missing from the JSON leave the field zero;
 * unknown keys are ignored.
 *
 * Returns 0, or -1 if `json` is not an object or a value has the wrong type.
 *
 * String and UUID values are always NUL-terminated. A value longer than the
 * field (more than size - 1 bytes) is truncated, never at the middle of a
 * UTF-8 character. Callers that must reject over-long input instead of
 * truncating it need to check the length before calling. */
int bb_json_to_entity(bb_schema_t *schema, bb_json_t *json, void *out);


#ifdef __cplusplus
}
#endif

#endif
