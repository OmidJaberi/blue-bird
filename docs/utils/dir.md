# Directory Listing

`blue-bird/utils/dir.h` lists the entries of a directory the same way on every
platform (POSIX `opendir`/`readdir`, Windows `FindFirstFile`).

---

# Usage

```c
#include <blue-bird/utils/dir.h>

char **names = NULL;
size_t count = 0;

if (bb_dir_list("migrations", &names, &count) != 0)
{
    /* directory missing or unreadable */
}

for (size_t i = 0; i < count; i++)
{
    printf("%s\n", names[i]);
}

bb_dir_list_free(names, count);
```

---

# Behaviour

- Returns entry **names**, not paths. Build the full path yourself.
- Names are sorted with `strcmp()`, so the order is identical on every platform
  and filesystem.
- `.` and `..` are never included.
- Files, subdirectories and anything else are all listed. Filter by name.
- A trailing `/` or `\` on the path is fine.
- An empty directory succeeds with `count == 0` and `names == NULL`.
- Returns `-1` if the directory can't be opened or read.

It is deliberately small: no recursion, no file types, no globbing. Add those
when something needs them.
