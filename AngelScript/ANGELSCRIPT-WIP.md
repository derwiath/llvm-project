# Brief
I want to extend `clang-format` with support for Angelscript, specifically
the flavor of angelscript that can be used as a plugin in UnrealEngine.

You can already do that to some extent, by fooling `clang-format`
that angelscript is C++, see the `clang-format-angelscript` config file.

However, there is a couple of cases where it does not function well that I would like to improve. Let's concentrate on one first.

## String prefixes (`f` and `n`)

```angelscript
int Value = 1234;
const FString FormattedString = f"Value = {Value}";
const FName MyName = n"JohnDoe";
```

This is formatted by inserting spaces between the string prefix and the double-qute:
```angelscript
int Value = 1234;
const FString FormattedString = f "Value = {Value}";
const FName MyName = n "JohnDoe";
```
This actually breaks the code, and it does nolonger compile.

Can you help me figure out how to fix this? I don't know how to work in this
codebase so I cant give you much more to start with.
Perhaps a good place to start would be to figure out how to build the
executable so that we can test out our changes.

## Accessor speecifiers per member variable or method

In Angelscript you can specify protected or private per member variable
or method (default is public). This is similar to both C# and Java I belive.

clang-format inserts unwanted line breaks after the accessor so that this:
```angelscript
class UAccessortTest
{
	int PublicMember;
	protected int ProtectedMember = 0;
	private int PrivateMember = 0;

	int GetPublicMember() const
	{
		return PublicMember;
	}

	protected int GetProtectedMember() const
	{
		return ProtectedMember;
	}

	private int GetPrivateMember() const
	{
		return PrivateMember;
	}
}
```

becomes:
```angelscript
class UAccessortTest
{
	int PublicMember;
	protected
	int ProtectedMember = 0;
	private
	int PrivateMember = 0;

	int GetPublicMember() const
	{
		return PublicMember;
	}

	protected
	int GetProtectedMember() const
	{
		return ProtectedMember;
	}

	private
	int GetPrivateMember() const
	{
		return PrivateMember;
	}
}
```

## Files of interest
@../clang/tools/clang-format/ClangFormat.cpp
@./.clang-format
@./FormatTest.as
@./AccessorTest.as
@~/github.com/Hazelight/UnrealEngine-Angelscript/

# Analysis of the problem

## Root cause: String prefixes

The space insertion is caused by a rule in the `spaceRequiredBefore()` function in
`clang/lib/Format/TokenAnnotator.cpp` (line ~5149):

```cpp
// `Left` is a keyword (including C++ alternative operator) or identifier.
if (Left.Tok.getIdentifierInfo() && Right.Tok.isLiteral())
  return true;
```

This rule says: if the left token is any identifier and the right token is any literal
(including string literals), insert a space. So `f"hello"` becomes `f "hello"` because
`f` is tokenized as an identifier and `"hello"` as a string literal.

## Root cause: Accessor specifiers

In C++, access specifiers (`public:`, `private:`, `protected:`) are section labels followed by a colon.
The parser in `UnwrappedLineParser.cpp` (line ~1481) handles them differently per language:

```cpp
if (FormatTok->isAccessSpecifierKeyword()) {
  if (Style.isJava() || Style.isJavaScript() || Style.isCSharp())
    nextToken();            // Treat as inline modifier (Java/C#)
  else
    parseAccessSpecifier(); // Treat as section label (C++)
  return;
}
```

The C++ path (`parseAccessSpecifier()`) consumes the keyword, expects a colon, and creates a new
unwrapped line - which is why `protected int x;` becomes `protected\nint x;`. It treats the keyword
as a standalone section header.

Java and C# just call `nextToken()`, treating the keyword as an inline modifier that stays on the
same line as the declaration. Angelscript needs the same behavior.

**Fix:** Add `Style.isUnrealEngineAngelscript()` to the Java/C#/JS check in `UnwrappedLineParser.cpp`
so that Angelscript access specifiers are treated as inline modifiers rather than section labels.

