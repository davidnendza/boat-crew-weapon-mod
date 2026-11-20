// drops.ini: MigrateDrops, LoadDrops, the roll engine, inventory save (a fragment of weaponmod.cpp, included in order, not standalone)
static void ClampCrateSettings() {
  for (int t = 0; t < 3; t++) { if (g_crateCost[t] < 1) g_crateCost[t] = 1; if (g_crateWeight[t] < 0) g_crateWeight[t] = 0; }
  if (g_crateWeight[0] + g_crateWeight[1] + g_crateWeight[2] <= 0) { g_crateWeight[0] = 70; g_crateWeight[1] = 25; g_crateWeight[2] = 5; }
  if (g_crateChance < 0) g_crateChance = 0; if (g_crateChance > 1) g_crateChance = 1;
  if (g_crateBonusUses < 0) g_crateBonusUses = 0;
}
// An existing drops.ini is never rewritten, so effects added in a newer version would never reach players who upgrade.
// Every built-in [affix] whose name is
// missing from the file is appended to it; the player's own edits stay. To switch an effect off, set its weight=0.
static void MigrateDrops(const std::string& path) {
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return;
  std::string text; char buf[4096]; size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
  fclose(f);
  std::string def = kDefaultDrops, add, names;
  size_t pos = 0;
  while ((pos = def.find("[template]\n", pos)) != std::string::npos) {
    size_t end = def.find("\n[", pos + 11);
    std::string block = def.substr(pos, end == std::string::npos ? std::string::npos : end + 1 - pos);
    pos = end == std::string::npos ? def.size() : end + 1;
    size_t lp = block.find("label="), fp = block.find("family=");
    if (lp == std::string::npos || fp == std::string::npos) continue;
    std::string label = block.substr(lp + 6, block.find('\n', lp) - lp - 6), fam = block.substr(fp + 7, block.find('\n', fp) - fp - 7);
    if (text.find("label=" + label + "\nfamily=" + fam + "\n") != std::string::npos || text.find("label=" + label + "\r\nfamily=" + fam + "\r\n") != std::string::npos) continue;
    add += "\n" + block; names += label + " (" + fam + ") ";
  }
  for (const char* head : { "[affix]\n", "[unique]\n" })
  for (pos = 0; (pos = def.find(head, pos)) != std::string::npos; ) {
    size_t end = def.find("\n[", pos + 8);
    std::string block = def.substr(pos, end == std::string::npos ? std::string::npos : end + 1 - pos);
    pos = end == std::string::npos ? def.size() : end + 1;
    size_t np = block.find("name="); if (np == std::string::npos) continue;
    std::string name = block.substr(np + 5, block.find('\n', np) - np - 5);
    if (text.find("name=" + name + "\n") != std::string::npos || text.find("name=" + name + "\r\n") != std::string::npos) continue;
    add += "\n" + block; names += name + " ";
  }
  std::string nl = text.find("\r\n") != std::string::npos ? "\r\n" : "\n", fixedNames;
  // migrated=N in [drops] records the numbered updates this file has had, so each applies once: a player who sets a
  // value back to an old default, or deletes an added line, keeps it. Entries with since=0 match
  // only text that is still exactly old and are safe to run every launch.
  const int kMigrations = 25;
  int done = 0;
  { size_t mp = text.find(nl + "migrated="); if (mp != std::string::npos) done = atoi(text.c_str() + mp + nl.size() + 9); }
  for (const AffixLineUpdate& u : kAffixLineUpdates) {
    if (u.since && u.since <= done) continue;
    size_t np = text.find(std::string("name=") + u.name + nl);
    if (np == std::string::npos || (np > 0 && text[np - 1] != '\n')) continue;
    size_t bs = text.rfind('[', np), be = text.find(nl + "[", np);
    if (bs == std::string::npos) bs = 0; if (be == std::string::npos) be = text.size();
    if (u.oldLine) {
      std::string ol = u.oldLine + nl;
      size_t lp = text.find(ol, np);
      if (lp == std::string::npos || lp >= be || text[lp - 1] != '\n') continue;   // edited by the player, or already new
      text.replace(lp, ol.size(), *u.newLine ? u.newLine + nl : std::string());   // an empty newLine deletes the line
    } else {
      std::string key = std::string(u.newLine).substr(0, strchr(u.newLine, '=') - u.newLine + 1);
      size_t kp = text.find(nl + key, bs);
      if (kp != std::string::npos && kp < be) continue;               // the affix already has that key
      text.insert(np + 5 + strlen(u.name) + nl.size(), u.newLine + nl);
    }
    fixedNames += std::string(u.name) + " ";
  }
  for (const UniqueBlockUpdate& u : kUniqueBlockUpdates) {
    if (u.since <= done) continue;
    std::string ob = u.oldBlock;
    ob = WithNewlines(ob, nl);
    size_t op = text.find(ob);
    if (op == std::string::npos || (op > 0 && text[op - 1] != '\n')) continue;   // edited by the player, or already new
    std::string head = std::string("[unique]\nname=") + u.name + "\n";
    size_t dp = def.find(head); if (dp == std::string::npos) continue;
    size_t de = def.find("\n[", dp + head.size());
    std::string nb = def.substr(dp, de == std::string::npos ? std::string::npos : de + 1 - dp);
    while (nb.size() > 1 && nb[nb.size() - 1] == '\n' && nb[nb.size() - 2] == '\n') nb.pop_back();   // the old block had no blank line
    nb = WithNewlines(nb, nl);
    text.replace(op, ob.size(), nb);
    fixedNames += std::string(u.name) + " ";
  }
  // [drops] settings whose defaults changed with the 2026-10-29 buff: a line still at the old default takes the new one,
  // and effect_counts / drawback_chance are written above uses= so they can be found. Only inside [drops]: the Lazarus
  // unique also has a uses=3 line.
  static const char* kDropsLineUpdates[][2] = { { "uses=3", "uses=4" }, { "unique_chance=0.02", "unique_chance=0.04" },
                                                { "unique_crate_chance=0.02,0.05,0.12", "unique_crate_chance=0.04,0.10,0.20" } };
  auto dropsEnd = [&](size_t ds) { size_t e = text.find(nl + "[", ds + 7); return e == std::string::npos ? text.size() : e; };
  size_t ds = text.find("[drops]");
  if (ds != std::string::npos && done < 1) {
    for (auto& u : kDropsLineUpdates) {
      std::string ol = nl + u[0] + nl; size_t lp = text.find(ol, ds);
      if (lp == std::string::npos || lp >= dropsEnd(ds)) continue;
      text.replace(lp, ol.size(), nl + u[1] + nl);
      fixedNames += std::string(u[0], strchr(u[0], '=') - u[0]) + " ";
    }
    size_t de = dropsEnd(ds), ec = text.find(nl + "effect_counts=", ds);
    if (ec == std::string::npos || ec >= de) {
      size_t at = text.find(nl + "# uses:", ds); if (at == std::string::npos || at >= de) at = text.find(nl + "uses=", ds);
      if (at != std::string::npos && at < de) {
        text.insert(at + nl.size(), "# effect_counts: odds of 1, 2 and 3 effects on a found gun (Common, Uncommon, Rare)" + nl + "effect_counts=35,45,20" + nl +
                    "# drawback_chance: odds of one drawback ON TOP of the effects; the good effects then roll in the top half of their range" + nl + "drawback_chance=0.15" + nl);
        fixedNames += "effect_counts drawback_chance ";
      }
    }
  }
  if (ds != std::string::npos && done < 3) {                           // the co-op switches, above debug=
    size_t de = dropsEnd(ds), cg = text.find(nl + "coop_guns=", ds);
    if (cg == std::string::npos || cg >= de) {
      size_t at = text.find(nl + "# debug=1", ds); if (at == std::string::npos || at >= de) at = de;
      std::string block, def = kDefaultDrops; size_t a = def.find("# Co-op ("), b = def.find("# debug=1", a);
      if (a != std::string::npos && b != std::string::npos) block = def.substr(a, b - a);
      block = WithNewlines(block, nl);
      text.insert(at + (at < text.size() ? nl.size() : 0), at < text.size() ? block : nl + block);
      fixedNames += "coop_guns coop_elites coop_visuals coop_helpers ";
    }
  }
  if (ds != std::string::npos && done < 5) {                           // worn-out guns give their tier's material
    std::string ol = nl + "# Crates. Scrapping a gun gives Scrap Iron (Common), Brass (Uncommon) or Gunmetal (Rare); a worn-out gun gives 1 Scrap Iron." + nl;
    size_t lp = text.find(ol, ds);
    if (lp != std::string::npos && lp < dropsEnd(ds)) {
      text.replace(lp, ol.size(), nl + "# Crates. Scrapping a gun gives Scrap Iron (Common), Brass (Uncommon) or Gunmetal (Rare, 2 for a Unique); a worn-out" + nl + "# gun gives the same." + nl);
      fixedNames += "worn-out note ";
    }
  }
  if (ds != std::string::npos && done < 6) {                           // loot grid 6x6 -> 8x8
    for (auto& u : { std::make_pair("loot_cols=6", "loot_cols=8"), std::make_pair("loot_rows=6", "loot_rows=8") }) {
      std::string ol = nl + u.first + nl; size_t lp = text.find(ol, ds);
      if (lp == std::string::npos || lp >= dropsEnd(ds)) continue;
      text.replace(lp, ol.size(), nl + u.second + nl);
      fixedNames += std::string(u.first, strchr(u.first, '=') - u.first) + " ";
    }
  }
  if (ds != std::string::npos && done < 8) {                           // the torpedo kill cam, above the co-op lines
    size_t de = dropsEnd(ds), kp = text.find(nl + "killcam=", ds);
    if (kp == std::string::npos || kp >= de) {
      size_t at = text.find(nl + "# Co-op (", ds); if (at == std::string::npos || at >= de) at = text.find(nl + "# debug=1", ds);
      if (at == std::string::npos || at >= de) at = de;
      std::string block, def = kDefaultDrops; size_t a = def.find("# killcam:"), b = def.find("# Co-op (", a);
      if (a != std::string::npos && b != std::string::npos) block = def.substr(a, b - a);
      block = WithNewlines(block, nl);
      text.insert(at + (at < text.size() ? nl.size() : 0), at < text.size() ? block : nl + block);
      fixedNames += "killcam killcam_cooldown killcam_slow chomp_volume crew_tracers crew_marker crew_aim_line ";
    }
  }
  if (ds != std::string::npos && done < 10) {                          // the crew weapon looks, above the co-op lines
    size_t de = dropsEnd(ds), cp = text.find(nl + "crew_tracers=", ds);
    if (cp == std::string::npos || cp >= de) {
      size_t at = text.find(nl + "# Co-op (", ds); if (at == std::string::npos || at >= de) at = text.find(nl + "# debug=1", ds);
      if (at == std::string::npos || at >= de) at = de;
      std::string block, def = kDefaultDrops; size_t a = def.find("# crew_tracers:"), b = def.find("# Co-op (", a);
      if (a != std::string::npos && b != std::string::npos) block = def.substr(a, b - a);
      block = WithNewlines(block, nl);
      text.insert(at + (at < text.size() ? nl.size() : 0), at < text.size() ? block : nl + block);
      fixedNames += "crew_tracers crew_marker crew_aim_line ";
    }
  }
  if (ds != std::string::npos && done < 11) {                          // fewer, better drops and the loot keys (0.9.1)
    for (auto& u : { std::make_pair("chance=1.0", "chance=0.4"), std::make_pair("effect_counts=35,45,20", "effect_counts=20,50,30") }) {
      std::string ol = nl + u.first + nl; size_t lp = text.find(ol, ds);   // only a line still at the old default
      if (lp == std::string::npos || lp >= dropsEnd(ds)) continue;
      text.replace(lp, ol.size(), nl + u.second + nl);
      fixedNames += std::string(u.first, strchr(u.first, '=') - u.first) + " ";
    }
    size_t de = dropsEnd(ds), tp = text.find(nl + "torpedo_share=", ds);
    if (tp == std::string::npos || tp >= de) {
      size_t at = text.find(nl + "# Crates.", ds); if (at == std::string::npos || at >= de) at = text.find(nl + "# killcam:", ds);
      if (at == std::string::npos || at >= de) at = text.find(nl + "# debug=1", ds);
      if (at == std::string::npos || at >= de) at = de;
      std::string block, def = kDefaultDrops; size_t a = def.find("# torpedo_share:"), b = def.find("# Crates.", a);
      if (a != std::string::npos && b != std::string::npos) block = def.substr(a, b - a);
      block = WithNewlines(block, nl);
      text.insert(at + (at < text.size() ? nl.size() : 0), at < text.size() ? block : nl + block);
      fixedNames += "torpedo_share auto_scrap ";
    }
    de = dropsEnd(ds); size_t cv = text.find(nl + "chomp_volume=", ds);   // the crew build's default had no chomp_volume line
    if (cv == std::string::npos || cv >= de) {
      size_t at = text.find(nl + "# crew_tracers:", ds); if (at == std::string::npos || at >= de) at = text.find(nl + "# Co-op (", ds);
      if (at == std::string::npos || at >= de) at = text.find(nl + "# debug=1", ds);
      if (at == std::string::npos || at >= de) at = de;
      std::string block, def = kDefaultDrops; size_t a = def.find("# chomp_volume:"), b = def.find("# crew_tracers:", a);
      if (a != std::string::npos && b != std::string::npos) block = def.substr(a, b - a);
      block = WithNewlines(block, nl);
      if (!block.empty()) { text.insert(at + (at < text.size() ? nl.size() : 0), at < text.size() ? block : nl + block); fixedNames += "chomp_volume "; }
    }
  }
  if (ds != std::string::npos && done < 12) {                          // paint schemes, under cosmetic_chance
    size_t de = dropsEnd(ds), pp = text.find(nl + "paint_chance=", ds);
    if (pp == std::string::npos || pp >= de) {
      size_t at = text.find(nl + "cosmetic_chance=", ds);
      if (at != std::string::npos && at < de) { at = text.find(nl, at + nl.size()); if (at == std::string::npos || at > de) at = de; }
      else { at = text.find(nl + "# effect_counts:", ds); if (at == std::string::npos || at >= de) at = text.find(nl + "# debug=1", ds); if (at == std::string::npos || at >= de) at = de; }
      std::string block, def = kDefaultDrops; size_t a = def.find("# paint_chance:"), b = def.find("# effect_counts:", a);
      if (a != std::string::npos && b != std::string::npos) block = def.substr(a, b - a);
      block = WithNewlines(block, nl);
      text.insert(at + (at < text.size() ? nl.size() : 0), at < text.size() ? block : nl + block);
      fixedNames += "paint_chance ";
    }
  }
  if (ds != std::string::npos && done < 14) {                          // the 0.9.2 keys: co-op torpedoes under coop_helpers=, effect_streaks, barrel_smoke
    size_t de = dropsEnd(ds), tp = text.find(nl + "coop_torpedoes=", ds);   // migration 3 copies the whole co-op block, this key too
    if (tp == std::string::npos || tp >= de) {
      size_t at = text.find(nl + "coop_helpers=", ds);
      if (at != std::string::npos && at < de) { at = text.find(nl, at + nl.size()); if (at == std::string::npos || at > de) at = de; }
      else { at = text.find(nl + "# debug=1", ds); if (at == std::string::npos || at >= de) at = de; }
      std::string block, def = kDefaultDrops; size_t a = def.find("# coop_torpedoes:"), b = def.find("# debug=1", a);
      if (a != std::string::npos && b != std::string::npos) block = def.substr(a, b - a);
      block = WithNewlines(block, nl);
      if (!block.empty()) { text.insert(at + (at < text.size() ? nl.size() : 0), at < text.size() ? block : nl + block); fixedNames += "coop_torpedoes "; }
    }
    de = dropsEnd(ds); size_t sp = text.find(nl + "effect_streaks=", ds);   // heal and chain streaks, above the co-op lines (files older
    if (sp == std::string::npos || sp >= de) {                         // than migration 9 or 10 already got them from those blocks)
      size_t at = text.find(nl + "# Co-op (", ds); if (at == std::string::npos || at >= de) at = text.find(nl + "# debug=1", ds);
      if (at == std::string::npos || at >= de) at = de;
      const char* last = "effect_streaks=1\n";                         // the block ends with its key's line
      std::string block, def = kDefaultDrops; size_t a = def.find("# effect_streaks:"), b = def.find(last, a);
      if (a != std::string::npos && b != std::string::npos) block = def.substr(a, b + strlen(last) - a);
      block = WithNewlines(block, nl);
      if (!block.empty()) { text.insert(at + (at < text.size() ? nl.size() : 0), at < text.size() ? block : nl + block); fixedNames += "effect_streaks "; }
    }
    de = dropsEnd(ds); size_t bp = text.find(nl + "barrel_smoke=", ds);    // smoking barrels, after effect_streaks (files older than
    if (bp == std::string::npos || bp >= de) {                         // migration 10 already got them from that block)
      size_t at = text.find(nl + "# Co-op (", ds); if (at == std::string::npos || at >= de) at = text.find(nl + "# debug=1", ds);
      if (at == std::string::npos || at >= de) at = de;
      const char* last = "barrel_smoke_seconds=4\n";                   // the block ends with its last key's line
      std::string block, def = kDefaultDrops; size_t a = def.find("# barrel_smoke:"), b = def.find(last, a);
      if (a != std::string::npos && b != std::string::npos) block = def.substr(a, b + strlen(last) - a);
      block = WithNewlines(block, nl);
      if (!block.empty()) { text.insert(at + (at < text.size() ? nl.size() : 0), at < text.size() ? block : nl + block); fixedNames += "barrel_smoke barrel_smoke_after barrel_smoke_seconds "; }
    }
  }
  if (ds != std::string::npos && done < 15) {                          // female crew (0.9.3-dev), above the co-op lines (migrations 8
    size_t de = dropsEnd(ds), fp = text.find(nl + "female_crew=", ds);  // and 10 copy ranges that now hold it, hence the check)
    if (fp == std::string::npos || fp >= de) {
      size_t at = text.find(nl + "# Co-op (", ds); if (at == std::string::npos || at >= de) at = text.find(nl + "# debug=1", ds);
      if (at == std::string::npos || at >= de) at = de;
      std::string block, def = kDefaultDrops; size_t a = def.find("# female_crew:");   // the block ends with its key's line, found
      size_t b = a == std::string::npos ? a : def.find("\nfemale_crew=", a);           // by the key, so a new default value keeps it
      size_t e = b == std::string::npos ? b : def.find('\n', b + 1);
      if (e != std::string::npos) block = def.substr(a, e + 1 - a);
      else Log("female crew: the female_crew block is missing from the default drops.ini; not added");
      block = WithNewlines(block, nl);
      if (!block.empty()) { text.insert(at + (at < text.size() ? nl.size() : 0), at < text.size() ? block : nl + block); fixedNames += "female_crew "; }
    }
  }
  if (ds != std::string::npos && done < 16) {                          // female crew phase 2: default 100 -> 25, only while the line is
    size_t de = dropsEnd(ds);                                          // still the old default (a value the player set stays); the old
    auto conv = [&](const std::string& b) { return WithNewlines(b, nl); };
    std::string oldBlock = conv("# female_crew: percent of crew members who are women (test): their own hair, no beard or pipe and a female figure that\n"
                                "# the clothes follow. 100 = all of them, 0 = stock figures and hair (beards and pipes\n"
                                "# taken from women stay off in the save). Picked from the name, so saves and co-op players with the mod agree\n"
                                "female_crew=100\n");
    size_t op = text.find(oldBlock, ds);                               // comment is replaced too while it is untouched
    std::string def = kDefaultDrops; size_t a = def.find("# female_crew:"), b = a == std::string::npos ? a : def.find("\nfemale_crew=", a);
    size_t e = b == std::string::npos ? b : def.find('\n', b + 1);
    if (op != std::string::npos && op < de && e != std::string::npos && (op == 0 || text[op - 1] == '\n')) {
      text.replace(op, oldBlock.size(), conv(def.substr(a, e + 1 - a))); fixedNames += "female_crew(25) ";
    } else {
      size_t lp = text.find(nl + "female_crew=100", ds), le = lp == std::string::npos ? lp : lp + nl.size() + 15;
      if (lp != std::string::npos && lp < de && (le == text.size() || text.compare(le, nl.size(), nl) == 0)) {
        text.replace(lp + nl.size(), 15, "female_crew=25"); fixedNames += "female_crew(25) ";
      }
    }
  }
  if (ds != std::string::npos && done < 17) {                          // no_females, right after the female_crew line
    size_t de = dropsEnd(ds), np = text.find(nl + "no_females=", ds);
    if (np == std::string::npos || np >= de) {
      size_t at = text.find(nl + "female_crew=", ds);
      if (at != std::string::npos && at < de) { at = text.find(nl, at + nl.size()); if (at == std::string::npos || at > de) at = de; }
      else { at = text.find(nl + "# Co-op (", ds); if (at == std::string::npos || at >= de) at = text.find(nl + "# debug=1", ds); if (at == std::string::npos || at >= de) at = de; }
      const char* last = "no_females=0\n";                             // the block ends with its key's line
      std::string block, def = kDefaultDrops; size_t a = def.find("# no_females"), b = a == std::string::npos ? a : def.find(last, a);
      if (a != std::string::npos && b != std::string::npos) block = def.substr(a, b + strlen(last) - a);
      else Log("female crew: the no_females block is missing from the default drops.ini; not added");
      block = WithNewlines(block, nl);
      if (!block.empty()) { text.insert(at + (at < text.size() ? nl.size() : 0), at < text.size() ? block : nl + block); fixedNames += "no_females "; }
    }
  }
  if (ds != std::string::npos && done < 18) {                          // female_crew help: genders are stored;
    size_t de = dropsEnd(ds);                                          // only the comment, only while untouched, the value stays
    auto conv = [&](const std::string& b) { return WithNewlines(b, nl); };
    std::string oldC = conv("# female_crew: percent of crew members who are women: their own hair, no beard or pipe and a female figure that the\n"
                            "# clothes follow. New women get women's first names. Crew you already have keep their names; the percent picks which of\n"
                            "# them are women. Enemy crews too. 0 = stock figures, hair and names (beards and pipes taken from women stay off in the\n"
                            "# save). Picked from the name, so saves agree; co-op players need the mod and the same percent\n");
    std::string def = kDefaultDrops; size_t a = def.find("# female_crew:"), b = a == std::string::npos ? a : def.find("\nfemale_crew=", a);
    size_t op = text.find(oldC, ds);
    if (op != std::string::npos && op < de && b != std::string::npos && (op == 0 || text[op - 1] == '\n')) {
      text.replace(op, oldC.size(), conv(def.substr(a, b + 1 - a))); fixedNames += "female_crew(help) ";
    }
  }
  if (ds != std::string::npos && done < 19) {                          // utility gear: smoke_seconds and raft_speed, above
    size_t de = dropsEnd(ds), sp = text.find(nl + "smoke_seconds=", ds);   // crew_tracers (migration 11's chomp_volume block holds them)
    if (sp == std::string::npos || sp >= de) {
      size_t at = text.find(nl + "# crew_tracers:", ds); if (at == std::string::npos || at >= de) at = text.find(nl + "# Co-op (", ds);
      if (at == std::string::npos || at >= de) at = text.find(nl + "# debug=1", ds);
      if (at == std::string::npos || at >= de) at = de;
      const char* last = "raft_speed=8\n";                             // the block ends with its last key's line
      std::string block, def = kDefaultDrops; size_t a = def.find("# smoke_seconds:"), b = a == std::string::npos ? a : def.find(last, a);
      if (a != std::string::npos && b != std::string::npos) block = def.substr(a, b + strlen(last) - a);
      block = WithNewlines(block, nl);
      if (!block.empty()) { text.insert(at + (at < text.size() ? nl.size() : 0), at < text.size() ? block : nl + block); fixedNames += "smoke_seconds raft_speed "; }
    }
  }
  if (ds != std::string::npos && done < 20) {                          // the talking crew: crew_talk and crew_chatter, above
    size_t de = dropsEnd(ds), sp = text.find(nl + "crew_talk=", ds);   // effect_streaks
    if (sp == std::string::npos || sp >= de) {
      size_t at = text.find(nl + "# effect_streaks:", ds); if (at == std::string::npos || at >= de) at = text.find(nl + "# Co-op (", ds);
      if (at == std::string::npos || at >= de) at = text.find(nl + "# debug=1", ds);
      if (at == std::string::npos || at >= de) at = de;
      const char* last = "crew_chatter=1\n";
      std::string block, def = kDefaultDrops; size_t a = def.find("# crew_talk:"), b = a == std::string::npos ? a : def.find(last, a);
      if (a != std::string::npos && b != std::string::npos) block = def.substr(a, b + strlen(last) - a);
      block = WithNewlines(block, nl);
      if (!block.empty()) { text.insert(at + (at < text.size() ? nl.size() : 0), at < text.size() ? block : nl + block); fixedNames += "crew_talk crew_chatter "; }
    }
  }
  if (ds != std::string::npos && done < 21) {                          // the newsreel switch, above the co-op block
    size_t de = dropsEnd(ds), sp = text.find(nl + "newsreel=", ds);
    if (sp == std::string::npos || sp >= de) {
      size_t at = text.find(nl + "# Co-op (", ds); if (at == std::string::npos || at >= de) at = text.find(nl + "# debug=1", ds);
      if (at == std::string::npos || at >= de) at = de;
      const char* last = "newsreel_keep=10\n";
      std::string block, def = kDefaultDrops; size_t a = def.find("# newsreel:"), b = a == std::string::npos ? a : def.find(last, a);
      if (a != std::string::npos && b != std::string::npos) block = def.substr(a, b + strlen(last) - a);
      block = WithNewlines(block, nl);
      if (!block.empty()) { text.insert(at + (at < text.size() ? nl.size() : 0), at < text.size() ? block : nl + block); fixedNames += "newsreel newsreel_keep "; }
    }
  }
  if (ds != std::string::npos && done < 22) {                          // mod_gun_share (0.9.6), under side_grades=
    size_t de = dropsEnd(ds), sp = text.find(nl + "mod_gun_share=", ds);
    if (sp == std::string::npos || sp >= de) {
      size_t at = text.find(nl + "side_grades=", ds);
      if (at != std::string::npos && at < de) { size_t le = text.find(nl, at + nl.size()); at = le == std::string::npos || le > de ? de : le; }
      else { at = text.find(nl + "# tracer_rarity:", ds); if (at == std::string::npos || at >= de) at = de; }
      const char* last = "mod_gun_share=0.25\n";
      std::string block, def = kDefaultDrops; size_t a = def.find("# mod_gun_share:"), b = a == std::string::npos ? a : def.find(last, a);
      if (a != std::string::npos && b != std::string::npos) block = def.substr(a, b + strlen(last) - a);
      block = WithNewlines(block, nl);
      if (!block.empty()) { text.insert(at + (at < text.size() ? nl.size() : 0), at < text.size() ? block : nl + block); fixedNames += "mod_gun_share "; }
    }
  }
  if (ds != std::string::npos && done < 23) {                          // crew_stories and crew_letters (0.9.8), under crew_chatter=
    size_t de = dropsEnd(ds), sp = text.find(nl + "crew_stories=", ds);
    if (sp == std::string::npos || sp >= de) {
      size_t at = text.find(nl + "crew_chatter=", ds);
      if (at != std::string::npos && at < de) { size_t le = text.find(nl, at + nl.size()); at = le == std::string::npos || le > de ? de : le; }
      else { at = text.find(nl + "# effect_streaks:", ds); if (at == std::string::npos || at >= de) at = de; }
      const char* last = "crew_letters=1\n";
      std::string block, def = kDefaultDrops; size_t a = def.find("# crew_stories:"), b = a == std::string::npos ? a : def.find(last, a);
      if (a != std::string::npos && b != std::string::npos) block = def.substr(a, b + strlen(last) - a);
      block = WithNewlines(block, nl);
      if (!block.empty()) { text.insert(at + (at < text.size() ? nl.size() : 0), at < text.size() ? block : nl + block); fixedNames += "crew_stories crew_letters "; }
    }
  }
  if (ds != std::string::npos && done < 24) {                          // balance pass: crate costs follow rarity, Uniques last 8
    std::string ol = nl + "crate_cost=5,3,2" + nl; size_t lp = text.find(ol, ds);   // only a line still at the old default
    if (lp != std::string::npos && lp < dropsEnd(ds)) { text.replace(lp, ol.size(), nl + "crate_cost=2,3,4" + nl); fixedNames += "crate_cost "; }
    int nu = 0;
    for (size_t up = text.find("[unique]" + nl); up != std::string::npos; up = text.find("[unique]" + nl, up + 8)) {
      size_t ue = text.find(nl + "[", up + 8); if (ue == std::string::npos) ue = text.size();
      size_t lp = text.find(nl + "uses=6" + nl, up);
      if (lp != std::string::npos && lp < ue) { text.replace(lp + nl.size(), 6, "uses=8"); nu++; }
    }
    if (nu) fixedNames += "uses(" + std::to_string(nu) + " uniques) ";
  }
  if (ds != std::string::npos && done < 25) {                          // kill_marks (0.9.10), under newsreel_keep=
    size_t de = dropsEnd(ds), sp = text.find(nl + "kill_marks=", ds);
    if (sp == std::string::npos || sp >= de) {
      size_t at = text.find(nl + "newsreel_keep=", ds);
      if (at != std::string::npos && at < de) { size_t le = text.find(nl, at + nl.size()); at = le == std::string::npos || le > de ? de : le; }
      else { at = text.find(nl + "# Co-op (", ds); if (at == std::string::npos || at >= de) at = de; }
      const char* last = "kill_marks=1\n";
      std::string block, def = kDefaultDrops; size_t a = def.find("# kill_marks:"), b = a == std::string::npos ? a : def.find(last, a);
      if (a != std::string::npos && b != std::string::npos) block = def.substr(a, b + strlen(last) - a);
      block = WithNewlines(block, nl);
      if (!block.empty()) { text.insert(at + (at < text.size() ? nl.size() : 0), at < text.size() ? block : nl + block); fixedNames += "kill_marks "; }
    }
  }
  if (ds != std::string::npos && done < kMigrations) {                 // record it, so these updates never run again
    std::string line = "migrated=" + std::to_string(kMigrations);
    size_t mp = text.find(nl + "migrated=", ds);
    if (mp != std::string::npos && mp < dropsEnd(ds)) { size_t le = text.find(nl, mp + nl.size()); text.replace(mp + nl.size(), (le == std::string::npos ? text.size() : le) - mp - nl.size(), line); }
    else text.insert(ds + 7 + (text.compare(ds + 7, nl.size(), nl) == 0 ? nl.size() : 0),
                     "# migrated: which of the mod's built-in updates this file has had (set by the mod; leave it)" + nl + line + nl);
    fixedNames += "migrated ";
  }
  if (add.empty() && fixedNames.empty()) return;
  if (!add.empty()) {
    std::string head = std::string("\n# Guns, torpedoes, effects and Uniques added by BoatCrewWeaponMod ") + kModVersion + " (new since this file was made). Set weight=0 to switch one off.\n";
    add = head + add;
    add = WithNewlines(add, nl);
    text += add;
  }
  f = fopen((path + ".tmp").c_str(), "wb");
  if (!f) return;
  fwrite(text.data(), 1, text.size(), f);
  FinishSave(f, path);
  if (!fixedNames.empty()) Log("drops.ini: updated built-in lines of %s", fixedNames.c_str());
  if (add.empty()) return;
  Log("drops.ini: added %s", names.c_str());
}

