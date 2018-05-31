/* The MIT License (MIT)
 *
 * Copyright (c) 2014-2018 David Medina and Tim Warburton
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 */
#include "../parserUtils.hpp"
#include "modes/okl.hpp"

void testLoops();
void testTypes();
void testLoopSkips();

void testLoopsErrors();
void testTypesErrors();
void testLoopSkipsErrors();

int main(const int argc, const char **argv) {
  parser.addAttribute<dummy>();
  parser.addAttribute<attributes::kernel>();
  parser.addAttribute<attributes::outer>();
  parser.addAttribute<attributes::inner>();
  parser.addAttribute<attributes::shared>();
  parser.addAttribute<attributes::exclusive>();

  testLoops();
  // testTypes();
  // testLoopSkips();

  // testLoopsErrors();
  // testTypesErrors();
  // testLoopSkipsErrors();

  return 0;
}

#define parseBadOKLSource(src_)                     \
  parseSource(src_);                                \
  OCCA_ASSERT_FALSE(okl::checkKernels(parser.root))



//---[ Loop ]---------------------------
void testOKLLoopExists();
void testProperOKLLoops();
void testInnerInsideOuter();
void testSameInnerLoopCount();

void testLoops() {
  testOKLLoopExists();
  testProperOKLLoops();
  testInnerInsideOuter();
  testSameInnerLoopCount();
}

void testOKLLoopExists() {
  // @outer + @inner exist
  parseBadOKLSource("@kernel void foo() {}");
  parseBadOKLSource("@kernel void foo() {\n"
                    "  for (;;; @outer) {}\n"
                    "}");
  parseBadOKLSource("@kernel void foo() {\n"
                    "  for (;;; @inner) {}\n"
                    "}");
}

void testProperOKLLoops() {
  // Proper loops (decl, update, inc)
  const std::string oStart = (
    "@kernel void foo() {\n"
  );
  const std::string oMid = (
    "\nfor (int i = 0; i < 2; ++i; @inner) {}\n"
  );
  const std::string oEnd = (
    "\n}\n"
  );

  const std::string iStart = (
    "@kernel void foo() {\n"
    "for (int o = 0; o < 2; ++o; @outer) {\n"
  );
  const std::string iEnd = (
    "\n}\n"
    "}\n"
  );

  parseBadOKLSource(oStart + "for (o = 0;;; @outer) {" + oMid + "}" + oEnd);
  parseBadOKLSource(oStart + "for (float o = 0;;; @outer) {" + oMid + "}" + oEnd);
  parseBadOKLSource(oStart + "for (int o = 0, j = 0;;; @outer) {" + oMid + "}" + oEnd);
  parseBadOKLSource(oStart + "for (int o = 0;;; @outer) {" + oMid + "}" + oEnd);
  parseBadOKLSource(oStart + "for (int o = 0; o + 2;; @outer) {" + oMid + "}" + oEnd);
  parseBadOKLSource(oStart + "for (int o = 0; j < 2;; @outer) {" + oMid + "}" + oEnd);
  parseBadOKLSource(oStart + "for (int o = 0; o < 2;; @outer) {" + oMid + "}" + oEnd);
  parseBadOKLSource(oStart + "for (int o = 0; o < 2; o *= 2; @outer) {" + oMid + "}" + oEnd);
  parseBadOKLSource(oStart + "for (int o = 0; o < 2; ++j; @outer) {" + oMid + "}" + oEnd);

  parseBadOKLSource(iStart + "for (i = 0;;; @inner) {}" + iEnd);
  parseBadOKLSource(iStart + "for (float i = 0;;; @inner) {}" + iEnd);
  parseBadOKLSource(iStart + "for (int i = 0, j = 0;;; @inner) {}" + iEnd);
  parseBadOKLSource(iStart + "for (int i = 0;;; @inner) {}" + iEnd);
  parseBadOKLSource(iStart + "for (int i = 0; i + 2;; @inner) {}" + iEnd);
  parseBadOKLSource(iStart + "for (int i = 0; j < 2;; @inner) {}" + iEnd);
  parseBadOKLSource(iStart + "for (int i = 0; i < 2;; @inner) {}" + iEnd);
  parseBadOKLSource(iStart + "for (int i = 0; i < 2; i *= 2; @inner) {}" + iEnd);
  parseBadOKLSource(iStart + "for (int i = 0; i < 2; ++j; @inner) {}" + iEnd);
}

