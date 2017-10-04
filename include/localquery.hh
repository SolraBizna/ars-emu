#ifndef LOCALQUERY_HH
#define LOCALQUERY_HH

#include "ars-emu.hh"
#include "byuuML_query.hh"

namespace ARS {
  const std::vector<std::string>& GetUILanguageList();
  class LocalizedQuery {
    byuuML::cursor here;
    std::string longest_match;
    void maybe_longer_match(const byuuML::cursor& where, std::string lang) {
      for(auto&& p : where.query("lang")) {
        auto& tag = p.data();
        if(tag != "*" && tag != "default") {
          bool matches;
          if(tag == lang) {
            // exact match
            matches = true;
          }
          else if(tag.size() < lang.size() && tag[lang.size()] == '-'
                  && std::equal(tag.begin(), tag.end(), lang.begin())) {
            // prefix match
            matches = true;
          }
          else matches = false;
          if(matches && tag.size() > longest_match.size()) {
            longest_match = tag;
          }
        }
      }
    }
    bool matches() const {
      auto q = here.query("lang");
      if(!q) return longest_match == "default";
      else {
        for(auto&& p : q) {
          if(p.data() == "*" || p.data() == longest_match) {
            return true;
          }
        }
      }
      return false;
    }
  public:
    LocalizedQuery(byuuML::cursor here) : LocalizedQuery(here, GetUILanguageList()) {}
    LocalizedQuery(byuuML::cursor here, const std::vector<std::string>& languages) : here(here) {
      for(auto language : languages) {
        maybe_longer_match(here, language);
        if(!longest_match.empty()) break;
      }
      if(longest_match.empty()) longest_match = "default";
    }
    LocalizedQuery begin() const { return *this; }
    LocalizedQuery end() const { return LocalizedQuery(here.end()); }
    LocalizedQuery& operator++() {
      do {
        ++here;
      } while(here && !matches());
      return *this;
    }
    bool operator!=(const LocalizedQuery& other) {
      return here != other.here;
    }
    byuuML::cursor& operator*() { return here; }
    byuuML::cursor* operator->() { return &here; }
  };
}

#endif
