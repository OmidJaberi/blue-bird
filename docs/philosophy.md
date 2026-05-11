# Philosophy

Blue-Bird is designed around a small set of core principles.

---

# Explicit Over Magical

Blue-Bird prioritizes transparency and explicit behavior over hidden runtime magic.

The framework avoids:
- heavy reflection
- dependency injection containers
- implicit runtime behavior

---

# Modular Over Monolithic

The framework is organized into independent modules:
- web
- persist
- utils
- log
- error

Applications should only depend on the components they need.

---

# Infrastructure-First

Blue-Bird focuses on backend infrastructure:
- HTTP
- persistence
- logging
- serialization
- utilities

rather than high-level abstractions that hide implementation details.

---

# Low-Level Transparency

The framework embraces the strengths of C:
- explicit memory management
- predictable behavior
- lightweight abstractions
- low overhead

---

# Composable Design

Blue-Bird encourages composition over tightly coupled systems.

Modules are designed to work together while remaining independently usable.

---

# Long-Term Vision

The long-term goal of Blue-Bird is to provide a clean, modular backend framework ecosystem in C with:
- reusable infrastructure
- schema-driven persistence
- tooling and code generation
- lightweight developer workflows