void testInnerInsideOuter() {
  // @outer > @inner
  parseBadOKLSource(
    "@kernel void foo() {\n"
    "  for (int i = 0; i < 2; ++i; @inner) {\n"
    "    for (int o = 0; o < 2; ++o; @outer) {}\n"
    "  }\n"
    "}\n"
  );
  parseBadOKLSource(
    "@kernel void foo() {\n"
    "  for (int o = 0; o < 2; ++o; @outer) {\n"
    "    for (int i = 0; i < 2; ++i; @inner) {\n"
    "      for (int o2 = 0; o2 < 2; ++o2; @outer) {}\n"
    "    }\n"
    "  }\n"
    "}\n"
  );
}

void testSameInnerLoopCount() {
  // Same # of @inner in each @outer
  parseBadOKLSource(
    "@kernel void foo() {\n"
    "  for (int o = 0; o < 2; ++o; @outer) {\n"
    "    for (int i = 0; i < 2; ++i; @inner) {}\n"
    "    for (int i = 0; i < 2; ++i; @inner) {\n"
    "      for (int i2 = 0; i2 < 2; ++i2; @inner) {\n"
    "      }\n"
    "    }\n"
    "  }\n"
    "}\n"
  );
}

//======================================

//---[ Types ]--------------------------
void testSharedLocation();
void testExclusiveLocation();
void testValidSharedArray();

void testTypes() {
  testSharedLocation();
  testExclusiveLocation();
  testValidSharedArray();
}

void testSharedLocation() {
  // @outer > @shared > @inner
  parseBadOKLSource(
    "@kernel void foo() {\n"
    "  @shared int s[10];\n"
    "  for (int o = 0; o < 2; ++o; @outer) {\n"
    "    for (int i = 0; i < 2; ++i; @inner) {\n"
    "    }\n"
    "  }\n"
    "}\n"
  );
  parseBadOKLSource(
    "@kernel void foo() {\n"
    "  for (int o = 0; o < 2; ++o; @outer) {\n"
    "    for (int i = 0; i < 2; ++i; @inner) {\n"
    "      @shared int s[10];\n"
    "    }\n"
    "  }\n"
    "}\n"
  );
}

void testExclusiveLocation() {
  // @outer > @exclusive > @inner
  parseBadOKLSource(
    "@kernel void foo() {\n"
    "  @exclusive int x;\n"
    "  for (int o = 0; o < 2; ++o; @outer) {\n"
    "    for (int i = 0; i < 2; ++i; @inner) {\n"
    "    }\n"
    "  }\n"
    "}\n"
  );
  parseBadOKLSource(
    "@kernel void foo() {\n"
    "  for (int o = 0; o < 2; ++o; @outer) {\n"
    "    for (int i = 0; i < 2; ++i; @inner) {\n"
    "      @exclusive int x;\n"
    "    }\n"
    "  }\n"
    "}\n"
  );
}

void testValidSharedArray() {
  // @shared has an array with evaluable sizes
  parseBadOKLSource(
    "@kernel void foo() {\n"
    "  for (int o = 0; o < 2; ++o; @outer) {\n"
    "    @shared int s[o];\n"
    "    for (int i = 0; i < 2; ++i; @inner) {\n"
    "    }\n"
    "  }\n"
    "}\n"
  );
  parseBadOKLSource(
    "@kernel void foo() {\n"
    "  for (int o = 0; o < 2; ++o; @outer) {\n"
    "    @shared int s[2][o];\n"
    "    for (int i = 0; i < 2; ++i; @inner) {\n"
    "    }\n"
    "  }\n"
    "}\n"
  );
}