static void UniqueKey(UniqueDef& u, const std::string& k, const std::string& v) {
  if (k == "name") u.name = v; else if (k == "base") u.base = v; else if (k == "flavor") u.flavor = v; else if (k == "line") u.lines.push_back(v);
  else if (k == "weight") u.weight = atof(v.c_str()); else if (k == "uses") u.uses = atoi(v.c_str()); else if (k == "fragment_from") u.fragmentFrom = v;
  else if (k == "model") u.model = v;
  else if (k == "model_scale") { float sc = (float)atof(v.c_str()); if (sc > 0.01f && sc < 100.f) u.modelScale = sc; }
  else if (k == "model_material") u.modelOwn = (v == "own");
  else if (k == "model_keep") u.modelKeep = SplitList(v);
  else if (k == "sound_from") u.soundFrom = v;
  else if (k == "built_from") u.builtFrom = v;
  else { Op tmp; if (ParseOpKey(k, &tmp)) u.ops.push_back({ k, v }); else Log("drops.ini: unknown unique key '%s'", k.c_str()); }
}
extern const char* kUnusualDefs;
// The Unusual guns are built in (see kUnusualDefs), parsed like [unique] blocks.
static void LoadUnusual() {
  std::string all = kUnusualDefs; size_t at = 0; int n = 0;
  while (at < all.size()) {
    size_t e = all.find('\n', at); if (e == std::string::npos) e = all.size();
    std::string l = Trim(all.substr(at, e - at)); at = e + 1;
    if (l == "[unique]") { g_uniques.emplace_back(); g_uniques.back().unusual = true; n++; continue; }
    size_t eq = l.find('='); if (eq == std::string::npos || g_uniques.empty()) continue;
    UniqueKey(g_uniques.back(), Trim(l.substr(0, eq)), Trim(l.substr(eq + 1)));
  }
  DLog("extras: %d built-in, chance %.4f", n, g_unusualChance);
}
// Saved Unusual guns ('Mother Duck', 'Mother Duck 2', ...) wear their def's model, so a new or changed model reaches
// guns found before it existed. Runs after LoadItems and LoadDrops.
static void UnusualModels() {
  int n = 0;
  for (Item& it : g_items) {
    if (it.rarity != "Unusual") continue;
    for (const UniqueDef& u : g_uniques) {
      if (!u.unusual || (u.model.empty() && u.soundFrom.empty())) continue;
      bool match = it.name == u.name;
      if (!match && it.name.size() > u.name.size() + 1 && it.name.compare(0, u.name.size() + 1, u.name + " ") == 0)
        match = it.name.find_first_not_of("0123456789", u.name.size() + 1) == std::string::npos;
      if (!match) continue;
      if (!u.model.empty()) { it.model = u.model; it.modelScale = u.modelScale; it.modelOwnMaterial = u.modelOwn; it.modelKeep = u.modelKeep; }
      n++;
      if (!u.soundFrom.empty()) it.soundFrom = u.soundFrom;
      break;
    }
  }
  if (n) DLog("extra effects: %d saved guns wear their model", n);
}
// The torpedo kill cam's settings ([drops] killcam, killcam_cooldown, killcam_slow); the camera itself is KillCamTick.
static bool g_kcOn = true; static double g_kcCooldown = 60, g_kcSlow = 0.25;
static bool KillCamKey(const std::string& k, const std::string& v) {
  if (k == "killcam") g_kcOn = atoi(v.c_str()) != 0;
  else if (k == "killcam_cooldown") { g_kcCooldown = atof(v.c_str()); if (g_kcCooldown < 5) g_kcCooldown = 5; }
  else if (k == "killcam_slow") { g_kcSlow = atof(v.c_str()); if (g_kcSlow < 0.05) g_kcSlow = 0.05; if (g_kcSlow > 1) g_kcSlow = 1; }
  else return false;
  return true;
}
// Loot volume, so players need not sort and discard all the time: [drops] torpedo_share and auto_scrap, read at drop
// time. auto_scrap: -1 none, else the highest rarity scrapped (0 Common, 1 Uncommon, 2 Rare); Mod Options writes the
// number, a hand-edited file the name.
static double g_torpShare = 0.025; static int g_autoScrap = -1;
static double g_modGunShare = 0.25;                                  // drops.ini mod_gun_share (see RollItem)
static double g_paintChance = 0.35;                                  // [drops] paint_chance: share of found guns with a paint scheme (see kPaint)
static const char* kAutoScrapNames[4] = { "none", "Common", "Uncommon", "Rare" };
static bool LootKey(const std::string& k, const std::string& v) {
  if (k == "torpedo_share") { g_torpShare = atof(v.c_str()); if (g_torpShare < 0) g_torpShare = 0; if (g_torpShare > 1) g_torpShare = 1; }
  else if (k == "mod_gun_share") { g_modGunShare = atof(v.c_str()); if (!(g_modGunShare >= 0)) g_modGunShare = 0; if (g_modGunShare > 0.9) g_modGunShare = 0.9; }
  else if (k == "auto_scrap") {
    std::string l = v; for (char& ch : l) ch = (char)tolower((unsigned char)ch);
    if (!l.empty() && l[0] >= '0' && l[0] <= '9') { int x = atoi(l.c_str()); g_autoScrap = x <= 0 ? -1 : (x > 3 ? 2 : x - 1); }   // 0 none, 1-3 Common..Rare
    else g_autoScrap = l == "common" ? 0 : l == "uncommon" ? 1 : l == "rare" ? 2 : -1;
  }
  else if (k == "paint_chance") { g_paintChance = atof(v.c_str()); if (!(g_paintChance >= 0)) g_paintChance = 0; if (g_paintChance > 1) g_paintChance = 1; }
  else return false;
  return true;
}
// The crew weapon looks ([drops] crew_tracers, crew_marker, crew_aim_line), switched while playing; see CrewTick.
static bool g_crewTracers = true, g_crewMarker = true, g_crewLine = true;
static bool g_streaksOn = true;                                      // drops.ini effect_streaks: heal and chain streaks (see StreakTick)
// [drops] crew_talk (0 = the crew never speak), crew_chatter (how often: 0.25 .. 2); switched while playing.
static bool g_talkOn = true; static double g_talkChatter = 1;
// [drops] crew_stories (0 = the after-action report never opens) and crew_letters (0 = no letters home, no mail call,
// no talk of letters), for players who would rather not read about losing crew; switched while playing.
static bool g_storiesOn = true, g_lettersOn = true;
static bool g_newsreelOn = true;                                     // drops.ini newsreel: the battle film (newsreel.h), switched while playing
static int g_newsreelKeep = 10;                                      // drops.ini newsreel_keep: GIFs kept, oldest deleted (0 = all)
static bool g_killMarks = true;                                      // drops.ini kill_marks: 0 hides the hull tallies (wm_kill_marks.h), kills still count
static bool CrewVisKey(const std::string& k, const std::string& v) {
  if (k == "crew_talk") g_talkOn = atoi(v.c_str()) != 0;
  else if (k == "crew_stories") g_storiesOn = atoi(v.c_str()) != 0;
  else if (k == "crew_letters") g_lettersOn = atoi(v.c_str()) != 0;
  else if (k == "newsreel") g_newsreelOn = atoi(v.c_str()) != 0;
  else if (k == "kill_marks") g_killMarks = atoi(v.c_str()) != 0;
  else if (k == "newsreel_keep") { int x = atoi(v.c_str()); g_newsreelKeep = x < 0 ? 0 : x > 200 ? 200 : x; }
  else if (k == "crew_chatter") { double x = atof(v.c_str()); g_talkChatter = !(x >= 0.25) ? 0.25 : x > 2 ? 2.0 : x; }
  else if (k == "crew_tracers") g_crewTracers = atoi(v.c_str()) != 0;
  else if (k == "effect_streaks") g_streaksOn = atoi(v.c_str()) != 0;
  else if (k == "crew_marker") g_crewMarker = atoi(v.c_str()) != 0;
  else if (k == "crew_aim_line") g_crewLine = atoi(v.c_str()) != 0;
  else return false;
  return true;
}
// [drops] barrel_smoke, barrel_smoke_after, barrel_smoke_seconds: your guns smoke after a long burst (see SmokeTick).
// Only barrel_smoke=0 turns it off; the two times are clamped to 1..15 s.
static bool g_barrelSmoke = true; static double g_smokeAfter = 4, g_smokeSecs = 4;
static bool SmokeKey(const std::string& k, const std::string& v) {
  auto secs = [](const std::string& s) { double x = atof(s.c_str()); return !(x >= 1) ? 1.0 : (x > 15 ? 15.0 : x); };
  if (k == "barrel_smoke") g_barrelSmoke = atoi(v.c_str()) != 0;
  else if (k == "barrel_smoke_after") g_smokeAfter = secs(v);
  else if (k == "barrel_smoke_seconds") g_smokeSecs = secs(v);
  else return false;
  return true;
}
// [drops] female_crew: percent of crew members who are women, 0..100 (see the female crew section). 0 leaves every crew
// member stock; at launch it also leaves CrewMemberVisual.combineMesh unhooked (the stored-gender save key is still
// hooked, so flags saved earlier survive a save at 0). A crew member whose gender is already stored keeps it when this
// changes; only new crew and crew the mod has never seen follow the new share.
// no_females=1 (Mod Options "I don't want females!"): every crew member is stock, as at female_crew 0,
// while female_crew keeps its value for when it is switched off again. Stored genders are kept (and saved), only ignored.
static int g_femalePct = 25;
static bool g_noFemales = false;
static int FemPct() { return g_noFemales ? 0 : g_femalePct; }         // the share in use: what every female crew rule reads
static bool FemaleKey(const std::string& k, const std::string& v) {
  if (k == "no_females") { g_noFemales = atoi(v.c_str()) != 0; return true; }
  if (k != "female_crew") return false;
  int x = atoi(v.c_str()); g_femalePct = x < 0 ? 0 : (x > 100 ? 100 : x);
  return true;
}
static void LoadDrops() {
  std::string path = g_folder + "\\drops.ini";
  MigrateDrops(path);
  FILE* f = fopen(path.c_str(), "r");
  if (!f) {
    f = fopen(path.c_str(), "w");
    if (f) { fputs(kDefaultDrops, f); fclose(f); }
    Log("drops.ini did not exist; wrote the default one");
    f = fopen(path.c_str(), "r");
    if (!f) return;
  }
  char line[1024]; int sec = 0;
  while (fgets(line, sizeof line, f)) {
    std::string l = Trim(line);
    if (l.empty() || l[0] == '#' || l[0] == ';') continue;
    if (l[0] == '[') {
      if (l == "[drops]") sec = 1;
      else if (l == "[template]") { g_templates.emplace_back(); sec = 2; }
      else if (l == "[affix]") { g_affixes.emplace_back(); sec = 3; }
      else if (l == "[unique]") { g_uniques.emplace_back(); sec = 4; }
      else sec = 0;
      continue;
    }
    size_t eq = l.find('=');
    if (eq == std::string::npos) continue;
    std::string k = Trim(l.substr(0, eq)), v = Trim(l.substr(eq + 1));
    if (sec == 1 && KillCamKey(k, v)) continue;
    if (sec == 1 && CrewVisKey(k, v)) continue;
    if (sec == 1 && SmokeKey(k, v)) continue;
    if (sec == 1 && FemaleKey(k, v)) continue;
    if (sec == 1 && LootKey(k, v)) continue;
    if (sec == 1) { if (k == "chance") { double c = atof(v.c_str()); g_dropChance = c < 0 ? 0 : (c > 1 ? 1 : c); } else if (k == "message") g_dropMessage = atoi(v.c_str()) != 0; else if (k == "inject_live") g_injectLive = atoi(v.c_str()) != 0; else if (k == "uses") g_usesBase = atoi(v.c_str()); else if (k == "effect_scale") g_effectScale = atof(v.c_str()); else if (k == "size_per_effect") g_sizePerEffect = atof(v.c_str()); else if (k == "color_text") g_colorText = atoi(v.c_str()) != 0; else if (k == "burst_seconds") g_burstMs = (int)(atof(v.c_str()) * 1000.0); else if (k == "grid_cols") { int c = atoi(v.c_str()); if (c >= 2 && c <= 30) g_gridCols = c; } else if (k == "grid_rows") { int r = atoi(v.c_str()); if (r >= 2 && r <= 20) g_gridRows = r; } else if (k == "crate_chance") g_crateChance = atof(v.c_str()); else if (k == "crate_bonus_uses") g_crateBonusUses = atoi(v.c_str()); else if (k == "crate_cost") sscanf(v.c_str(), "%d , %d , %d", &g_crateCost[0], &g_crateCost[1], &g_crateCost[2]); else if (k == "crate_weights") sscanf(v.c_str(), "%lf , %lf , %lf", &g_crateWeight[0], &g_crateWeight[1], &g_crateWeight[2]); else if (k == "cosmetic_chance") g_cosmeticChance = atof(v.c_str()); else if (k == "price_scale") { double c = atof(v.c_str()); g_priceScale = c < 0 ? 0 : c; } else if (k == "turret_guns") g_turretGuns = atoi(v.c_str()) != 0; else if (k == "side_grades") g_sideGrades = atoi(v.c_str()) != 0; else if (k == "unique_chance") g_uniqueChance = atof(v.c_str()); else if (k == "unusual_chance") g_unusualChance = atof(v.c_str()); else if (k == "unique_crate_chance") sscanf(v.c_str(), "%lf , %lf , %lf", &g_uniqueCrate[0], &g_uniqueCrate[1], &g_uniqueCrate[2]); else if (k == "effect_counts") sscanf(v.c_str(), "%lf , %lf , %lf", &g_effectCounts[0], &g_effectCounts[1], &g_effectCounts[2]); else if (k == "drawback_chance") g_drawbackChance = atof(v.c_str()); else if (k == "tracer_rarity") g_tracerRarity = atoi(v.c_str()) != 0; else if (k == "modded_banner") g_bannerOn = atoi(v.c_str()) != 0; else if (k == "modded_banner_place") sscanf(v.c_str(), "%lf , %lf , %lf , %lf", &g_bannerPlace[0], &g_bannerPlace[1], &g_bannerPlace[2], &g_bannerPlace[3]);else if (k == "ui_scale") { float u = (float)atof(v.c_str()); g_uiScaleSet = u >= 0.5f && u <= 4.0f ? u : 0.0f; } else if (k == "loot_cols") { int c = atoi(v.c_str()); if (c >= 4 && c <= 30) g_lootCols = c; } else if (k == "loot_rows") { int r = atoi(v.c_str()); if (r >= 2 && r <= 20) g_lootRows = r; } else if (k == "coop_guns") g_coopGuns = atoi(v.c_str()) != 0; else if (k == "coop_elites") g_coopElites = atoi(v.c_str()) != 0; else if (k == "coop_visuals") g_coopVisuals = atoi(v.c_str()) != 0; else if (k == "coop_helpers") g_coopHelpers = atoi(v.c_str()) != 0; else if (k == "coop_torpedoes") g_coopTorps = atoi(v.c_str()) != 0; else if (k == "smoke_seconds") { double c = atof(v.c_str()); g_smokeSeconds = c < 10 ? 10 : (c > 600 ? 600 : c); } else if (k == "raft_speed") { double c = atof(v.c_str()); g_raftSpeed = c < 1 ? 1 : (c > 60 ? 60 : c); } else if (k == "chomp_volume") { double c = atof(v.c_str()); g_chompVolume = c < 0 ? 0 : (c > 2 ? 2 : c); } }
    else if (sec == 2) {
      DropTemplate& t = g_templates.back();
      if (k == "label") t.label = v; else if (k == "family") t.family = v; else if (k == "base") t.base = v; else if (k == "weight") t.weight = atof(v.c_str()); else if (k == "built_from") t.builtFrom = v;
    } else if (sec == 3) {
      Affix& a = g_affixes.back();
      if (k == "name") a.name = v;
      else if (k == "kind") { a.bad = (v == "bad"); a.cosmetic = (v == "cosmetic"); }
      else if (k == "roll") { double lo = 1, hi = 1; if (sscanf(v.c_str(), "%lf - %lf", &lo, &hi) == 2 || sscanf(v.c_str(), "%lf-%lf", &lo, &hi) == 2) { a.lo = lo; a.hi = hi; } else Log("drops.ini: roll '%s' is not lo-hi", v.c_str()); }
      else if (k == "weight") a.weight = atof(v.c_str());
      else if (k == "text") a.text = v;
      else if (k == "position") a.suffix = (v == "suffix");
      else if (k == "title") a.title = v;
      else if (k == "min_seats") a.minSeats = atoi(v.c_str());
      else if (k == "bases") { size_t st = 0; while (st <= v.size()) { size_t c = v.find(',', st); if (c == std::string::npos) c = v.size(); std::string nm = Trim(v.substr(st, c - st)); if (!nm.empty()) a.onlyBases.push_back(nm); st = c + 1; } }
      else if (k == "not_bases") { size_t st = 0; while (st <= v.size()) { size_t c = v.find(',', st); if (c == std::string::npos) c = v.size(); std::string nm = Trim(v.substr(st, c - st)); if (!nm.empty()) a.notBases.push_back(nm); st = c + 1; } }
      else if (k == "not_built_from") { size_t st = 0; while (st <= v.size()) { size_t c = v.find(',', st); if (c == std::string::npos) c = v.size(); std::string nm = Trim(v.substr(st, c - st)); if (!nm.empty()) a.notBuiltFrom.push_back(nm); st = c + 1; } }
      else if (k == "fragment_from") a.fragmentFrom = v;
      else if (k == "no_scale") a.noScale = atoi(v.c_str()) != 0;
      else if (k == "not_families") { size_t st = 0; while (st <= v.size()) { size_t c = v.find(',', st); if (c == std::string::npos) c = v.size(); std::string nm = Trim(v.substr(st, c - st)); if (!nm.empty()) a.notFamilies.push_back(nm); st = c + 1; } }
      else if (k == "family_weight") {
        for (const std::string& p : SplitList(v)) {
          size_t c = p.rfind(':'); double m = c == std::string::npos ? -1 : atof(p.c_str() + c + 1);
          if (m >= 0) a.familyWeights.push_back({ Trim(p.substr(0, c)), m }); else Log("drops.ini: family_weight '%s' is not Family:number", p.c_str());
        }
      }
      else if (k == "slot") a.slot = v == "torpedo" ? 1 : (v == "any" ? 2 : 0);
      else if (k == "excludes") { size_t st = 0; while (st <= v.size()) { size_t c = v.find(',', st); if (c == std::string::npos) c = v.size(); std::string nm = Trim(v.substr(st, c - st)); if (!nm.empty()) a.excludes.push_back(nm); st = c + 1; } }
      else { Op tmp; if (ParseOpKey(k, &tmp)) a.ops.push_back({ k, v }); else Log("drops.ini: unknown affix key '%s'", k.c_str()); }
    } else if (sec == 4) UniqueKey(g_uniques.back(), k, v);
  }
  fclose(f);
  LoadUnusual();
  for (size_t i = 0; i < g_uniques.size(); ) { if (g_uniques[i].name.empty() || (g_uniques[i].base.empty() && g_uniques[i].builtFrom.empty()) || g_uniques[i].ops.empty()) g_uniques.erase(g_uniques.begin() + i); else i++; }
  for (Affix& a : g_affixes)
    for (auto& r : kRetiredAffixes)
      if (a.name == r[0] && a.text == r[1] && a.weight > 0) { a.weight = 0; Log("drops.ini: '%s' is no longer a random effect (edit its text to keep it)", a.name.c_str()); }
  for (size_t i = 0; i < g_templates.size(); ) { if (g_templates[i].label.empty() || g_templates[i].family.empty() || g_templates[i].base.empty()) g_templates.erase(g_templates.begin() + i); else i++; }
  for (size_t i = 0; i < g_affixes.size(); ) { if (g_affixes[i].name.empty() || g_affixes[i].ops.empty()) g_affixes.erase(g_affixes.begin() + i); else i++; }
  ClampCrateSettings();
  Log("drops.ini: chance %.2f, %d templates, %d affixes, %d uniques (chance %.3f, crates %.2f/%.2f/%.2f), torpedo share %.3f, mod gun share %.2f, auto_scrap %s", g_dropChance, (int)g_templates.size(), (int)g_affixes.size(),
      (int)std::count_if(g_uniques.begin(), g_uniques.end(), [](const UniqueDef& u) { return !u.unusual; }), g_uniqueChance, g_uniqueCrate[0], g_uniqueCrate[1], g_uniqueCrate[2],
      g_torpShare, g_modGunShare, kAutoScrapNames[g_autoScrap + 1]);
}

