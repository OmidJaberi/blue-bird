# Blue-Bird Template Module

## Overview

The Blue-Bird Template module is a lightweight, generic templating subsystem designed for:

- HTML rendering
- text generation
- configuration generation
- code generation
- scaffolding
- future CLI tooling

The module is intentionally minimal and composable.

It is **not**:

- an embedded scripting engine
- a frontend framework
- a VM
- a programming language runtime

The system prioritizes:

- simplicity
- readability
- predictability
- modularity
- explicit architecture

---

# Module Location

```txt
modules/template/
```

Public headers:

```txt
include/blue-bird/template/
```

---

# Design Goals

The template engine is designed as reusable infrastructure.

It is intentionally:

- generic
- backend-oriented
- embeddable
- lightweight

The module does **not** depend on:

- web
- persist
- repositories
- HTTP

It may depend on:

- utils
- error

This preserves clean dependency direction within Blue-Bird.

---

# Supported Features

Current supported features:

- variable interpolation
- nested lookup
- sections (loops)
- conditionals
- comments
- escaped delimiters

---

# Unsupported Features

The engine intentionally does NOT support:

- embedded scripting
- expression evaluation
- filters
- helpers
- inheritance
- macros
- runtime compilation
- VM execution

The goal is to keep the engine small and maintainable.

---

# Public Headers

```c
#include <blue-bird/template/template.h>
```

---

# Core Concepts

## Template Source

A raw template string.

Example:

```txt
Hello {{name}}
```

---

## Parser

Parses template text into an internal AST.

---

## AST

The template engine internally represents templates as a recursive syntax tree.

Supported node types:

- text
- variable
- section
- conditional
- comment

---

## Context

Rendering data is provided through `bb_json_t`.

This reuses the existing Blue-Bird JSON infrastructure.

---

## Renderer

The renderer walks the AST recursively and produces the final rendered output.

---

# Basic Usage

## Example

```c
#include <stdio.h>
#include <stdlib.h>

#include <blue-bird/template/template.h>
#include <blue-bird/utils/json.h>

int main(void)
{
    bb_template_t *tpl;

    bb_error_t err =
        bb_template_parse(
            "Hello {{name}}",
            &tpl
        );

    bb_json_t ctx = OBJ(
        KEY("name", TEXTV("Blue-Bird"))
    );

    char *result;
    err =
        bb_template_render(
            tpl,
            ctx,
            &result
        );

    printf("%s\n", result);

    free(result);

    bb_json_destroy(ctx);
    bb_template_destroy(tpl);

    return 0;
}
```

Output:

```txt
Hello Blue-Bird
```

---

# Variable Interpolation

Variables use:

```txt
{{name}}
```

Example:

```txt
Hello {{username}}
```

---

# Nested Lookup

Nested object traversal uses dot notation.

Example:

```txt
{{user.name}}
```

Context:

```json
{
  "user": {
    "name": "Alice"
  }
}
```

Output:

```txt
Alice
```

---

# Sections (Loops)

Sections iterate over JSON arrays.

Syntax:

```txt
{{#items}}
...
{{/items}}
```

Example:

```txt
{{#tasks}}
- {{name}}
{{/tasks}}
```

---

# Nested Sections

Sections may be nested recursively.

Example:

```txt
{{#users}}
User: {{name}}

{{#posts}}
* {{title}}
{{/posts}}

{{/users}}
```

---

# Conditionals

Conditionals render content only if the value is truthy.

Syntax:

```txt
{{?logged_in}}
Welcome
{{/logged_in}}
```

Truthy values:

- true
- non-zero numbers
- non-empty strings
- non-empty arrays
- objects

Falsy values:

- false
- null
- 0
- empty string
- empty array

---

# Comments

Comments are ignored during rendering.

Syntax:

```txt
{{! comment }}
```

Example:

```txt
Hello
{{! ignored }}
World
```

Output:

```txt
Hello
World
```

---

# Escaping Delimiters

Use `\{{` to render literal delimiters.

Example:

```txt
\{{example}}
```

Output:

```txt
{{example}}
```

---

# Missing Variables

Missing variables currently render as empty strings.

Example:

```txt
Hello {{missing}}
```

Output:

```txt
Hello
```

---

# HTML Rendering Example

```html
<h1>{{title}}</h1>

<ul>
{{#items}}
    <li>{{name}}</li>
{{/items}}
</ul>
```

---

# Configuration Generation Example

```txt
host={{host}}
port={{port}}
```

---

# Code Generation Example

```txt
typedef struct {
{{#fields}}
    {{type}} {{name}};
{{/fields}}
} {{model_name}};
```

This is intended for future:

- CLI generators
- scaffolding
- API generation
- repository generation

---

# Memory Management

The parser allocates the template object.

The renderer allocates the output string.

Ownership rules:

```c
free(result);
bb_template_destroy(tpl);
```

The caller owns:

- rendered output
- template destruction
- context destruction

---

# Error Handling

The module integrates with the Blue-Bird error system.

APIs return structured errors through:

```c
bb_error_t
```

Example:

```c
bb_template_t *tpl;

bb_error_t err =
    bb_template_parse(
        source,
        &tpl
    );

if (BB_FAILED(err))
{
    /* handle error */
}
```

---

# Architecture

## Recommended Module Layout

```txt
modules/template/
├── include/
│   └── blue-bird/template/
│       └── template.h
│
├── internal/
│   ├── ast.h
│   ├── parser.h
│   ├── render_context.h
│   ├── renderer.h
│   ├── string_builder.h
│   └── template_internal.h
│
├── src/
│   ├── ast.c
│   ├── parser.c
│   ├── renderer.c
│   ├── string_builder.c
│   └── template.c
│
└── CMakeLists.txt
```

---

# Internal Design

The parser produces a recursive AST.

The renderer recursively traverses:

- nodes
- sections
- nested contexts

Rendering uses:

```txt
context stack traversal
```

This allows child sections to access parent scope values naturally.

---

# Intended Future Extensions

Potential future additions:

- template file loading
- partials/includes
- template caching
- streaming output
- precompiled IR
- CLI scaffolding integration

These features should remain optional and must not compromise the lightweight design philosophy.

---

# Design Philosophy

The template engine is intended to feel like:

- Mustache
- lightweight Go templates
- small infrastructure libraries

Not like:

- Jinja2
- Twig
- Razor
- embedded scripting runtimes

The system is intentionally conservative and explicit.

---

# Summary

The Blue-Bird Template module provides:

- lightweight rendering
- reusable infrastructure
- generic text generation
- clean integration with JSON
- recursive rendering support
- composable architecture

while remaining:

- small
- predictable
- maintainable
- backend-oriented
