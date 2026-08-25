# UnrealEngine-Angelscript support in clang-format

This fork of `llvm-project` adds [UnrealEngine-Angelscript](https://angelscript.hazelight.se/) as a first-class
language in `clang-format`. Everything else in the tree is stock LLVM.

## Why

You can already format Angelscript by telling `clang-format` that it is C++, but a few constructs get mangled badly
enough that the result no longer compiles:

| Construct                              | Stock clang-format produces        |
| -------------------------------------- | ---------------------------------- |
| `f"Value = {Value}"`, `n"JohnDoe"`      | `f "Value = {Value}"` (broken)     |
| `void Fn(int&in Value)`                 | `int &in Value` split apart        |
| `protected int Member;`                 | line break after `protected`       |

The fork adds a real `UnrealEngineAngelscript` language so these are handled by the formatter itself instead of
being worked around in a style file.

## What it changes

- A new `FormatStyle::LK_UnrealEngineAngelscript` language, selectable via `Language: UnrealEngineAngelscript`.
- `.as` files are detected as Angelscript automatically, including in `git-clang-format`.
- Language-gated token merging for `f"` / `n"` string prefixes, `&in` / `&out` / `&inout` reference qualifiers, and
  inline `private` / `protected` access specifiers.

The implementation lives in `clang/lib/Format/` (`FormatTokenLexer`, `TokenAnnotator`, `UnwrappedLineParser`,
`Format.cpp`) and is covered by `clang/unittests/Format/FormatTestUnrealEngineAngelscript.cpp`.

## Build

Configure once, from the repository root:

```sh
cmake -G Ninja -S llvm -B build -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_PROJECTS=clang
```

Then build just the formatter (a few minutes on a warm tree, considerably longer for the first build):

```sh
ninja -C build clang-format
```

The binary ends up at `build/bin/clang-format`.

## Test

```sh
ninja -C build FormatTests
./build/tools/clang/unittests/Format/FormatTests --gtest_filter='FormatTestUnrealEngineAngelscript*'
```

Drop the `--gtest_filter` to run the full formatter test suite, which is what you want before sending a change.

## Try it on real code

`AngelScript/` holds a style file and sample scripts to eyeball the output against. `AngelScript/Script-Examples/`
is a copy of the upstream Unreal Engine Angelscript examples, so it exercises a lot of real syntax:

```sh
rg --files -g '*.as' AngelScript/Script-Examples \
  | xargs ./build/bin/clang-format -i --style=file:AngelScript/.clang-format
```

Then use `git diff` to inspect what the formatter did.

## Layout

| Path                                                          | Contents                                  |
| ------------------------------------------------------------- | ----------------------------------------- |
| `AngelScript/.clang-format`                                    | Style file used for the sample scripts    |
| `AngelScript/Script-Examples/`                                 | Larger real-world example scripts         |