static std::mt19937& Rng() { static std::mt19937 r{ std::random_device{}() ^ (unsigned)GetTickCount() }; return r; }
static double Rand01() { return std::uniform_real_distribution<double>(0.0, 1.0)(Rng()); }
static double EvalExpr(const std::string& e, double r) {
  if (e == "$r") return r;
  if (e == "1/$r") return r > 0 ? 1.0 / r : 1.0;
  if (e == "2-$r") return (std::max)(0.1, 2.0 - r);                   // the first Wild (0.9.3-dev): kept for files that still have it
  if (e == "2.5*$r-1.5") return (std::max)(1.0, 2.5 * r - 1.5);      // Wild: x1.5-2.0 spread for +20-40% damage
  return atof(e.c_str());
}
static std::string FormatText(std::string t, double r) {
  char b[16];
  snprintf(b, sizeof b, "%+d", (int)std::lround((r - 1.0) * 100.0));
  size_t p; while ((p = t.find("{pct}")) != std::string::npos) t.replace(p, 5, b);
  snprintf(b, sizeof b, "%d", (int)std::lround(std::fabs(r - 1.0) * 100.0));
  while ((p = t.find("{abs}")) != std::string::npos) t.replace(p, 5, b);
  snprintf(b, sizeof b, "%d", (int)std::lround((std::max)(0.0, 2.5 * r - 2.5) * 100.0));   // Wild's spread: 2.5*$r-1.5 as a percent
  while ((p = t.find("{spread}")) != std::string::npos) t.replace(p, 8, b);
  snprintf(b, sizeof b, "%d", (int)std::lround(r * 100.0));
  while ((p = t.find("{val}")) != std::string::npos) t.replace(p, 5, b);
  snprintf(b, sizeof b, "%d", (int)std::lround(r));
  while ((p = t.find("{int}")) != std::string::npos) t.replace(p, 5, b);
  return t;
}

