/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <stdio.h>
#include "plstr.h"
#include "nsCSSProps.h"
#include "nsCSSKeywords.h"
#include "nsString.h"
#include "nsXPCOM.h"

using namespace mozilla;

static const char* const kJunkNames[] = {
  nullptr,
  "",
  "123",
  "backgroundz",
  "zzzzzz",
  "#@$&@#*@*$@$#"
};

bool
TestKeywords()
{
  nsCSSKeywords::AddRefTable();

  bool success = true;
  nsCSSKeyword id;
  nsCSSKeyword index;

  extern const char* const kCSSRawKeywords[];

  // First make sure we can find all of the tags that are supposed to
  // be in the table. Futz with the case to make sure any case will
  // work
  const char*const*  et = &kCSSRawKeywords[0];
  const char*const*  end = &kCSSRawKeywords[eCSSKeyword_COUNT - 1];
  index = eCSSKeyword_UNKNOWN;
  while (et < end) {
    char tagName[512];
    char* underscore = &(tagName[0]);

    PL_strcpy(tagName, *et);
    while (*underscore) {
      if (*underscore == '_') {
        *underscore = '-';
      }
      underscore++;
    }
    index = nsCSSKeyword(int32_t(index) + 1);

    id = nsCSSKeywords::LookupKeyword(nsCString(tagName));
    if (id <= eCSSKeyword_UNKNOWN) {
      printf("bug: can't find '%s'\n", tagName);
      success = false;
    }
    if (id != index) {
      printf("bug: name='%s' id=%d index=%d\n", tagName, id, index);
      success = false;
    }

    // fiddle with the case to make sure we can still find it
    if (('a' <= tagName[0]) && (tagName[0] <= 'z')) {
      tagName[0] = tagName[0] - 32;
    }
    id = nsCSSKeywords::LookupKeyword(nsCString(tagName));
    if (id <= eCSSKeyword_UNKNOWN) {
      printf("bug: can't find '%s'\n", tagName);
      success = false;
    }
    if (id != index) {
      printf("bug: name='%s' id=%d index=%d\n", tagName, id, index);
      success = false;
    }
    et++;
  }

  // Now make sure we don't find some garbage
  for (int i = 0; i < (int) (sizeof(kJunkNames) / sizeof(const char*)); i++) {
    const char* const tag = kJunkNames[i];
    id = nsCSSKeywords::LookupKeyword(nsAutoCString(tag));
    if (eCSSKeyword_UNKNOWN < id) {
      printf("bug: found '%s'\n", tag ? tag : "(null)");
      success = false;
    }
  }

  nsCSSKeywords::ReleaseTable();
  return success;
}

int
main(void)
{
  nsresult rv = NS_InitXPCOM2(nullptr, nullptr, nullptr);
  NS_ENSURE_SUCCESS(rv, 2);

  bool testOK = true;
  testOK &= TestProps();
  testOK &= TestKeywords();

  rv = NS_ShutdownXPCOM(nullptr);
  NS_ENSURE_SUCCESS(rv, 2);

  return testOK ? 0 : 1;
}
