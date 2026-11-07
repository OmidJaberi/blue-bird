#ifndef SCHEMA_API_H
#define SCHEMA_API_H

#include <blue-bird/persist/model.h>
#include <blue-bird/persist/schema.h>
#include <blue-bird/web/http/handler.h>
#include <blue-bird/web/server.h>

/* Exposes `schema` as a REST resource named after its table (schema->name),
 * stored through `model`/`handle`. This is the only call an application makes
 * per entity; the handlers below serve every registered resource. */
int api_register(bb_schema_t *schema, const bb_model_api_t *model, bb_model_handle_t *handle);

/* Adds the six routes below to `server`. The fixed route goes first because
 * routes are matched in registration order. */
void api_add_routes(bb_server_t *server);

bb_error_t api_describe(bb_request_t *req, bb_response_t *res);  /* GET    /                */
bb_error_t api_list(bb_request_t *req, bb_response_t *res);      /* GET    /:resource       */
bb_error_t api_create(bb_request_t *req, bb_response_t *res);    /* POST   /:resource       */
bb_error_t api_get(bb_request_t *req, bb_response_t *res);       /* GET    /:resource/:id   */
bb_error_t api_replace(bb_request_t *req, bb_response_t *res);   /* PUT    /:resource/:id   */
bb_error_t api_delete(bb_request_t *req, bb_response_t *res);    /* DELETE /:resource/:id   */

#endif /* SCHEMA_API_H */