// Gunner seats of a stock gun, from any live mount of it (-1 if no boat has one right now).
static void* LiveRegister();
static std::map<std::string, int> g_baseSeats;
static int SeatsOfBase(const std::string& base) {
  auto f = g_baseSeats.find(base);
  if (f != g_baseSeats.end()) return f->second;
  void* reg = LiveRegister();
  for (int i = 0; i < ListCount(reg); i++) {
    void* ws = ListAt(reg, i); if (!ws) continue;
    void* mounts = *(void**)((char*)ws + g_offMounts);
    for (int j = 0; j < ListCount(mounts); j++) {
      void* m = ListAt(mounts, j); void* gt = m ? *(void**)((char*)m + g_offGunType) : nullptr;
      if (gt && NameOf(gt) == base) { int n = SeatCount(m); g_baseSeats[base] = n; return n; }
    }
  }
  return -1;
}

// Effect text is colored per comma part: a good effect can carry a drawback ('fires on its own, -20% accuracy'), and a
// part that starts with '-' or 'but ' is shown as the drawback it is. Inside a drawback, a part that starts with '+' is
// the upside of the trade and shown as good (Wild: '+30% damage, 30% less accurate').
static std::string ColorEffectText(const std::string& text, bool bad) {
  std::string out; size_t st = 0;
  while (st <= text.size()) {
    size_t c = text.find(", ", st); if (c == std::string::npos) c = text.size();
    std::string part = text.substr(st, c - st);
    bool partBad = (bad && !(!part.empty() && part[0] == '+')) || (!part.empty() && part[0] == '-') || part.compare(0, 4, "but ") == 0 || part.compare(0, 7, "WARNING") == 0;
    if (!out.empty()) out += ", ";
    out += Colored(part, partBad ? kBadHex : kGoodHex);
    st = c + 2;
  }
  return out;
}