## How clang-format already handles similar cases

There is existing precedent for string literal prefixes. In the `spaceRequiredBetween()` function
(line ~5017-5025), C++ string prefixes are handled when followed by `#`:

```cpp
// u#str, U#str, L#str, u8#str
// uR#str, UR#str, LR#str, u8R#str
if (Right.is(tok::hash) && Left.is(tok::identifier) &&
    (Left.TokenText == "L" || Left.TokenText == "u" ||
     Left.TokenText == "U" || Left.TokenText == "u8" ||
     Left.TokenText == "LR" || Left.TokenText == "uR" ||
     Left.TokenText == "UR" || Left.TokenText == "u8R")) {
  return false;
}
```

Note that the standard C++ prefixes (L, u, U, u8) don't hit the identifier+literal rule because
the C++ lexer merges them into a single `string_literal` token during tokenization. The `#`-based
check above is for a different scenario.

There's also token merging in `FormatTokenLexer.cpp` for language-specific prefixes:
- `tryMergeNSStringLiteral()` - merges `@"..."` for Objective-C
- `tryMergeCSharpStringLiteral()` - merges `$"..."` and `@"..."` for C#

## Plan: Add Angelscript as a real language in clang-format

Register `LK_UnrealEngineAngelscript` as a first-class language in clang-format. Since Angelscript (UE variant)
is syntactically very close to C++, this language would inherit almost all C++ behavior and only override
specific things (like string prefixes). This gives us a clean place to hang all future Angelscript-specific
behavior without polluting the C++ code path.

**Files that need changes to register the language:**

1. `clang/include/clang/Format/Format.h`
   - Add `LK_UnrealEngineAngelscript` to the `LanguageKind` enum (after `LK_TextProto`, before `LK_Verilog`)
   - Add helper: `bool isUnrealEngineAngelscript() const { return Language == LK_UnrealEngineAngelscript; }`
   - Add case to `getLanguageName()` returning `"UnrealEngineAngelscript"`

2. `clang/lib/Format/Format.cpp`
   - Add YAML enum case in `ScalarEnumerationTraits<FormatStyle::LanguageKind>`:
     `IO.enumCase(Value, "UnrealEngineAngelscript", FormatStyle::LK_UnrealEngineAngelscript);`
   - Add file extension mapping in `getLanguageByFileName()`:
     `.as` -> `LK_UnrealEngineAngelscript`

3. `clang/lib/Format/TokenAnnotator.cpp`
   - In `spaceRequiredBefore()`, add a check gated on `isUnrealEngineAngelscript()` that returns `false`
     when left token is `f` or `n` and right token is a string literal.

4. `clang/lib/Format/UnwrappedLineParser.cpp`
   - In `parseStatement()`, add `Style.isUnrealEngineAngelscript()` to the Java/C#/JS check for
     access specifier keywords, so they are treated as inline modifiers instead of section labels.

5. `clang/unittests/Format/` - Create test file `FormatTestUnrealEngineAngelscript.cpp`

**How existing languages are defined for reference:**

The `LanguageKind` enum lives in `Format.h` (line ~3690). Helper methods are defined nearby (line ~3720):
```cpp
bool isCpp() const { return Language == LK_Cpp || Language == LK_C || Language == LK_ObjC; }
bool isCSharp() const { return Language == LK_CSharp; }
bool isJavaScript() const { return Language == LK_JavaScript; }
// etc.
```

Language-specific behavior is gated throughout the codebase with checks like
`if (Style.isCSharp()) { ... }`. There are 290+ such checks across the formatting files, so the
pattern is well-established.

**Design decision:** `isCpp()` will include `LK_UnrealEngineAngelscript`, so we inherit all C++ formatting
rules by default and only need to add overrides for the differences. Can revisit later if needed.

## Open issues found during real-world testing

Formatted all `.as` files in `AngelScript/Script-Examples/` and reviewed the diff. Most changes
are correct (whitespace normalization, trailing whitespace removal, consistent spacing around
operators). The following issues need to be addressed:

