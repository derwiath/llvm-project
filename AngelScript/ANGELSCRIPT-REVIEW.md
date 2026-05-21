# Review: UnrealEngine Angelscript support in clang-format

## Overall assessment

The branch is small, focused, and follows the established pattern for adding a language (enum value, YAML enum
case, file-extension mapping, `is*()` predicate, token-merge hooks gated by language, a dedicated test file). The
mechanics are mostly sound and the token-merge approach mirrors the existing `tryMergeNSStringLiteral` /
`tryMergeJSPrivateIdentifier` precedents closely. The main risks for an upstream PR are **process/scope** (will the
maintainers accept an engine-specific dialect?) and a few **correctness/consistency** issues that CI or reviewers
will flag. Details below, ordered by how likely they are to block the PR.

## Blocking / process

**1. Get buy-in before the PR (RFC on Discourse).**
Adding a whole new language is a significant decision for clang-format. The recent precedent (Verilog, TableGen)
went through discussion on the LLVM Discourse (`#clang-format` / "Clang Frontend"). The active maintainers (Owen
Pan, Bjorn Schaepers/HazardyKnusperkeks, mydeveloperday) will almost certainly ask "why this and not upstream
AngelScript?" Post an RFC first and link it from the PR. The naming question (below) is the crux of that discussion.

**2. Naming / scope: `UnrealEngineAngelscript` vs `Angelscript`.**
The features here are a mix:

- `&in`, `&out`, `&inout` are **core AngelScript** reference-parameter syntax (angelcode.com), not Unreal-specific.
- `f"..."` / `n"..."` string prefixes and the `access:` / `access ... (modifier)` specifiers are
  **Unreal-Engine-Angelscript-specific** (angelscript.hazelight.se).

So the single name covers both core and fork-specific syntax. Expect maintainers to push on either (a) calling it
plain `Angelscript` and treating the UE bits as a superset, or (b) justifying why a third-party engine's dialect
belongs in upstream clang-format. Decide your position before the PR. Also note the cosmetic inconsistency: the
config token is `UnrealEngineAngelscript` (no space) but all prose says "UnrealEngine Angelscript" (with a space,
and "UnrealEngine" itself is normally two words "Unreal Engine").

**3. Docs are out of sync with the header - CI will catch this.**
`clang/docs/ClangFormatStyleOptions.rst` is **auto-generated** from the `///` comments in `Format.h` by
`clang/docs/tools/dump_format_style.py`. The script (`__clean_comment_line`) does nothing but strip the `/// `
prefix - it copies the comment text verbatim. The two sources disagree:

- `Format.h`: `Should be used for UnrealEngine Angelscript` / `(https://angelscript.hazelight.se/)`
  (no period, URL in parens)
- `.rst`: `Should be used for UnrealEngine Angelscript.` / `https://angelscript.hazelight.se/`
  (period, no parens)

Regenerating the docs will produce a diff, which the docs check / reviewers will reject. Fix: settle the wording in
`Format.h`, then run `clang/docs/tools/dump_format_style.py` to regenerate the `.rst` rather than hand-editing it.
(Compare with the `LK_Verilog` entry, where header and `.rst` match line-for-line.)

NOTE: Fixed

## Correctness / robustness

**4. `tryMergeAngelscriptStringLiteral` doesn't check adjacency.**
It merges any identifier `f`/`n` immediately followed by a `string_literal` token, regardless of intervening
whitespace, and `f`/`n` are extremely common identifiers. `f "x"` (with a space) would be silently merged into one
literal and the space dropped. Valid Angelscript requires the prefix to abut the quote; consider checking
`Prefix->TokenText.end() == String->TokenText.begin()` (or columns) to be safe. The NS/JS merges share this
looseness, so it may be accepted by precedent, but worth a comment at minimum.

NOTE: Fixed. `tryMergeAngelscriptStringLiteral` now bails out unless
`Prefix->TokenText.end() == String->TokenText.begin()`, so only the truly adjacent `f"..."` / `n"..."` form is
merged. Added the `StringPrefixRequiresAdjacency` test, which fails without the guard (the spaced input is frozen)
and passes with it (spacing normalized).

