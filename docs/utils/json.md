# JSON

The JSON module provides parsing, serialization, and JSON object utilities.

---

# Features

- JSON parser
- JSON serializer
- object model
- arrays
- comparison utilities
- JSON DSL macros

---

# Parsing JSON

Example:

```c
bb_json_t *json;
int rc = bb_json_parse(&json, "{ \"name\": \"Bob\" }");
```

---

# Creating JSON

Example:

```c
bb_json_t *obj = BB_JSON(
    OBJ(
        KEY("name", TEXT("Bob")),
        KEY("age", INT(24))
    )
);
```

---

# Serialization

```c
int size;
char *buffer;
int rc = bb_json_serialize(&json, &buffer, &size);
```

---

# JSON DSL

The DSL provides a lightweight syntax for constructing JSON structures programmatically.

Example:

```c
BB_JSON(
    OBJ(
        KEY("active", BOOL(true)),
        KEY("count", INT(10))
    )
)
```

---

# Design Goals

The JSON system is designed to be:
- lightweight
- composable
- framework-independent
- usable outside the web subsystem