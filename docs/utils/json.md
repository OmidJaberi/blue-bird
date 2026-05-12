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
json_node_t json;
int rc = parse_json_str(&json, "{ \"name\": \"Bob\" }");
```

---

# Creating JSON

Example:

```c
json_node_t *obj = JSON(
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
int rc = serialize_json(&json, &buffer, &size);
```

---

# JSON DSL

The DSL provides a lightweight syntax for constructing JSON structures programmatically.

Example:

```c
JSON(
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