### `&out` and `&in` parameter modifiers get split

Angelscript uses `&out` and `&in` as reference qualifiers on parameters. clang-format sees the
`&` as a C++ reference operator and `out`/`in` as separate identifiers, inserting a space.

```angelscript
// Before formatting (correct):
void ExampleFunction(FVector&out OutPosition)
void OnAttributeChanged(const FAngelscriptModifiedAttribute&in Data)

// After formatting (broken):
void ExampleFunction(FVector& out OutPosition)
void OnAttributeChanged(const FAngelscriptModifiedAttribute& in Data)
```

### `access:` specifier syntax mangled

Angelscript has a custom `access:SpecifierName` syntax for fine-grained access control. clang-format
doesn't understand this and reformats it incorrectly - adding spaces around the colon and collapsing
the declaration onto one line.

```angelscript
// Before formatting (correct):
access:Internal
float PrivateFloatValue = 0.0;

// After formatting (broken):
access : Internal float PrivateFloatValue = 0.0;
```

### Spaces removed in access specifier declarations with modifiers

In access specifier declarations, the space before the parenthesized modifier list is meaningful
but gets removed by clang-format.

```angelscript
// Before formatting (correct):
access SpecifierCapabilityCanOnlyRead = private, UAccessSpecifierComponent (readonly);
access EditAndReadOnly = private, * (editdefaults, readonly);

// After formatting (broken):
access SpecifierCapabilityCanOnlyRead = private, UAccessSpecifierComponent(readonly);
access EditAndReadOnly = private, *(editdefaults, readonly);
```

## Building clang-format

From the repo root:
```
mkdir build && cd build
cmake -G Ninja -DLLVM_ENABLE_PROJECTS=clang ../llvm
ninja clang-format
```

The built binary will be at `build/bin/clang-format`. To test:
```
./build/bin/clang-format --style=file:AngelScript/.clang-format AngelScript/FormatTest.as
```

# Analyzing the full language

I have downloaded the documentation for Unreal-Angelscript, can you read through it and see if there is
anything more formatting related we may need to pay attention to?

@~/github.com/Hazelight/Docs-UnrealEngine-Angelscript/content

## Findings

Read through the full Unreal-Angelscript documentation. The following constructs differ from C++ syntax
and were flagged as potential formatting issues. However, all of them **format correctly already** because
clang-format treats the unknown keywords as identifiers and the surrounding syntax is valid enough C++.

Verified by running clang-format on examples of each:

### `default` statements in class body (formats OK)
```angelscript
default bReplicates = true;
default Tags.Add(n"ExampleTag");
default Mesh.RelativeLocation = FVector(0.0, 0.0, 0.0);
```
clang-format sees `default` as a keyword but it appears inside a class body where `default:`
would be invalid, so it falls through to normal statement formatting.

### `delegate` and `event` type declarations (format OK)
```angelscript
delegate void FExampleDelegate(UObject Object, float Value);
event void FExampleEvent(UObject Object, float Value);
```
Treated as `identifier type function(params);` which looks like a valid C++ declaration.

### `mixin` function modifier (formats OK)
```angelscript
mixin void ExampleMixin(AActor Self, FVector Location)
{
}
```
Treated as a type name followed by a function declaration.

### `property` function qualifier (formats OK)
```angelscript
FVector GetRotatedOffset() const property
{
}
```
`property` after `const` is treated as an identifier, similar to `override`/`final`.

### `float32` / `float64` types (format OK)
Treated as user-defined types, formats normally.

### Conclusion
No additional formatting fixes are needed at this time. All Angelscript-specific syntax that
differs from C++ happens to format correctly with the current implementation.

# Really long lines with contatenated format strings
In @FormatTest.as I have added a really long string where two part format strings are concatenated.
In this case clang-format separates the f to the line before the second string constant breaking the code.

Add a unit test to replicate the error, and fix the issue

# Deep dive in including Angelscript in isCpp()

I want us to deeply analyze what it would mean to not consider angelscript as C++.
Is there really a lot of places where isCpp() is used that is relevant to angelscript?

