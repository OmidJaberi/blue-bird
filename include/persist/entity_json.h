#ifndef BB_ENTITY_JSON_H
#define BB_ENTITY_JSON_H

#include "persist/schema.h"
#include "utils/json.h"

json_node_t *bb_entity_to_json(BB_Schema *schema, void *entity);

#endif