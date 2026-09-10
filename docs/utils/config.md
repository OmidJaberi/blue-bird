# Configuration

The config module loads settings from `.env` files or JSON files into a plain `bb_json_t` object, so application config lives in the same object model as everything else in Blue-Bird.

---

# Features

- `.env` file loading, with `#`-comments, `export ` prefixes, and automatic bool/int/real/text value parsing
- JSON file loading
- both loaders merge into an existing `bb_json_t` object, so multiple sources (e.g. a `.env` file followed by a JSON override) can be layered
- well-known keys for TLS certificate/private-key paths, consumed by the web module

---

# Loading Configuration

```c
#include <blue-bird/utils/bb_config.h>

bb_json_t *config = bb_json_create(BB_JSON_OBJECT);

bb_config_load_env(config, ".env");
bb_config_load_json(config, "config.json"); // overrides matching keys from .env

char *host = bb_json_get_value_text(bb_json_object_get_value(config, "host"));
int port = bb_json_get_value_integer(bb_json_object_get_value(config, "port"));
```

Both `bb_config_load_env()` and `bb_config_load_json()` require `config` to already be a JSON object, and they overwrite any existing keys with the same name. Values loaded from a `.env` file are inferred: `true`/`false` become booleans, `null` becomes JSON null, integers and floats are parsed as numbers, and everything else is kept as text (surrounding quotes are stripped).

---

# TLS Configuration

Two well-known keys carry the TLS certificate and private-key paths:

```c
#define BB_CONFIG_KEY_TLS_CERTIFICATE_FILE "tls_certificate_file"
#define BB_CONFIG_KEY_TLS_PRIVATE_KEY_FILE "tls_private_key_file"
```

These are ordinary string values, loaded the same way as any other config -- there's no separate TLS-specific loading path. Once the web module is available, `bb_tls_config_from_json()` turns them into a `bb_tls_config_t` for `bb_server_create_tls()`/`bb_server_create_tls_on_runtime()`. See [TLS, HTTPS & WSS](../web/tls.md#loading-tls-configuration-from-bb_config) for the full example.

---

# Related Documentation

- [JSON](json.md)
- [TLS, HTTPS & WSS](../web/tls.md)
