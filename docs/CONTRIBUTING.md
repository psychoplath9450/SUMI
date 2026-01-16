# Contributing

Thanks for wanting to help out! Here's how to get started.

## Quick Start

1. Fork the repo
2. Clone your fork
3. Make a branch (`git checkout -b my-feature`)
4. Do your thing
5. Test it on actual hardware if you can
6. Open a PR

## Code Style

Nothing too crazy:

- 4 spaces, not tabs
- Opening braces on same line
- camelCase for functions and variables
- PascalCase for classes
- UPPER_CASE for constants
- Prefix private members with underscore (`_count`)

## Adding a Plugin

See [PLUGINS.md](PLUGINS.md) for the full guide, but basically:

1. Create `include/plugins/YourPlugin.h` with the class
2. Create `src/plugins/YourPlugin.cpp` that instantiates it
3. Hook it up in `HomeItems.h` and `AppLauncher.cpp`

## Commits

Try to write decent commit messages:

```
fix(reader): handle empty chapters without crashing

feat(flashcards): add CSV import support

docs: update README with new controls
```

The format is `type(scope): description` where type is one of:
- `feat` - new stuff
- `fix` - bug fixes
- `docs` - documentation
- `refactor` - code changes that don't add features or fix bugs
- `perf` - performance improvements

## Pull Requests

Before submitting:

- Make sure it compiles
- Test on hardware if possible
- Check memory usage (watch for `[MEM]` in serial output)
- Update docs if you changed anything user-facing

In the PR description, just explain what you changed and why. Screenshots help if it's a UI change.

## Testing

The ESP32-C3 only has 400KB of RAM, so memory matters. Keep an eye on the heap logs:

```
[MEM] Free: 142384  Min: 98432
```

If free heap drops below 50KB, something's probably wrong.

## E-Ink Gotchas

- Full refreshes are slow and flashy - use partial refresh when you can
- Don't update the display in a loop, only when something changes
- Test your refresh strategy to avoid ghosting buildup

## Questions?

Open an issue or start a discussion. We're happy to help.