// Build one random item. rarity gets Common, Uncommon or Rare (1, 2 or 3 affixes).
static std::vector<std::string> g_rollParts;                         // the last roll's effect texts (colored), one per effect
static std::string g_rollLabel;
static std::vector<std::pair<std::string, bool>> g_rollAffixes;     // the last roll's effect names and whether each is a drawback                                      // the last roll's base gun label ('Dual Bofors')
// minAffixes: 2 for at least Uncommon, 3 for Rare (crates); 0 rolls normally.
static std::vector<std::string> BoatFamilies();
// A purged gun's name stays in positions.ini as a tombstone (its saved position), so a new gun must not take it: it would
// inherit old loadouts and, kept in another family, show in two slot lists.
static bool IsTombstone(const std::string& name);
static bool RollItem(Item* out, std::string* rarity, int minAffixes = 0, bool gunsOnly = false) {
  if (g_templates.empty() || g_affixes.empty()) return false;
  // only guns for slot families the current boat has (so the 5-inch stays a late-game find, appearing once you sail a
  // boat with a Heavy+ slot); if the boat cannot be read, every template is allowed
  std::vector<std::string> fams = BoatFamilies();
  auto allowed = [&](const DropTemplate& c) {
    if (!c.builtFrom.empty()) {                                        // turret_guns=0 / side_grades=0: none of them in the loot either
      const Item* b = FindItem(c.builtFrom);
      if (b && b->builtIn && b->retired) return false;
    }
    if (IsTorpFamily(c.family) && (g_torpOff || gunsOnly)) return false;
    return fams.empty() || std::find(fams.begin(), fams.end(), c.family) != fams.end();
  };
  // mod_gun_share: the mod's own guns (built_from templates) of a family together get that share of the family's drops,
  // split by their weight=, so each new side-grade adds variety, not more mod guns. Computed per roll
  // over the allowed templates: side_grades=0 or a boat without the family leaves the rest as they are.
  std::map<std::string, std::pair<double, double>> famW;              // family -> (stock weight, mod weight), allowed only
  auto tplWeight = [&](const DropTemplate& c) {
    if (c.builtFrom.empty() || g_modGunShare <= 0) return c.weight;
    auto it = famW.find(c.family);
    if (it == famW.end() || it->second.first <= 0 || it->second.second <= 0) return c.weight;
    return c.weight / it->second.second * it->second.first * g_modGunShare / (1 - g_modGunShare);
  };
  auto sumFams = [&]() { famW.clear(); for (auto& c : g_templates) if (allowed(c)) (c.builtFrom.empty() ? famW[c.family].first : famW[c.family].second) += c.weight; };
  // torpedo_share: a fixed share of drops is a torpedo when the boat has torpedo slots (the templates' weights only pick
  // which one), so the torpedo count does not grow with the number of torpedo templates (it was about 1 in 7 on a PT)
  double twGun = 0, twTorp = 0;
  sumFams();
  for (auto& c : g_templates) if (allowed(c)) (IsTorpFamily(c.family) ? twTorp : twGun) += tplWeight(c);
  if (twGun + twTorp <= 0) {                                           // no template fits this boat: fall back to every gun
    fams.clear();
    sumFams();
    for (auto& c : g_templates) if (allowed(c) && !IsTorpFamily(c.family)) twGun += tplWeight(c);
  }
  int pool = twTorp <= 0 || g_torpShare <= 0 ? 0 : (twGun > 0 ? (Rand01() < g_torpShare ? 1 : 0) : 1);   // 1: a torpedo
  auto inPool = [&](const DropTemplate& c) { return allowed(c) && IsTorpFamily(c.family) == (pool == 1); };
  double tw = pool ? twTorp : twGun;
  double x = Rand01() * tw; const DropTemplate* t = nullptr;
  for (auto& c : g_templates) { if (!inPool(c)) continue; t = &c; x -= tplWeight(c); if (x <= 0) break; }
  if (!t) return false;
  double cs = g_effectCounts[0] + g_effectCounts[1] + g_effectCounts[2], rr = Rand01() * cs;
  int n = cs <= 0 || rr < g_effectCounts[0] ? 1 : (rr < g_effectCounts[0] + g_effectCounts[1] ? 2 : 3);
  g_rollParts.clear(); g_rollAffixes.clear(); g_rollLabel = t->label;
  if (minAffixes >= 3) n = 3; else if (minAffixes == 2) n = Rand01() < 0.7 ? 2 : 3;
  std::vector<const Affix*> chosen;
  auto used = [&](const Affix* a) { return std::find(chosen.begin(), chosen.end(), a) != chosen.end(); };
  // the stat an edit touches, e.g. "roundsPerMinute" or "proj.BaseDamage" (mul and set count as the same stat)
  auto statOf = [](const std::string& key) {
    Op o; ParseOpKey(key, &o);
    std::string f = o.field;
    if (f == "MuzzleVelocityRealistic" || f == "timeScale") f = "MuzzleVelocity";   // all shell speed: Fast and Homing never meet
    if (f == "Inaccuracy") f = "Accuracy";                             // the spread is the Accuracy stat
    return std::string(OpPrefix(o)) + f;
  };
  // an effect may not touch a stat another chosen effect already touches (no '+132% damage, -47% damage')
  auto excl = [](const Affix* a, const Affix* b) { return std::find(a->excludes.begin(), a->excludes.end(), b->name) != a->excludes.end(); };
  auto clashes = [&](const Affix* a) {
    for (const Affix* c : chosen) if (excl(a, c) || excl(c, a)) return true;   // drops.ini excludes=, either direction
    for (const Affix* c : chosen)
      for (const AffixOp& x : a->ops) for (const AffixOp& y : c->ops) if (statOf(x.key) == statOf(y.key)) return true;
    return false;
  };
  auto fits = [&](const Affix* a) {
    if (a->slot != 2 && (a->slot == 1) != IsTorpFamily(t->family)) return false;   // gun effects on guns, torpedo effects on torpedoes
    if (std::find(a->notFamilies.begin(), a->notFamilies.end(), t->family) != a->notFamilies.end()) return false;   // drops.ini not_families=
    if (!a->onlyBases.empty() && std::none_of(a->onlyBases.begin(), a->onlyBases.end(), [&](const std::string& b) { return t->base.find(b) != std::string::npos; })) return false;
    if (std::any_of(a->notBases.begin(), a->notBases.end(), [&](const std::string& b) { return t->base.find(b) != std::string::npos; })) return false;
    if (!t->builtFrom.empty() && std::any_of(a->notBuiltFrom.begin(), a->notBuiltFrom.end(), [&](const std::string& b) { return t->builtFrom.find(b) != std::string::npos; })) return false;
    if (!g_elitesOn && std::any_of(a->ops.begin(), a->ops.end(), [](const AffixOp& o) { return o.key.find("giantSlayer") != std::string::npos; })) {
      static bool said = false;                                        // Giant Slayer: nothing to hit with Rare/Elite enemies off
      if (!said) { said = true; Log("drops: '%s' does not roll: Rare/Elite enemies are off (enemies.ini)", a->name.c_str()); }
      return false;
    }
    return a->minSeats <= 0 || SeatsOfBase(t->base) >= a->minSeats;   // e.g. one fewer gunner needs 2+
  };   // e.g. one fewer gunner needs 2+
  auto weightOf = [&](const Affix& a) {                                // family_weight= scales it on this slot family
    double w = a.weight;
    for (const auto& fw : a.familyWeights) if (fw.first == t->family) w *= fw.second;
    return w;
  };
  auto pick = [&](bool bad) -> const Affix* {
    double w = 0; for (auto& a : g_affixes) if (!a.cosmetic && a.bad == bad && !used(&a) && !clashes(&a) && fits(&a)) w += weightOf(a);
    if (w <= 0) return nullptr;
    double y = Rand01() * w;
    for (auto& a : g_affixes) if (!a.cosmetic && a.bad == bad && !used(&a) && !clashes(&a) && fits(&a)) { y -= weightOf(a); if (y <= 0) return &a; }
    return nullptr;
  };
  // Every effect slot is a good effect. A drawback comes on top (drawback_chance), never instead of one, and then the
  // good effects roll in the top half of their ranges: a trade, not a punishment. Rarity counts good effects only.
  for (int i = 0; i < n; i++) { const Affix* a = pick(false); if (!a) break; chosen.push_back(a); }
  if (chosen.empty()) return false;
  int goods = (int)chosen.size();
  bool tradeoff = g_drawbackChance > 0 && Rand01() < g_drawbackChance;
  if (tradeoff) { const Affix* b = pick(true); if (b) chosen.push_back(b); else tradeoff = false; }
  *rarity = goods == 1 ? "Common" : (goods == 2 ? "Uncommon" : "Rare");
  Item it; it.family = t->family; it.base = t->base; it.price = 0; it.builtFrom = t->builtFrom;
  it.fromInventory = true; it.rarity = *rarity; it.uses = g_usesBase > 0 ? g_usesBase + goods - 1 : 0;
  it.scale = 1.0f + (float)(g_sizePerEffect * (goods - 1));                         // more effects, bigger gun
  std::string desc, lead, tail;
  for (const Affix* a : chosen) {
    double u = Rand01(); if (tradeoff && !a->bad) u = 0.5 + 0.5 * u;  // a gun with a drawback gets the stronger half
    double r = a->lo + (a->hi - a->lo) * u;
    bool fxOnly = true; for (const AffixOp& o : a->ops) if (o.key.compare(0, 3, "fx.") != 0) fxOnly = false;
    if (g_effectScale != 1.0 && !fxOnly && !a->noScale) { r = 1.0 + (r - 1.0) * g_effectScale; if (r < 0.1) r = 0.1; }   // testing: exaggerate multipliers
    for (const AffixOp& o : a->ops) {
      Op op; ParseOpKey(o.key, &op);
      op.value = EvalExpr(o.expr, r);
      it.ops.push_back(op);
    }
    if (!desc.empty()) desc += ", ";
    g_rollParts.push_back(ColorEffectText(FormatText(a->text, r), a->bad));
    g_rollAffixes.push_back({ a->name, a->bad });
    if (!a->fragmentFrom.empty()) it.fragmentFrom = a->fragmentFrom;
    desc += g_rollParts.back();
    std::string word = a->title.empty() ? a->name : a->title;
    if (!a->bad) { if (a->suffix) { if (tail.empty()) tail = word; } else if (lead.empty()) lead = word; }
  }
  { double uc = ItemFx(it, "usesCut");                                // of Cheapness: a third fewer missions
    if (uc > 0 && uc < 1 && it.uses > 0) { it.uses = (int)std::lround(it.uses * (1 - uc)); if (it.uses < 1) it.uses = 1; } }
  auto compose = [&](bool withTail) {
    std::string n = lead.empty() ? t->label : lead + " " + t->label;
    if (withTail && !tail.empty()) n += " " + tail;
    return n;
  };
  std::string baseName = compose(true);
  if (baseName.size() > 34 && !lead.empty() && !tail.empty()) baseName = compose(false);   // the list shrinks long names, but not endlessly
  std::string name = baseName;
  for (int k = 2; FindItem(name) || IsTombstone(name); k++) name = baseName + " " + std::to_string(k);   // a purged gun's name keeps its position
  it.name = name;
  // one cosmetic extra now and then: looks only, so it neither counts toward rarity nor names the gun
  std::string look; int lookPart = -1;                                 // the 'Look:' text, and its line in g_rollParts (the crate reel)
  if (g_cosmeticChance > 0 && Rand01() < g_cosmeticChance) {
    double cw = 0; for (auto& a : g_affixes) if (a.cosmetic && fits(&a) && !clashes(&a)) cw += a.weight;   // Long Tracers would stack on Bold
    double y = Rand01() * cw; const Affix* cz = nullptr;
    if (cw > 0) for (auto& a : g_affixes) if (a.cosmetic && fits(&a) && !clashes(&a)) { cz = &a; y -= a.weight; if (y <= 0) break; }
    if (cz) {
      double r = cz->lo + (cz->hi - cz->lo) * Rand01();
      for (const AffixOp& o : cz->ops) { Op op; ParseOpKey(o.key, &op); op.value = EvalExpr(o.expr, r); it.ops.push_back(op); }
      look = FormatText(cz->text, r);
      lookPart = (int)g_rollParts.size(); g_rollParts.push_back(""); g_rollAffixes.push_back({ cz->name, false });
    }
  }
  // a paint scheme now and then (paint_chance), on top of the cosmetic and on the same Look: line. Not on torpedoes:
  // TorpedoColorer recolours their bodies from the boat's torpedo colours. Gold and pink come from rarity, not from here.
  if (g_paintChance > 0 && !IsTorpFamily(t->family) && Rand01() < g_paintChance) {
    std::vector<int> pool; for (int k = 1; k < kPaintN; k++) if (kPaint[k].rolled) pool.push_back(k);
    if (!pool.empty()) {
      int pk = pool[(size_t)(Rand01() * pool.size()) % pool.size()];
      Op op; ParseOpKey("fx.set.paint", &op); op.value = pk; it.ops.push_back(op);
      look += (look.empty() ? "" : ", ") + std::string(kPaint[pk].name) + " paint";
      if (lookPart < 0) { lookPart = (int)g_rollParts.size(); g_rollParts.push_back(""); }
    }
  }
  if (!look.empty()) { look = Colored("Look: " + look, kFunHex); g_rollParts[lookPart] = look; }
  it.description = Colored(*rarity, RarityHex(*rarity)) + ": " + desc + "." + (look.empty() ? "" : " " + look + ".");
  *out = it;
  return true;
}

