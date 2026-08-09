# picoruby-ti

a universal picoruby type inferrer

## Database generation

Place the RBS files used to generate the built-in type database in `sig/`.
The files are read in alphabetical order.

Run:

```sh
make gendb
```

This generates:

```text
src/generated/picoruby_ti_builtin_database.c
src/generated/picoruby_ti_builtin_database.h
```

`sig/*.rbs` and `src/generated/` are not tracked by Git.
