#include "core/http/response.h"

void init_response(response_t *res) { init_server_response(res); }

void destroy_response(response_t *res) { destroy_server_response(res); }

// Server:

int set_response_status(response_t *res, int code) { return set_server_response_status(res, code); }

void set_response_header(response_t *res, const char *name, const char *value) { set_server_response_header(res, name, value); }

void set_response_body(response_t *res, char *body) { set_server_response_body(res, body); }

int serialize_response(response_t *res, char *buffer, int buffer_size) { return serialize_server_response(res, buffer, buffer_size); }

int send_response(int sock_fd, response_t *res) { return send_server_response(sock_fd, res); }

// Client:

const char *get_response_header(response_t *res, const char *name) { return get_client_response_header(res, name); }

int parse_response(const char *raw, response_t *res) { return parse_client_response(raw, res); }