// One of the [unique] guns, on a template with its base gun whose slot family this boat has. False if none fits, and
// the caller rolls a normal gun instead. Fills g_rollParts / g_rollAffixes / g_rollLabel like RollItem (the crate reel).
// A Unique's description after 'Unique <label>: ': its lines, then the flavour quote. parts (the crate reel's lines) is
// optional.
static std::string UniqueText(const UniqueDef& u, const char* word, std::vector<std::string>* parts) {
  std::string desc;
  for (const std::string& l : u.lines) { std::string c = ColorEffectText(l, false); if (parts) parts->push_back(c); desc += (desc.empty() ? "" : ", ") + c; }
  std::string flavor = u.flavor.empty() ? "" : Colored("<i>\"" + u.flavor + "\"</i>", RarityHex(word));
  if (parts && !flavor.empty()) parts->push_back(flavor);
  return desc + "." + (flavor.empty() ? "" : " " + flavor);
}
// A Unique's base= may name several stock bases, comma separated (the chompers take the sub chaser's Mk 24 pair as well
// as Mk 14 / Mk 18, which it cannot carry).
static bool BaseListed(const std::string& list, const std::string& base) {
  for (const std::string& b : SplitList(list)) if (b == base) return true;
  return false;
}
static bool RollUnique(Item* out, std::string* rarity, bool unusual = false) {
  if (g_uniques.empty() || g_templates.empty()) return false;
  std::vector<std::string> fams = BoatFamilies();
  auto templatesFor = [&](const UniqueDef& u) {
    std::vector<const DropTemplate*> r;
    // a stock-gun Unique takes only the stock templates of its base (the mod's guns reuse stock bases: the Pom-pom's is
    // the Bofors); one with built_from takes that gun's templates, unless turret_guns=0 / side_grades=0 hid it
    if (!u.builtFrom.empty()) { const Item* b = FindItem(u.builtFrom); if (!b || !b->builtIn || b->retired) return r; }
    for (auto& t : g_templates) if ((u.builtFrom.empty() ? BaseListed(u.base, t.base) && t.builtFrom.empty() : t.builtFrom == u.builtFrom) && !(g_torpOff && IsTorpFamily(t.family)) && (fams.empty() || std::find(fams.begin(), fams.end(), t.family) != fams.end())) r.push_back(&t);
    return r;
  };
  double tw = 0; for (auto& u : g_uniques) if (u.unusual == unusual && u.weight > 0 && !templatesFor(u).empty()) tw += u.weight;
  if (tw <= 0) return false;
  double x = Rand01() * tw; const UniqueDef* u = nullptr;
  for (auto& c : g_uniques) { if (c.unusual != unusual || c.weight <= 0 || templatesFor(c).empty()) continue; u = &c; x -= c.weight; if (x <= 0) break; }
  const char* word = unusual ? "Unusual" : "Unique";
  if (!u) return false;
  std::vector<const DropTemplate*> ts = templatesFor(*u);
  const DropTemplate* t = ts[(size_t)(Rand01() * ts.size()) % ts.size()];
  Item it; it.family = t->family; it.base = t->base; it.builtFrom = t->builtFrom; it.price = 0; it.fromInventory = true; it.rarity = word;
  it.uses = g_usesBase > 0 ? (u->uses > 0 ? u->uses : g_usesBase + 3) : 0;   // uses=0 in [drops] keeps every gun unlimited
  for (const AffixOp& o : u->ops) { Op op; ParseOpKey(o.key, &op); op.value = EvalExpr(o.expr, 1.0); it.ops.push_back(op); }
  it.fragmentFrom = u->fragmentFrom;
  it.model = u->model; it.modelScale = u->modelScale; it.modelOwnMaterial = u->modelOwn; it.modelKeep = u->modelKeep; it.soundFrom = u->soundFrom;
  std::string name = u->name;
  for (int k = 2; FindItem(name) || IsTombstone(name); k++) name = u->name + " " + std::to_string(k);
  it.name = name;
  g_rollParts.clear(); g_rollAffixes.clear(); g_rollLabel = t->label;
  g_rollAffixes.push_back({ u->name, false });                         // the third reel shows the unique's own name
  it.description = Colored(std::string(word) + " " + t->label, RarityHex(word)) + ": " + UniqueText(*u, word, &g_rollParts);
  *rarity = word; *out = it;
  return true;
}

