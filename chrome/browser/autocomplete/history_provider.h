// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_HISTORY_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_HISTORY_PROVIDER_H_

#include "base/compiler_specific.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/history/in_memory_url_index_types.h"

class AutocompleteInput;
struct AutocompleteMatch;

// This class is a base class for the history autocomplete providers and
// provides functions useful to all derived classes.
class HistoryProvider : public AutocompleteProvider {
 public:
  virtual void DeleteMatch(const AutocompleteMatch& match) OVERRIDE;

 protected:
  HistoryProvider(AutocompleteProviderListener* listener,
                  Profile* profile,
                  AutocompleteProvider::Type type);
  virtual ~HistoryProvider();

  // Finds and removes the match from the current collection of matches and
  // backing data.
  void DeleteMatchFromMatches(const AutocompleteMatch& match);

  // Returns true if inline autocompletion should be prevented. Use this instead
  // of |input.prevent_inline_autocomplete| if the input is passed through
  // FixupUserInput(). This method returns true if
  // |input.prevent_inline_autocomplete()| is true or the input text contains
  // trailing whitespace.
  bool PreventInlineAutocomplete(const AutocompleteInput& input);

  // Fill and return an ACMatchClassifications structure given the |matches|
  // to highlight.
  static ACMatchClassifications SpansFromTermMatch(
      const history::TermMatches& matches,
      size_t text_length,
      bool is_url);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_HISTORY_PROVIDER_H_
