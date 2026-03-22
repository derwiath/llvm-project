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
    FormatStyle Style = getLLVMStyle();
    Style.Language = FormatStyle::LK_UnrealEngineAngelscript;

    // Only settings that affect the current test cases.
    Style.TabWidth = 4;
    Style.UseTab = FormatStyle::UT_Always;
    Style.IndentWidth = 4;
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
               "\t   f\"{Value}\";");
}

TEST_F(FormatTestUnrealEngineAngelscript, StringPrefixN) {
  verifyFormat("return n\"JohnDoe\";");
  verifyFormat("FName Name = n\"Something\";");
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
               "\tint PublicMember;\n"
               "\tprotected int ProtectedMember;\n"
               "\tprivate int PrivateMember;\n"
               "};");
}

TEST_F(FormatTestUnrealEngineAngelscript, InlineAccessSpecifiersOnMethods) {
  verifyFormat("class UMyClass\n"
               "{\n"
               "\tint GetPublic() const\n"
               "\t{\n"
               "\t\treturn X;\n"
               "\t}\n"
               "\tprotected int GetProtected() const\n"
               "\t{\n"
               "\t\treturn Y;\n"
               "\t}\n"
               "\tprivate int GetPrivate() const\n"
               "\t{\n"
               "\t\treturn Z;\n"
               "\t}\n"
               "};");
}

TEST_F(FormatTestUnrealEngineAngelscript, AccessSpecifierCustomAccess) {
  verifyNoChange("access:Internal\n"
                 "float PrivateFloatValue = 0.0;");
  verifyNoChange("access:InternalWithCapability\n"
                 "void AccessibleMethod()\n"
                 "{\n"
                 "}");
}

} // namespace
} // namespace test
} // namespace format
} // namespace clang
