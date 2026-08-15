# Archive

Salvaged dead-end work kept for reference, not for applying as-is.

- `2026-06-16-sdl3-wip-stash.patch` — an old `git stash` (detached HEAD, base
  `3df908d4`) of exploratory SDL3 migration work: explicit `IMG_Init`, `std::cerr`
  debug logging in `rTexture.cpp`, and a `gluBuild2DMipmaps` → `glTexImage2D` +
  `glGenerateMipmap` replacement (with a hand-rolled mipmap fallback). The
  implementation was throwaway debug scaffolding and is superseded by the committed
  title-color fix. The durable facts from it are recorded in `docs/sharp-edges.md`
  (rendering section). Restore with `git apply` from repo root if ever needed.
