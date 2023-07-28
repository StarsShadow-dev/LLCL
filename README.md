# The Parallel Programming Language

**Right now the compiler is very unstable.**

I am working on this programming language for personal use,
but the source code is public on github because someone might find this interesting.

Right now the compiler is being made for macOS.
But it may work on other platforms.

# Installation

### macOS (arm64)

* Choose the [latest release](https://github.com/StarsShadow-dev/Parallel-lang/releases).
* Click on "Parallel-lang-arm64-macos.zip".

If you get an error message like `“Parallel-lang” cannot be opened because it is from an unidentified developer`.

This is probably because safari gave the executable the `com.apple.quarantine` attribute.

You can list the attributes on the binary with:
```
xattr -l Parallel-lang
```

And you can remove the `com.apple.quarantine` attribute with: 
```
xattr -d com.apple.quarantine Parallel-lang
```