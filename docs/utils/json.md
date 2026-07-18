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
bb_json_t *json = bb_json_parse("{ \"name\": \"Bob\" }");
```

---

# Creating JSON

Example:

```c
bb_json_t *obj = BB_JSON(
    OBJ(
        KEY("name", TEXTV("Bob")),
        KEY("age", INTV(24))
    )
);
```

---

# Serialization

```c
int size;
char *buffer;
bb_error_t err = bb_json_serialize(&json, &buffer, &size);
```

---

# JSON DSL

The DSL provides a lightweight syntax for constructing JSON structures programmatically.

Example:

```c
BB_JSON(
    OBJ(
        KEY("active", BOOLV(true)),
        KEY("count", INTV(10))
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