Analyse each isCpp() invocation please, and describe what they do below here

## Analysis: What breaks if Angelscript is NOT in `isCpp()`?

Beyond the 13 direct `Style.isCpp()` call sites, the cached `IsCpp` member variable is used
**18 times in UnwrappedLineParser.cpp** and **27 times in TokenAnnotator.cpp**. These are the
ones that matter most - they control how tokens are classified and how lines are structured.

### Parser behavior that would break (`UnwrappedLineParser.cpp` - 18 `IsCpp` uses)

These are all gated on the cached `IsCpp` member. If `isCpp()` returns false, none of them fire.

**Would break Angelscript formatting:**

- **Braced init list detection** (line 566) - Identifies `{...}` as braced initializer lists when
  preceded by a literal or followed by paren/arrow. Without this, `TArray<int> Arr = {1, 2, 3}`
  gets misformatted as a block instead of an initializer.

- **Lambda parsing** (line 2282) - `[...]` lambda expressions are only parsed as lambdas in C++.
  Without this, Angelscript lambdas would be treated as array subscripts and formatting would break.

- **C++ attributes `[[...]]`** (line 1450) - Parses `[[...]]` attribute syntax. Angelscript uses
  UE macros like `UFUNCTION()` rather than `[[...]]`, but if any appear they'd be misparsed.

- **Enum class/struct** (line 3830) - Parses `enum class` and `enum struct`. Without this,
  `enum class EMyEnum` would not be recognized as a scoped enum.

- **Structured bindings** (line 2424) - `auto [x, y] = ...` would be misparsed without this.
  Angelscript may not use these, but the parser would still encounter `[` tokens in other contexts
  and the fallback path could misclassify them.