// Saved Uniques follow their [unique] block, so a rebalance reaches guns found earlier: a gun named
// 'X' or 'X N' takes X's current effects and description text. Lazarus lost its 3-mission limit, so a Lazarus found
// with it gets those 3 missions back, once (its description no longer says so afterwards). Runs at launch and after an
// inventory switch, after ConvertSlowShells.
static void SaveInventory();
static void RefreshUniques() {
  int changed = 0;
  for (Item& it : g_items) {
    if (it.rarity != "Unique" || !it.fromInventory) continue;
    const UniqueDef* u = nullptr;
    for (const UniqueDef& c : g_uniques) {
      if (c.unusual) continue;
      bool match = it.name == c.name;
      if (!match && it.name.size() > c.name.size() + 1 && it.name.compare(0, c.name.size() + 1, c.name + " ") == 0)
        match = it.name.find_first_not_of("0123456789", c.name.size() + 1) == std::string::npos;
      if (match) { u = &c; break; }
    }
    if (!u) continue;
    if (g_invUniqueUses < 8 && it.uses > 0 && u->uses >= 8) {          // Uniques last 8 missions (2026-11-16), once per inventory
      it.uses += 2; changed++;
      Log("unique '%s': +2 missions (Uniques now last 8), %d left", it.name.c_str(), it.uses);
    }
    std::vector<Op> ops;
    for (const AffixOp& o : u->ops) { Op op; ParseOpKey(o.key, &op); op.value = EvalExpr(o.expr, 1.0); ops.push_back(op); }
    size_t colon = it.description.find(": ");
    std::string desc = (colon == std::string::npos ? Colored("Unique " + it.base, RarityHex("Unique")) : it.description.substr(0, colon)) + ": " + UniqueText(*u, "Unique", nullptr);
    bool same = desc == it.description && it.fragmentFrom == u->fragmentFrom && ops.size() == it.ops.size();
    for (size_t k = 0; same && k < ops.size(); k++)
      same = ops[k].field == it.ops[k].field && ops[k].mul == it.ops[k].mul && ops[k].proj == it.ops[k].proj && ops[k].fx == it.ops[k].fx &&
             ops[k].mount == it.ops[k].mount && ops[k].sphere == it.ops[k].sphere && fabs(ops[k].value - it.ops[k].value) < 1e-4;
    if (same) continue;
    int addUses = 0;
    if (u->name == "Lazarus" && it.uses > 0 && it.description.find("only lasts 3 missions") != std::string::npos && desc.find("only lasts 3 missions") == std::string::npos) addUses = 3;
    it.ops = ops; it.fragmentFrom = u->fragmentFrom; it.description = desc; it.uses += addUses;
    Log("unique '%s': now has the current '%s' effects%s", it.name.c_str(), u->name.c_str(), addUses ? " (+3 missions: its 3-mission limit is gone)" : "");
    changed++;
  }
  g_invUniqueUses = 8;
  if (changed) SaveInventory();
}

