#ifndef BB_WEB_TLS_H
#define BB_WEB_TLS_H

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Server-side TLS configuration.
 *
 * `certificate_file` may be a single certificate or a full chain (PEM,
 * leaf first). `private_key_file` must be the PEM-encoded private key
 * matching that certificate. Both files are loaded and validated during
 * bb_server_create_tls_on_runtime()/bb_server_create_tls() -- startup
 * fails immediately if either file is missing, malformed, or the two
 * don't match, rather than deferring the error to the first client
 * connection.
 */
typedef struct {
    const char *certificate_file;
    const char *private_key_file;
} bb_tls_config_t;


#ifdef __cplusplus
}
#endif

#endif //BB_WEB_TLS_H