**Harmless if missing (Angelscript doesn't use these features):**

- **Export block** (line 1639) - C++20 `export namespace` / `export { ... }`. Not in Angelscript.
- **Module import** (line 1691) - C++20 `import` statements. Not in Angelscript.
- **Qt signals/slots** (line 1694) - Qt-specific `signals:` / `slots:`. Not in Angelscript.
- **Statement macros** (line 1703) - C++ preprocessor statement macros. Not in Angelscript.
- **Namespace macros** (line 1707) - C++ namespace macros. Not in Angelscript.
- **Requires expressions** (line 1803) - C++20 `requires`. Not in Angelscript.
- **co_await in for loops** (line 3319) - C++20 coroutines. Not in Angelscript.
- **K&R function definitions** (line 1891) - Ancient C syntax. Not relevant.
- **Macro body braced lists** (line 607) - Preprocessor macros. Not in Angelscript.

**Inverted check (would wrongly activate):**

- **Enum unwrapped line** (line 1825) - `if (!IsCpp) addUnwrappedLine()` after enum. If Angelscript
  is NOT cpp, this fires and adds extra line breaks after enums like Java/C# do. Would change enum
  formatting.
- **Interface keyword** (line 2024) - `if (!IsCpp && ... kw_interface)`. Would start recognizing
  `interface` as a keyword. Angelscript doesn't use `interface`, so harmless.

### Token annotation that would break (`TokenAnnotator.cpp` - 27 `IsCpp` uses)

**Would break Angelscript formatting:**

- **C-style cast detection** (lines 2719, 2745, 2829) - `(Type)value` casts are only detected for
  C++. Without this, casts would be formatted as function calls with wrong spacing.

- **Constructor/destructor names** (line 3848) - `MyClass()` and `~MyClass()` are marked as
  ctor/dtor declaration names only for C++. Without this, constructors and destructors get wrong
  formatting (treated as regular function calls).

- **Overloaded operator spacing** (line 5115) - Spacing around `operator<<`, `operator+` etc.
  Angelscript has operator overloads, so this matters.

- **Adjacent string literal breaking** (line 5859) - Controls whether adjacent string literals
  like `"foo" "bar"` can be broken across lines. Relevant for Angelscript string concatenation.

- **Function declaration detection** (lines 3967, 4003, 4101) - Skipping attributes in function
  declarations, detecting default parameters, and setting function brace types. Without these,
  function declarations could be misformatted.

- **Square bracket classification** (lines 691, 697, 705, 707, 727, 734, 744, 807, 812, 875) -
  A large block of logic that distinguishes `[` as: C++ attribute `[[`, structured binding `[x,y]`,
  lambda capture `[&]`, array subscript `a[i]`, or ObjC message `[obj msg]`. Without `IsCpp`,
  the parser skips most of these checks and falls through to simpler classification. This would
  misclassify lambdas and could cause cascading formatting errors.

- **Colon in dict literal** (line 1234) - Affects whether `:` inside braces is treated as a dict
  literal separator or something else. Could affect brace formatting.

**Harmless if missing:**

- **Requires expression** (line 1596) - C++20. Not in Angelscript.
- **co_await in for** (line 1507) - C++20. Not in Angelscript.
- **Calling conventions** (line 1793) - `__cdecl` etc. Not in Angelscript.
- **User-defined conversions** (line 1652) - `operator Type()`. Not in Angelscript.
- **Attribute after if/switch** (line 1314) - `[[likely]]` etc. Not in Angelscript.

### Direct `Style.isCpp()` call sites

**Would break if false:**

- **Analyzer passes** (`Format.cpp:4180`) - `QualifierAlignment`, `RemoveParentheses`, `InsertBraces`,
  `RemoveBracesLLVM`, `RemoveSemicolon` would all stop working. These are opt-in, but if a user
  enables them in their `.clang-format` they'd silently do nothing.

- **Comment backslash handling** (`BreakableToken.cpp:101`, `FormatTokenLexer.cpp:1388`) - Without
  the C++ path, backslash at end of `//` comments would be handled like Java/JS (always truncate).
  Minor difference but could change comment formatting.

- **Lambda indentation** (`ContinuationIndenter.cpp:406, 800`) - Lambda body indent and argument
  break control would use non-C++ paths. Would degrade lambda formatting.

- **Template closer line breaks** (`ContinuationIndenter.cpp:511`) - After `TArray<int>`, the
  logic that prevents unnecessary line breaks before the variable name would not apply. Could cause
  unwanted breaks in template-heavy Angelscript code.

**Would improve if false:**

- **Numeric literal separator** (`NumericLiteralCaseFixer.cpp:79`) - Would correctly use `_` instead
  of `'`. This is the one place where NOT being C++ is actually better.

**Neutral:**

- **Preprocessor directive typing** (`UnwrappedLineParser.cpp:1044`) - No preprocessor in AS, no-op.
- **Sort/fix includes** (`Format.cpp:3957, 4025`) - No `#include` in AS, no-op.
- **Nested block special case** (`ContinuationIndenter.cpp:1296`) - Inverted check (`!isCpp()`).
  If AS is not cpp, it enters the Java/C# special case for nested blocks - slightly different
  bin-packing but unlikely to be noticeable.
- **Integer suffix stripping** (`IntegerLiteralSeparatorFixer.cpp:130`) - Only matters with
  non-default options.

### Conclusion

Removing Angelscript from `isCpp()` would break formatting in at least these critical areas:

1. **Lambda parsing** - lambdas misclassified as array subscripts
2. **Braced initializer lists** - `{1, 2, 3}` treated as blocks instead of init lists
3. **Square bracket classification** - 10 checks that distinguish `[[attr]]`, `[&]`, `a[i]`
4. **Constructor/destructor detection** - ctors formatted as function calls
5. **C-style cast detection** - `(Type)value` spacing broken
6. **Operator overload spacing** - `operator+` etc. formatted wrong
7. **Function declaration detection** - multiple heuristics lost

These are fundamental to formatting C++-like code. Keeping Angelscript in `isCpp()` is clearly
the right choice. The only downside is the numeric literal separator (`'` vs `_`), which is
opt-in and easily fixable with a single targeted guard if ever needed.
