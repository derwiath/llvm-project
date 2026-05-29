//===- unittest/Format/FormatTestUnrealEngineAngelscript.cpp ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "FormatTestBase.h"

#define DEBUG_TYPE "format-test-unreal-engine-angelscript"

namespace clang {
namespace format {
namespace test {
namespace {

class FormatTestUnrealEngineAngelscript : public FormatTestBase {
protected:
  FormatStyle getDefaultStyle() const override {
    FormatStyle Style = getLLVMStyle(FormatStyle::LK_UnrealEngineAngelscript);

    // Only settings that affect the current test cases.
    Style.IndentWidth = 2;
    Style.PointerAlignment = FormatStyle::PAS_Left;
    Style.AllowShortFunctionsOnASingleLine = FormatStyle::ShortFunctionStyle();

    Style.BreakBeforeBraces = FormatStyle::BS_Custom;
    Style.BraceWrapping.AfterClass = true;
    Style.BraceWrapping.AfterFunction = true;
    Style.BraceWrapping.SplitEmptyFunction = true;

    return Style;
  }
};

TEST_F(FormatTestUnrealEngineAngelscript, StringPrefixF) {
  verifyFormat("return f\"Value {Value}\";");
  verifyFormat("FString S = f\"hello\";");
  // The f prefix must stay attached to the string when the line wraps.
  verifyFormat("return f\"Value ======================================"
               "==========================================\" +\n"
               "       f\"{Value}\";");
}

TEST_F(FormatTestUnrealEngineAngelscript, StringPrefixN) {
  verifyFormat("return n\"JohnDoe\";");
  verifyFormat("FName Name = n\"Something\";");
}

TEST_F(FormatTestUnrealEngineAngelscript, StringPrefixRequiresAdjacency) {
  // A space between the prefix and the quote means it is an ordinary
  // identifier followed by a string, not a format-string prefix. The two must
  // stay separate so the spacing is normalized rather than frozen into a
  // single merged token.
  verifyFormat("return f \"hello\";", "return f   \"hello\";");
  verifyFormat("FName Name = n \"x\";", "FName Name = n   \"x\";");
}

TEST_F(FormatTestUnrealEngineAngelscript, StringPrefixIsLanguageGated) {
  // The f"..." / n"..." merge is Angelscript-specific. Under C++ the prefix is
  // an ordinary identifier, so clang-format keeps it separate from the string.
  verifyFormat("FString S = f\"hello\";");
  verifyFormat("FString S = f \"hello\";", "FString S = f\"hello\";",
               getLLVMStyle());
}

TEST_F(FormatTestUnrealEngineAngelscript, RefQualifierIn) {
  verifyFormat("void Func(const FHitResult&in Hit)\n"
               "{\n"
               "}");
  verifyFormat("void Func(const FVector&in Position)\n"
               "{\n"
               "}");
}

TEST_F(FormatTestUnrealEngineAngelscript, RefQualifierInOut) {
  verifyFormat("void Func(FVector&inout Position)\n"
               "{\n"
               "}");
}

TEST_F(FormatTestUnrealEngineAngelscript, RefQualifierOut) {
  verifyFormat("void Func(FVector&out OutPosition)\n"
               "{\n"
               "}");
  verifyFormat("void Func(int&out OutValue, float&out OutFloat)\n"
               "{\n"
               "}");
}

TEST_F(FormatTestUnrealEngineAngelscript, InlineAccessSpecifiers) {
  verifyFormat("class UMyClass\n"
               "{\n"
               "  int PublicMember;\n"
               "  protected int ProtectedMember;\n"
               "  private int PrivateMember;\n"
               "};");
}

TEST_F(FormatTestUnrealEngineAngelscript, InlineAccessSpecifiersOnMethods) {
  verifyFormat("class UMyClass\n"
               "{\n"
               "  int GetPublic() const\n"
               "  {\n"
               "    return X;\n"
               "  }\n"
               "  protected int GetProtected() const\n"
               "  {\n"
               "    return Y;\n"
               "  }\n"
               "  private int GetPrivate() const\n"
               "  {\n"
               "    return Z;\n"
               "  }\n"
               "};");
}

TEST_F(FormatTestUnrealEngineAngelscript, AccessSpecifierModifierParenSpacing) {
  verifyFormat("access Foo = private, UComponent (readonly);");
  verifyFormat("access Bar = private, * (editdefaults, readonly);");
}

TEST_F(FormatTestUnrealEngineAngelscript,
       AccessSpecifierModifierParenSpacingWithCall) {
  // The space is forced only before the trailing modifier parentheses, not
  // before an ordinary call paren elsewhere on the access declaration.
  verifyFormat("access Foo = Helper(x), Bar (readonly);");
}

TEST_F(FormatTestUnrealEngineAngelscript, AccessSpecifierCustomAccess) {
  verifyNoChange("access:Foo\n"
                 "float PrivateFloatValue = 0.0;");
  verifyNoChange("access:Bar\n"
                 "void AccessibleMethod()\n"
                 "{\n"
                 "}");
}

TEST_F(FormatTestUnrealEngineAngelscript, RealisticClass) {
  // Members and methods with mixed inline access specifiers and an &in
  // reference parameter, to exercise interactions between the features.
  verifyFormat("class UMyActor\n"
               "{\n"
               "  int Health;\n"
               "  protected float Speed;\n"
               "  private bool bDead;\n"
               "  void TakeDamage(int&in Amount)\n"
               "  {\n"
               "    Health -= Amount;\n"
               "  }\n"
               "  protected int GetHealth() const\n"
               "  {\n"
               "    return Health;\n"
               "  }\n"
               "  private void Die()\n"
               "  {\n"
               "    bDead = true;\n"
               "  }\n"
               "};");
}

} // namespace
} // namespace test
} // namespace format
} // namespace clang