**5. `spaceRequiredBefore` for `access ... (modifier)` is broad.**
The rule fires for *any* `l_paren` on a line whose first token is the identifier `access`. An access declaration
that contains a call, e.g. `access Foo = Helper(x), Bar (readonly);`, would get a spurious space inserted into
`Helper (x)`. Probably rare in real access declarations, but it's a false-positive class worth narrowing (e.g. only
the paren that follows the modifier list / comma).

NOTE: Fixed. The rule now forces the space only when the `l_paren`'s matching `)` ends the statement
(`MatchingParen->Next` is `;`), which uniquely identifies the trailing modifier group; other parens fall back to
default call spacing. Added the `AccessSpecifierModifierParenSpacingWithCall` test, which fails without the guard
(the call is mangled to `Helper (x)`) and passes with it.

**6. `isCpp()` returning `true` for Angelscript is a wide hammer - call it out in the PR.**
It routes Angelscript through every `if (Style.isCpp())` path. Pragmatic given the C++-like syntax, and it correctly
satisfies the `assert(!IsVerilog || !IsCpp)`, but any future C++-specific logic will now also apply to Angelscript.
Document this as a deliberate design choice in the PR description.

**7. `&in`/`&out`/`&inout` merge fires on bare `tok::amp` + identifier.**
Fine in practice since `in`/`out`/`inout` are reserved in AngelScript, but a binary `x & out`-style expression (if
`out` were ever a value) would be misread. Low risk; a one-line comment noting the assumption would help reviewers.

NOTE: Will this ever be a problem? If they are reserved keywords then no-one will ever be able to write compilable
angelscript that has `out` as a variable?

NOTE: Confirmed not a problem; no code change needed. The AngelScript spec lists `in`, `out`, and `inout` as
**fully reserved** keywords ("they can't be used by any script defined identifiers"), separate from the
context-sensitive group (`get`, `set`, `from`, `super`, ...). So `out`/`in`/`inout` can never be a variable or
function name, and a binary `x & out` cannot occur in compilable code; the merge can only ever match the parameter
reference qualifier. (`&&` lexes as `tok::ampamp`, not `tok::amp`, so logical-and never reaches the merge.) Source:
https://www.angelcode.com/angelscript/sdk/docs/manual/doc_reserved_keywords.html . Optional: a one-line code comment
recording this assumption could pre-empt the same question from upstream reviewers.

## Tests

**8. Tabs in expected strings hurt readability and are unconventional.**
The suite forces `UseTab = UT_Always` / `TabWidth = 4` and embeds `\t` throughout the expected output. Most
clang-format test files use spaces precisely because `\t` in C string literals is hard to eyeball. Unless tab
behavior is specifically under test, prefer LLVM defaults (spaces) and reserve a dedicated tab test for tab-specific
cases.

NOTE: MAkes sense, please change the tests to configure a 2 space indentation and change all verifications to expect 
this.

NOTE: Fixed. `getDefaultStyle()` now sets `IndentWidth = 2` and drops `TabWidth`/`UseTab` (so the default
`UseTab = Never` applies); all `\t` in the expectations were converted to spaces. The wrapped-string continuation in
`StringPrefixF` is aligned with 7 spaces (under `return `). Verified by building `FormatTests` and running
`--gtest_filter='FormatTestUnrealEngineAngelscript*'`: all 9 tests pass.

**9. Coverage gaps.** Consider adding:

- A negative test proving the merges are language-gated (e.g. `f"..."` is **not** merged under `LK_Cpp`).
- The whitespace/adjacency edge case from #4.
- The `access ... (call(...))` false-positive from #5.
- At least one larger, realistic file (class with members, methods, mixed access modifiers) via the normal verify
  path, to catch interaction bugs.
NOTE: Makes sense, but would it be consistent with the otherlanguages? Do they have larger chunks of realistic codes
added to tests like that?

NOTE: Checked the existing per-language test files. The convention is overwhelmingly small targeted snippets, but a
moderate inline multi-construct block (~15-20 lines) is idiomatic when testing feature interactions, e.g.
`FormatTestCSharp.cpp:50-68` formats a whole class with several methods in one `verifyFormat` call. What is NOT done:
large production-like files or external fixture files loaded from disk (no test reads files; all inputs are inline
string literals). So a single realistic class is consistent; a big fixture would not be.

