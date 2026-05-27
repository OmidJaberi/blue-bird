#include "blue-bird/web/http/response.h"

void bb_response_init(bb_response_t *res) { bb_server_response_init(res); }

void bb_response_destroy(bb_response_t *res) { bb_server_response_destroy(res); }

// Server:

int bb_response_set_status(bb_response_t *res, int code) { return bb_server_response_set_status(res, code); }

void bb_response_set_header(bb_response_t *res, const char *name, const char *value) { bb_server_response_set_header(res, name, value); }

void bb_response_set_body(bb_response_t *res, char *body) { bb_server_response_set_body(res, body); }

int bb_response_serialize(bb_response_t *res, char **buffer, size_t *size) { return bb_server_response_serialize(res, buffer, size); }

// Client:

const char *bb_response_get_header(bb_response_t *res, const char *name) { return bb_client_response_get_header(res, name); }

int bb_response_parse(const char *raw, bb_response_t *res) { return bb_client_response_parse(raw, res); }