// inventory.ini is rewritten whole whenever a found gun is added, used up or retired.
// One [item] block as inventory.ini has it. coop: what another player's machine needs to build the gun (no grid place,
// loot state or cell size, but how the gun was made).
static std::string ItemText(const Item& it, bool coop) {
  char b[64]; std::string s = "\n[item]\nname=" + it.name + "\nfamily=" + it.family + "\nbase=" + it.base + "\ndescription=" + it.description + "\nprice=" + std::to_string(it.price) + "\n";
  // not sent in co-op: a gun spends a use on its first shot of a battle, which would change its text and stamp and
  // make every other machine rebuild it mid-battle. Other players only build and draw the gun.
  if (it.uses > 0 && !coop) s += "uses=" + std::to_string(it.uses) + "\n";
  if (it.scale != 1.0f) { snprintf(b, sizeof b, "scale=%.3f\n", it.scale); s += b; }
  if (!it.rarity.empty()) s += "rarity=" + it.rarity + "\n";
  if (!coop && it.gx >= 0) s += "grid=" + std::to_string(it.gx) + "," + std::to_string(it.gy) + "\n";
  if (!coop && it.held) s += "held=1\n";
  if (!it.fragmentFrom.empty()) s += "fragment_from=" + it.fragmentFrom + "\n";
  if (!it.projectileFrom.empty()) s += "projectile_from=" + it.projectileFrom + "\n";
  if (!it.model.empty()) { snprintf(b, sizeof b, "%.3f", it.modelScale); s += "model=" + it.model + "\nmodel_scale=" + b + "\nmodel_material=" + (it.modelOwnMaterial ? "own" : "gun") + "\n"; }
  if (!it.modelKeep.empty()) s += "model_keep=" + JoinList(it.modelKeep) + "\n";
  if (!it.soundFrom.empty()) s += "sound_from=" + it.soundFrom + "\n";
  if (!it.builtFrom.empty()) s += "built_from=" + it.builtFrom + "\n";
  if (!coop && it.gw > 0) s += "size=" + std::to_string(it.gw) + "x" + std::to_string(it.gh) + "\n";
  if (!coop && !it.vetCrew.empty()) s += "vet_crew=" + it.vetCrew + "\nvet_kills=" + std::to_string(it.vetKills) + "\n";   // like uses=: changes in battle
  if (it.retired) s += "retired=1\n";
  if (coop && it.fromInventory) s += "from_inventory=1\n";
  if (coop && it.builtIn) s += "built_in=1\n";
  for (const Op& o : it.ops) { char v[400]; snprintf(v, sizeof v, "=%.4f\n", o.value); s += std::string(OpPrefix(o)) + (o.mul ? "mul." : "set.") + o.field + v; }
  return s;
}
static void SaveInventory() {
  std::string path = g_folder + "\\inventory.ini";
  FILE* f = OpenForSave(path);
  if (!f) { Log("could not write inventory.ini"); return; }
  fputs("# Guns found in play, written by BoatCrewWeaponMod. Same format as weapons.ini.\n"
        "# uses=N is the number of missions the gun has left. retired=1 hides a gun; it is deleted at the next launch.\n", f);
  fprintf(f, "# materials = Scrap Iron, Brass, Gunmetal; crates = unopened Iron, Brass, Gunmetal crates\nmaterials=%d,%d,%d\ncrates=%d,%d,%d\nunique_uses=%d\n",
          g_mat[0], g_mat[1], g_mat[2], g_crates[0], g_crates[1], g_crates[2], g_invUniqueUses);
  for (const Item& it : g_items) if (it.fromInventory) fputs(ItemText(it, false).c_str(), f);
  FinishSave(f, path);
}

// Slow shells keep their range (since 2026-10-28). Found guns saved before that are converted at launch:
//  - steering ones (Homing, The Ferryman: fx seek) trade their lower MuzzleVelocity for ProjectileType.timeScale, the
//    same flight in slow motion (see kAffixLineUpdates), with FrameLifeTime x 1/speed; rolled guns are also held to the
//    -60% cap (Homing once rolled -85..-90%, which left a 3-inch with a 230 m range and it never fired), and the number in
//    their description follows. Uniques keep their own speed.
//  - others keep their lower MuzzleVelocity, so the gun's aim still leads moving targets right, and get FrameLifeTime
//    x 1/speed so the shell flies as far before it expires.
static const double kMinShellSpeed = 0.4;
static void ConvertSlowShells() {
  int changed = 0;
  for (Item& it : g_items) {
    if (!it.fromInventory) continue;
    double was = 1;
    for (const Op& o : it.ops) if (o.proj && o.mul && o.field == "MuzzleVelocity") was = o.value;
    if (was >= 1 - 1e-6 || was <= 0) continue;
    bool unique = it.rarity == "Unique" || it.rarity == "Unusual";
    double life = 1;
    for (const Op& o : it.ops) if (o.proj && o.mul && o.field == "FrameLifeTime") life = o.value;
    auto setOp = [&](const char* field, double v) {
      for (Op& o : it.ops) if (o.proj && o.mul && o.field == field) { o.value = v; return; }
      Op o; o.field = field; o.mul = true; o.proj = true; o.value = v; it.ops.push_back(o);
    };
    if (ItemFx(it, "seek") > 0) {
      double r = !unique && was < kMinShellSpeed ? kMinShellSpeed : was;
      it.ops.erase(std::remove_if(it.ops.begin(), it.ops.end(), [](const Op& o) { return o.proj && o.mul && (o.field == "MuzzleVelocity" || o.field == "MuzzleVelocityRealistic"); }), it.ops.end());
      setOp("timeScale", r);
      setOp("FrameLifeTime", life * was / r > 1 / r ? life * was / r : 1 / r);
      if (r != was)
        for (size_t at = it.description.find("% shell speed"); at != std::string::npos; at = it.description.find("% shell speed", at + 1)) {
          size_t from = at;                                            // the negative one: "-88% shell speed" -> "-60% shell speed"
          while (from > 0 && isdigit((unsigned char)it.description[from - 1])) from--;
          if (from == 0 || from == at || it.description[from - 1] != '-') continue;
          it.description.replace(from - 1, at - from + 1, std::to_string((int)std::lround((r - 1) * 100)));
          break;
        }
      Log("'%s': shell speed x%.2f now slow motion x%.2f (timeScale), so it keeps the stock range", it.name.c_str(), was, r);
    } else {
      if (life >= 1 / was - 1e-3) continue;
      setOp("FrameLifeTime", 1 / was);
      Log("'%s': shells at x%.2f speed now live x%.2f longer, so they fly as far as the stock gun's", it.name.c_str(), was, 1 / was);
    }
    changed++;
  }
  if (changed) SaveInventory();
}

// Automated found before 0.9.3 carries mul.Accuracy=0.8, which scales the score itself (a 95 gun -> 76, 4.8x the spread).
// 0.9.3's Automated is mul.Inaccuracy=1.25 (25% wider spread; see ApplyOp), so those guns are converted at launch and
// after an inventory switch.
static void ConvertAutomated() {
  int changed = 0;
  const std::string oldText = "-20% accuracy", newText = "25% wider spread";
  for (Item& it : g_items) {
    if (!it.fromInventory || it.description.find("fires on its own with no gunner") == std::string::npos) continue;
    size_t at = it.description.find(oldText);
    if (at == std::string::npos) continue;
    Op* acc = nullptr;
    for (Op& o : it.ops)
      if (o.mul && !o.proj && !o.mount && !o.sphere && !o.fx && o.field == "Accuracy" && fabs(o.value - 0.8) < 1e-3) { acc = &o; break; }
    if (!acc) continue;
    acc->field = "Inaccuracy"; acc->value = 1.25;
    it.description.replace(at, oldText.size(), newText);
    Log("'%s': Automated now gives a 25%% wider spread instead of -20%% accuracy", it.name.c_str());
    changed++;
  }
  if (changed) SaveInventory();
}

// At launch, found guns that were retired last time are deleted for good. Their names STAY in positions.ini as
// tombstones (a hidden placeholder mount takes the position), so later guns never shift and a saved loadout never comes
// back with a different gun; a slot that pointed at a deleted gun is emptied by UnequipRetired.
static void PurgeRetired() {
  bool changed = false;
  for (size_t i = 0; i < g_items.size(); ) {
    if (g_items[i].fromInventory && g_items[i].retired) {
      Log("purged retired gun '%s'", g_items[i].name.c_str());
      g_items.erase(g_items.begin() + i);
      changed = true;
    } else i++;
  }
  if (changed) SaveInventory();
}