void testSharedLocationErrors();
void testExclusiveLocationErrors();
void testValidSharedArrayErrors();

void testTypesErrors() {
  testSharedLocationErrors();
  testExclusiveLocationErrors();
  testValidSharedArrayErrors();
}

void testSharedLocationErrors() {
  // @outer > @shared > @inner
  parseBadOKLSource(
    "@kernel void foo() {\n"
    "  @shared int s[10];\n"
    "  for (int o = 0; o < 2; ++o; @outer) {\n"
    "    for (int i = 0; i < 2; ++i; @inner) {\n"
    "    }\n"
    "  }\n"
    "}\n"
  );
  parseBadOKLSource(
    "@kernel void foo() {\n"
    "  for (int o = 0; o < 2; ++o; @outer) {\n"
    "    for (int i = 0; i < 2; ++i; @inner) {\n"
    "      @shared int s[10];\n"
    "    }\n"
    "  }\n"
    "}\n"
  );
}

void testExclusiveLocationErrors() {
  // @outer > @exclusive > @inner
  parseBadOKLSource(
    "@kernel void foo() {\n"
    "  @exclusive int x;\n"
    "  for (int o = 0; o < 2; ++o; @outer) {\n"
    "    for (int i = 0; i < 2; ++i; @inner) {\n"
    "    }\n"
    "  }\n"
    "}\n"
  );
  parseBadOKLSource(
    "@kernel void foo() {\n"
    "  for (int o = 0; o < 2; ++o; @outer) {\n"
    "    for (int i = 0; i < 2; ++i; @inner) {\n"
    "      @exclusive int x;\n"
    "    }\n"
    "  }\n"
    "}\n"
  );
}

void testValidSharedArrayErrors() {
  // @shared has an array with evaluable sizes
  parseBadOKLSource(
    "@kernel void foo() {\n"
    "  for (int o = 0; o < 2; ++o; @outer) {\n"
    "    @shared int s[o];\n"
    "    for (int i = 0; i < 2; ++i; @inner) {\n"
    "    }\n"
    "  }\n"
    "}\n"
  );
  parseBadOKLSource(
    "@kernel void foo() {\n"
    "  for (int o = 0; o < 2; ++o; @outer) {\n"
    "    @shared int s[2][o];\n"
    "    for (int i = 0; i < 2; ++i; @inner) {\n"
    "    }\n"
    "  }\n"
    "}\n"
  );
}
//======================================

//---[ Loop Skips ]---------------------
void testValidBreaks();
void testValidContinues();

void testLoopSkips() {
  testValidBreaks();
  testValidContinues();
}

void testValidBreaks() {
  // No break in @outer/@inner (ok inside regular loops inside @outer/@inner)
  parseBadOKLSource(
    "@kernel void foo() {\n"
    "  for (int o = 0; o < 2; ++o; @outer) {\n"
    "    for (int i = 0; i < 2; ++i; @inner) {\n"
    "    }\n"
    "    break;"
    "  }\n"
    "}\n"
  );
  parseBadOKLSource(
    "@kernel void foo() {\n"
    "  for (int o = 0; o < 2; ++o; @outer) {\n"
    "    for (int i = 0; i < 2; ++i; @inner) {\n"
    "      break;"
    "    }\n"
    "  }\n"
    "}\n"
  );
}

void testValidContinues() {
  // No continue in @inner (ok inside regular loops inside @outer/@inner)
  parseBadOKLSource(
    "@kernel void foo() {\n"
    "  for (int o = 0; o < 2; ++o; @outer) {\n"
    "    for (int i = 0; i < 2; ++i; @inner) {\n"
    "    }\n"
    "    continue;"
    "  }\n"
    "}\n"
  );
  parseBadOKLSource(
    "@kernel void foo() {\n"
    "  for (int o = 0; o < 2; ++o; @outer) {\n"
    "    for (int i = 0; i < 2; ++i; @inner) {\n"
    "      continue;"
    "    }\n"
    "  }\n"
    "}\n"
  );
}
//======================================