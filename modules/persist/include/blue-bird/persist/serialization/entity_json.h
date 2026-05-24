#ifndef BB_ENTITY_JSON_H
#define BB_ENTITY_JSON_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/persist/schema.h"
#include "blue-bird/utils/json.h"

bb_json_t bb_entity_to_json(bb_schema_t *schema, void *entity);
int bb_json_to_entity(bb_schema_t *schema, bb_json_t json, void *out);


#ifdef __cplusplus
}
#endif

#endif