Done:
- Added `StringPrefixIsLanguageGated`: `f"hello"` stays merged under Angelscript but becomes `f "hello"` under C++
  (`getLLVMStyle()`), proving the merge is language-gated.
- Added `RealisticClass`: one ~17-line class mixing public/protected/private members and methods plus an `&in`
  reference parameter, in the C# precedent's style.
- The #4 and #5 edge cases are already covered by `StringPrefixRequiresAdjacency` and
  `AccessSpecifierModifierParenSpacingWithCall`.

Suite is now 13 tests, all passing.

**10. Style construction.**
Tests do `getLLVMStyle()` then set `Style.Language`. Prefer `getLLVMStyle(FormatStyle::LK_UnrealEngineAngelscript)`
so any language defaults from `getLLVMStyle`'s switch apply. No-op today (no Angelscript case there), but it's the
correct idiom and future-proofs the tests.

NOTE: Fixed. `getDefaultStyle()` now calls `getLLVMStyle(FormatStyle::LK_UnrealEngineAngelscript)` and drops the
separate `Style.Language =` assignment (`getLLVMStyle` sets `.Language` itself). All 13 tests still pass. The C++
gating assertion in `StringPrefixIsLanguageGated` intentionally keeps plain `getLLVMStyle()`.

## Smaller items

**11. `git-clang-format` doesn't know about `.as`.**
The `default_extensions` list in `clang/tools/clang-format/git-clang-format` (around line 91) lists every supported
extension but omits `.as`, so `git clang-format` won't pick up Angelscript files by default. Add it for consistency
with `getLanguageByFileName`.

NOTE: Fixed. Added `"as",  # Unreal Engine Angelscript` to the `default_extensions` list in `git-clang-format`.
Script still parses as valid Python.

**12. `.as` collides with ActionScript.**
Unconditionally mapping `.as` to Angelscript is fine (clang-format never supported ActionScript), but mention it in
the PR so it's a conscious decision.

**13. Overview/help string.**
`"...Protobuf/C#/UnrealEngine Angelscript code."` reads awkwardly - the embedded space makes the slash-list
ambiguous and the line is long. Consider `".../C#/Angelscript"` (or whatever final name lands) and check the
`--help` line length.

NOTE: Fixed. Changed the help/overview text (`ClangFormat.cpp`) and both occurrences in `ClangFormat.rst` to
`.../C#/UnrealEngine-Angelscript code.`, using the project's own hyphenated name from angelscript.hazelight.se
(no embedded space, so the slash-list stays unambiguous). The config token `UnrealEngineAngelscript` is unchanged.
The `.cpp` literal is split after `C#/` so both lines stay under 80 columns; verified that `clang-format --help`'s
OVERVIEW line matches the `.rst` verbatim.

**14. Release note** is fine; consider naming the concrete features ("string prefixes, reference qualifiers, inline
access specifiers") so users know what's covered.

**15. Commit history.**
Seven commits; upstream squash-merges PRs, so this is cosmetic, but ensure the final PR title/description references
the RFC and summarizes the feature set.

## Things that look correct

- Enum placement (alphabetical, before `LK_Verilog`), `getLanguageName`, and the YAML `enumCase` are all wired up
  consistently.
- Treating `public`/`protected`/`private` as inline modifiers (`nextToken()` in `parseStructuralElement`) plus
  `getIndentOffset` returning 0 is the right combination for Angelscript's `private int X;` member syntax, and
  matches the Java/JS/C# handling.
- The `&in`/`&out`/`&inout` merge as `TT_PointerOrReference` with `PAS_Left` correctly yields `FHitResult&in Hit`.
- The merge functions correctly mirror the existing `tryMergeNSStringLiteral` mechanics
  (kind/text/ColumnWidth/erase).

## Before pushing

Run `FormatTests --gtest_filter='FormatTestUnrealEngineAngelscript*'` and `clang/docs/tools/dump_format_style.py`
locally to confirm the tests pass and the generated docs match the header.
