// crew_report_test.cpp: renders three real battles (transcribed from BoatCrewMod/crew-reports.txt: 18:45 big fight,
// 19:15 nobody hurt, 19:33 kills only with the grazes left out) through crew_report.h and checks the output.
// Build and run: tools\build_report_test.bat
#include "../src/crew_report.h"
#include "../src/crew_life.h"
#include "../src/crew_talk.h"
#include "../src/crew_bonds.h"
#include "../src/crew_nick.h"
#include <cstdio>
#include <algorithm>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static int g_fail = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); ++g_fail; } } while (0)

static uint32_t SeedOf(const char* s) {
  uint32_t h = 2166136261u;
  for (; *s; ++s) { h ^= (unsigned char)*s; h *= 16777619u; }
  return h;
}
static RepEv Hit(float t, int kind, int tier, const char* part, const char* part2, const char* enemy, const char* base,
                 const char* gun, const char* shell, bool close, float lost, float hp) {
  RepEv e;
  e.t = t; e.kind = kind; e.tier = tier; e.part = part; e.part2 = part2; e.enemy = enemy; e.enemyBase = base;
  e.gun = gun; e.shell = shell; e.close = close; e.lost = lost; e.hpLeft = hp;
  return e;
}
static RepEv St(float t, int kind, bool whileDown = false) {
  RepEv e;
  e.t = t; e.kind = kind; e.whileDown = whileDown;
  return e;
}
static RepEv Kill(float t, const char* enemy, const char* base, const char* gun, std::vector<std::string> mates, int id) {
  RepEv e;
  e.t = t; e.kind = K_KILL; e.enemy = enemy; e.enemyBase = base; e.gun = gun; e.mates = mates; e.killId = id;
  return e;
}

// ---- data (generated from crew-reports.txt; grazes spread evenly over each enemy's time range in the header) ----
static RepBattle BattleBig() {
  RepBattle b;
  b.len = 652;
  b.seed = SeedOf("2026-11-10 18:45");
  {
    RepCrew c;
    c.name = "George Thomason"; c.female = false; c.role = "passing ammunition";
    c.ev.push_back(Hit(204, R_CLOSE, 1, "the left leg", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", true, 13, 211));
    c.ev.push_back(Hit(264, R_CLOSE, 1, "the chest", "", "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "", true, 27, 175));
    c.ev.push_back(St(264, S_WOUNDED));
    c.ev.push_back(Hit(545, R_CLOSE, 1, "the left leg", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", true, 13, 172));
    c.ev.push_back(Hit(547, R_CLOSE, 1, "the left leg", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", true, 15, 157));
    c.ev.push_back(Hit(558, R_CLOSE, 1, "the right leg", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", true, 16, 128));
    c.grazes.push_back({193.6f, "Ka-Mi Amphibious", 6.6f});
    c.grazes.push_back({200.4f, "Short King IJA Type A Landing Boat", 1.5f});
    c.grazes.push_back({201.1f, "Short King IJA Type A Landing Boat", 1.5f});
    c.grazes.push_back({201.9f, "Short King IJA Type A Landing Boat", 1.5f});
    c.grazes.push_back({202.6f, "Short King IJA Type A Landing Boat", 1.5f});
    c.grazes.push_back({203.4f, "Short King IJA Type A Landing Boat", 1.5f});
    c.grazes.push_back({204.1f, "Short King IJA Type A Landing Boat", 1.5f});
    c.grazes.push_back({204.9f, "Short King IJA Type A Landing Boat", 1.5f});
    c.grazes.push_back({205.6f, "Short King IJA Type A Landing Boat", 1.5f});
    c.grazes.push_back({206.4f, "Short King IJA Type A Landing Boat", 1.5f});
    c.grazes.push_back({207.1f, "Short King IJA Type A Landing Boat", 1.5f});
    c.grazes.push_back({207.9f, "Short King IJA Type A Landing Boat", 1.5f});
    c.grazes.push_back({208.6f, "Short King IJA Type A Landing Boat", 1.5f});
    c.grazes.push_back({214.9f, "Ka-Mi Amphibious", 6.6f});
    c.grazes.push_back({227.1f, "Armored Ka-Mi Amphibious", 10.0f});
    c.grazes.push_back({236.2f, "Ka-Mi Amphibious", 6.6f});
    c.grazes.push_back({257.5f, "Ka-Mi Amphibious", 6.6f});
    c.grazes.push_back({278.8f, "Ka-Mi Amphibious", 6.6f});
    c.grazes.push_back({287.2f, "Armored Ka-Mi Amphibious", 10.0f});
    c.grazes.push_back({300.1f, "Ka-Mi Amphibious", 6.6f});
    c.grazes.push_back({321.4f, "Ka-Mi Amphibious", 6.6f});
    c.grazes.push_back({347.4f, "Armored Ka-Mi Amphibious", 10.0f});
    c.grazes.push_back({407.6f, "Armored Ka-Mi Amphibious", 10.0f});
    c.grazes.push_back({467.8f, "Armored Ka-Mi Amphibious", 10.0f});
    c.grazes.push_back({527.9f, "Armored Ka-Mi Amphibious", 10.0f});
    c.maxHp = 248; c.endHp = 128; c.outcome = O_WOUNDED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Myron U. Andrews"; c.female = false; c.role = "passing ammunition";
    c.ev.push_back(Hit(197, R_CLOSE, 1, "the left leg", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", true, 14, 225));
    c.ev.push_back(Hit(202, R_CLOSE, 1, "the left leg", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", true, 19, 204));
    c.ev.push_back(Hit(204, R_PAIR, 1, "the right leg", "the right leg", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "57 mm Tank Round (JAP)", true, 44, 159));
    c.ev.push_back(St(204, S_WOUNDED));
    c.ev.push_back(Hit(208, R_FAR, 1, "the left leg", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", false, 12, 134));
    c.ev.push_back(Hit(264, R_CLOSE, 1, "the chest", "", "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "", true, 18, 180));
    c.ev.push_back(Hit(525, R_CLOSE, 1, "the right arm", "", "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "", true, 12, 167));
    c.grazes.push_back({202.8f, "Ka-Mi Amphibious", 4.1f});
    c.grazes.push_back({222.8f, "Armored Ka-Mi Amphibious", 9.4f});
    c.grazes.push_back({252.4f, "Ka-Mi Amphibious", 4.1f});
    c.grazes.push_back({261.4f, "Soukou-Tei", 3.1f});
    c.grazes.push_back({262.2f, "Soukou-Tei", 3.1f});
    c.grazes.push_back({263.0f, "Soukou-Tei", 3.1f});
    c.grazes.push_back({263.9f, "Soukou-Tei", 3.1f});
    c.grazes.push_back({264.7f, "Soukou-Tei", 3.1f});
    c.grazes.push_back({265.5f, "Soukou-Tei", 3.1f});
    c.grazes.push_back({266.3f, "Soukou-Tei", 3.1f});
    c.grazes.push_back({267.1f, "Soukou-Tei", 3.1f});
    c.grazes.push_back({268.0f, "Soukou-Tei", 3.1f});
    c.grazes.push_back({268.8f, "Soukou-Tei", 3.1f});
    c.grazes.push_back({269.6f, "Soukou-Tei", 3.1f});
    c.grazes.push_back({274.4f, "Armored Ka-Mi Amphibious", 9.4f});
    c.grazes.push_back({301.9f, "Ka-Mi Amphibious", 4.1f});
    c.grazes.push_back({325.9f, "Armored Ka-Mi Amphibious", 9.4f});
    c.grazes.push_back({351.5f, "Ka-Mi Amphibious", 4.1f});
    c.grazes.push_back({377.5f, "Armored Ka-Mi Amphibious", 9.4f});
    c.grazes.push_back({401.1f, "Ka-Mi Amphibious", 4.1f});
    c.grazes.push_back({429.1f, "Armored Ka-Mi Amphibious", 9.4f});
    c.grazes.push_back({450.6f, "Ka-Mi Amphibious", 4.1f});
    c.grazes.push_back({480.6f, "Armored Ka-Mi Amphibious", 9.4f});
    c.grazes.push_back({500.2f, "Ka-Mi Amphibious", 4.1f});
    c.grazes.push_back({532.2f, "Armored Ka-Mi Amphibious", 9.4f});
    c.maxHp = 240; c.endHp = 167; c.outcome = O_WOUNDED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Mae Cushman"; c.female = true; c.role = "repairing";
    c.ev.push_back(Hit(264, R_CLOSE, 1, "the back", "", "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "", true, 14, 207));
    c.ev.push_back(St(268, S_WOUNDED));
    c.ev.push_back(Hit(336, R_CLOSE, 1, "the head", "", "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "", true, 22, 150));
    c.ev.push_back(Hit(558, R_CLOSE, 1, "the left leg", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", true, 27, 149));
    c.grazes.push_back({191.5f, "Ka-Mi Amphibious", 6.3f});
    c.grazes.push_back({208.5f, "Ka-Mi Amphibious", 6.3f});
    c.grazes.push_back({225.5f, "Ka-Mi Amphibious", 6.3f});
    c.grazes.push_back({231.7f, "Armored Ka-Mi Amphibious", 8.0f});
    c.grazes.push_back({242.5f, "Ka-Mi Amphibious", 6.3f});
    c.grazes.push_back({259.5f, "Ka-Mi Amphibious", 6.3f});
    c.grazes.push_back({261.4f, "Soukou-Tei", 2.1f});
    c.grazes.push_back({262.4f, "Soukou-Tei", 2.1f});
    c.grazes.push_back({263.2f, "Soukou-Tei", 2.1f});
    c.grazes.push_back({264.1f, "Soukou-Tei", 2.1f});
    c.grazes.push_back({265.1f, "Soukou-Tei", 2.1f});
    c.grazes.push_back({265.9f, "Soukou-Tei", 2.1f});
    c.grazes.push_back({266.9f, "Soukou-Tei", 2.1f});
    c.grazes.push_back({267.8f, "Soukou-Tei", 2.1f});
    c.grazes.push_back({268.6f, "Soukou-Tei", 2.1f});
    c.grazes.push_back({269.6f, "Soukou-Tei", 2.1f});
    c.grazes.push_back({276.5f, "Ka-Mi Amphibious", 6.3f});
    c.grazes.push_back({291.0f, "Armored Ka-Mi Amphibious", 8.0f});
    c.grazes.push_back({293.5f, "Ka-Mi Amphibious", 6.3f});
    c.grazes.push_back({310.5f, "Ka-Mi Amphibious", 6.3f});
    c.grazes.push_back({327.5f, "Ka-Mi Amphibious", 6.3f});
    c.grazes.push_back({350.3f, "Armored Ka-Mi Amphibious", 8.0f});
    c.grazes.push_back({409.7f, "Armored Ka-Mi Amphibious", 8.0f});
    c.grazes.push_back({469.0f, "Armored Ka-Mi Amphibious", 8.0f});
    c.grazes.push_back({528.3f, "Armored Ka-Mi Amphibious", 8.0f});
    c.maxHp = 260; c.endHp = 149; c.outcome = O_WOUNDED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Carwood Herron"; c.female = false; c.role = "repairing";
    c.ev.push_back(Hit(202, R_CLOSE, 1, "the left leg", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", true, 17, 183));
    c.ev.push_back(Hit(204, R_CLOSE, 1, "the left leg", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", true, 28, 154));
    c.ev.push_back(St(207, S_WOUNDED));
    c.ev.push_back(Hit(208, R_CLOSE, 1, "the left leg", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", true, 13, 135));
    c.ev.push_back(Hit(262, R_PAIR, 1, "the left leg", "the left leg", "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "57 mm Tank Round (JAP)", true, 44, 127));
    c.ev.push_back(Hit(264, R_PAIR, 1, "the right arm", "the back", "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "57 mm Tank Round (JAP)", true, 44, 84));
    c.ev.push_back(Hit(267, R_PAIR, 1, "the right arm", "the back", "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "57 mm Tank Round (JAP)", true, 44, 61));
    c.ev.push_back(St(267, S_OVER_BLAST));
    c.ev.push_back(St(360, S_PULLED));
    c.ev.push_back(Hit(552, R_FAR, 1, "the chest", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", false, 11, 90));
    c.ev.push_back(Hit(555, R_FAR, 1, "the left leg", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", false, 12, 79));
    c.ev.push_back(Hit(558, R_CLOSE, 1, "the right arm", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", true, 27, 52));
    c.grazes.push_back({189.0f, "Ka-Mi Amphibious", 9.6f});
    c.grazes.push_back({200.4f, "Short King IJA Type A Landing Boat", 2.0f});
    c.grazes.push_back({201.0f, "Ka-Mi Amphibious", 9.6f});
    c.grazes.push_back({201.2f, "Short King IJA Type A Landing Boat", 2.0f});
    c.grazes.push_back({202.0f, "Short King IJA Type A Landing Boat", 2.0f});
    c.grazes.push_back({202.9f, "Short King IJA Type A Landing Boat", 2.0f});
    c.grazes.push_back({203.7f, "Short King IJA Type A Landing Boat", 2.0f});
    c.grazes.push_back({204.5f, "Short King IJA Type A Landing Boat", 2.0f});
    c.grazes.push_back({205.3f, "Short King IJA Type A Landing Boat", 2.0f});
    c.grazes.push_back({206.1f, "Short King IJA Type A Landing Boat", 2.0f});
    c.grazes.push_back({207.0f, "Short King IJA Type A Landing Boat", 2.0f});
    c.grazes.push_back({207.8f, "Short King IJA Type A Landing Boat", 2.0f});
    c.grazes.push_back({208.6f, "Short King IJA Type A Landing Boat", 2.0f});
    c.grazes.push_back({213.0f, "Ka-Mi Amphibious", 9.6f});
    c.grazes.push_back({225.0f, "Ka-Mi Amphibious", 9.6f});
    c.grazes.push_back({237.0f, "Ka-Mi Amphibious", 9.6f});
    c.grazes.push_back({249.0f, "Ka-Mi Amphibious", 9.6f});
    c.grazes.push_back({261.0f, "Ka-Mi Amphibious", 9.6f});
    c.grazes.push_back({380.0f, "Armored Ka-Mi Amphibious", 1.0f});
    c.maxHp = 204; c.endHp = 52; c.outcome = O_WOUNDED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Rosalind Dickerson"; c.female = true; c.role = "at the helm";
    c.grazes.push_back({188.8f, "Ka-Mi Amphibious", 5.2f});
    c.grazes.push_back({200.3f, "Short King IJA Type A Landing Boat", 1.2f});
    c.grazes.push_back({201.0f, "Short King IJA Type A Landing Boat", 1.2f});
    c.grazes.push_back({201.7f, "Short King IJA Type A Landing Boat", 1.2f});
    c.grazes.push_back({202.4f, "Short King IJA Type A Landing Boat", 1.2f});
    c.grazes.push_back({203.1f, "Short King IJA Type A Landing Boat", 1.2f});
    c.grazes.push_back({203.8f, "Short King IJA Type A Landing Boat", 1.2f});
    c.grazes.push_back({204.5f, "Short King IJA Type A Landing Boat", 1.2f});
    c.grazes.push_back({205.2f, "Short King IJA Type A Landing Boat", 1.2f});
    c.grazes.push_back({205.9f, "Short King IJA Type A Landing Boat", 1.2f});
    c.grazes.push_back({206.6f, "Short King IJA Type A Landing Boat", 1.2f});
    c.grazes.push_back({207.3f, "Short King IJA Type A Landing Boat", 1.2f});
    c.grazes.push_back({208.0f, "Short King IJA Type A Landing Boat", 1.2f});
    c.grazes.push_back({208.7f, "Short King IJA Type A Landing Boat", 1.2f});
    c.grazes.push_back({210.5f, "Ka-Mi Amphibious", 5.2f});
    c.grazes.push_back({213.4f, "Armored Ka-Mi Amphibious", 4.3f});
    c.grazes.push_back({232.2f, "Ka-Mi Amphibious", 5.2f});
    c.grazes.push_back({246.2f, "Armored Ka-Mi Amphibious", 4.3f});
    c.grazes.push_back({253.9f, "Ka-Mi Amphibious", 5.2f});
    c.grazes.push_back({275.6f, "Ka-Mi Amphibious", 5.2f});
    c.grazes.push_back({279.0f, "Armored Ka-Mi Amphibious", 4.3f});
    c.grazes.push_back({297.3f, "Ka-Mi Amphibious", 5.2f});
    c.grazes.push_back({311.9f, "Armored Ka-Mi Amphibious", 4.3f});
    c.grazes.push_back({319.0f, "Ka-Mi Amphibious", 5.2f});
    c.grazes.push_back({340.7f, "Ka-Mi Amphibious", 5.2f});
    c.grazes.push_back({344.7f, "Armored Ka-Mi Amphibious", 4.3f});
    c.grazes.push_back({362.3f, "Ka-Mi Amphibious", 5.2f});
    c.grazes.push_back({377.5f, "Armored Ka-Mi Amphibious", 4.3f});
    c.grazes.push_back({384.0f, "Ka-Mi Amphibious", 5.2f});
    c.grazes.push_back({405.7f, "Ka-Mi Amphibious", 5.2f});
    c.grazes.push_back({410.3f, "Armored Ka-Mi Amphibious", 4.3f});
    c.grazes.push_back({427.4f, "Ka-Mi Amphibious", 5.2f});
    c.grazes.push_back({443.1f, "Armored Ka-Mi Amphibious", 4.3f});
    c.grazes.push_back({449.1f, "Ka-Mi Amphibious", 5.2f});
    c.grazes.push_back({470.8f, "Ka-Mi Amphibious", 5.2f});
    c.grazes.push_back({476.0f, "Armored Ka-Mi Amphibious", 4.3f});
    c.grazes.push_back({492.5f, "Ka-Mi Amphibious", 5.2f});
    c.grazes.push_back({508.8f, "Armored Ka-Mi Amphibious", 4.3f});
    c.grazes.push_back({514.2f, "Ka-Mi Amphibious", 5.2f});
    c.grazes.push_back({541.6f, "Armored Ka-Mi Amphibious", 4.3f});
    c.maxHp = 270; c.endHp = 124; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Darrell Dominquez"; c.female = false; c.role = "tending the wounded";
    c.ev.push_back(Hit(262, R_CLOSE, 1, "the left leg", "", "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "", true, 25, 194));
    c.ev.push_back(Hit(264, R_CLOSE, 1, "the right arm", "", "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "", true, 27, 167));
    c.ev.push_back(St(264, S_WOUNDED));
    c.ev.push_back(Hit(267, R_CLOSE, 1, "the right arm", "", "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "", true, 28, 138));
    c.ev.push_back(Hit(558, R_CLOSE, 1, "the left leg", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", true, 22, 86));
    c.grazes.push_back({187.6f, "Ka-Mi Amphibious", 7.0f});
    c.grazes.push_back({206.9f, "Ka-Mi Amphibious", 7.0f});
    c.grazes.push_back({226.1f, "Ka-Mi Amphibious", 7.0f});
    c.grazes.push_back({245.4f, "Ka-Mi Amphibious", 7.0f});
    c.grazes.push_back({248.2f, "Armored Ka-Mi Amphibious", 7.8f});
    c.grazes.push_back({261.4f, "Soukou-Tei", 1.6f});
    c.grazes.push_back({262.4f, "Soukou-Tei", 1.6f});
    c.grazes.push_back({263.2f, "Soukou-Tei", 1.6f});
    c.grazes.push_back({264.1f, "Soukou-Tei", 1.6f});
    c.grazes.push_back({264.6f, "Ka-Mi Amphibious", 7.0f});
    c.grazes.push_back({265.1f, "Soukou-Tei", 1.6f});
    c.grazes.push_back({265.9f, "Soukou-Tei", 1.6f});
    c.grazes.push_back({266.9f, "Soukou-Tei", 1.6f});
    c.grazes.push_back({267.8f, "Soukou-Tei", 1.6f});
    c.grazes.push_back({268.6f, "Soukou-Tei", 1.6f});
    c.grazes.push_back({269.6f, "Soukou-Tei", 1.6f});
    c.grazes.push_back({283.9f, "Ka-Mi Amphibious", 7.0f});
    c.grazes.push_back({303.1f, "Ka-Mi Amphibious", 7.0f});
    c.grazes.push_back({322.4f, "Ka-Mi Amphibious", 7.0f});
    c.grazes.push_back({336.8f, "Armored Ka-Mi Amphibious", 7.8f});
    c.grazes.push_back({425.2f, "Armored Ka-Mi Amphibious", 7.8f});
    c.grazes.push_back({513.8f, "Armored Ka-Mi Amphibious", 7.8f});
    c.maxHp = 258; c.endHp = 86; c.outcome = O_WOUNDED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Bernard Tipper"; c.female = false; c.role = "on the Heavy Lewis of Salvage";
    c.gun = "Heavy Lewis of Salvage"; c.gunBase = "30 Cal Dual MG (Lewis)";
    c.ev.push_back(Kill(146, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "Heavy Lewis of Salvage", {"Earl Davenport"}, 1));
    c.ev.push_back(Kill(152, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Heavy Lewis of Salvage", {}, 2));
    c.ev.push_back(Kill(167, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "Heavy Lewis of Salvage", {"Earl Davenport", "Jack Janovec"}, 3));
    c.ev.push_back(Kill(187, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Heavy Lewis of Salvage", {"Earl Davenport", "Jack Janovec"}, 4));
    c.ev.push_back(Kill(197, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Heavy Lewis of Salvage", {"Earl Davenport", "Jack Janovec"}, 5));
    c.ev.push_back(Hit(204, R_CLOSE, 1, "the right leg", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", true, 15, 211));
    c.ev.push_back(Kill(209, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "Heavy Lewis of Salvage", {"Earl Davenport", "Jack Janovec"}, 6));
    c.ev.push_back(Kill(216, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Heavy Lewis of Salvage", {"Jack Janovec"}, 7));
    c.ev.push_back(Kill(259, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Heavy Lewis of Salvage", {"Jack Janovec"}, 8));
    c.ev.push_back(St(269, S_WOUNDED));
    c.ev.push_back(Kill(297, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Heavy Lewis of Salvage", {"Jack Janovec"}, 9));
    c.ev.push_back(Kill(333, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Heavy Lewis of Salvage", {"Jack Janovec"}, 10));
    c.ev.push_back(Kill(338, "A6M Zero", "A6M Zero", "Heavy Lewis of Salvage", {}, 11));
    c.ev.push_back(Kill(450, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "Heavy Lewis of Salvage", {"Earl Davenport"}, 12));
    c.ev.push_back(Kill(480, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Heavy Lewis of Salvage", {"Earl Davenport", "Jack Janovec"}, 13));
    c.ev.push_back(Kill(489, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Heavy Lewis of Salvage", {"Earl Davenport", "Jack Janovec"}, 14));
    c.ev.push_back(Kill(518, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Heavy Lewis of Salvage", {"Earl Davenport", "Jack Janovec"}, 15));
    c.ev.push_back(Kill(523, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Heavy Lewis of Salvage", {"Jack Janovec"}, 16));
    c.ev.push_back(Kill(527, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Heavy Lewis of Salvage", {"Earl Davenport", "Jack Janovec"}, 17));
    c.ev.push_back(Kill(568, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Heavy Lewis of Salvage", {"Earl Davenport", "Jack Janovec"}, 18));
    c.grazes.push_back({185.7f, "Ka-Mi Amphibious", 5.5f});
    c.grazes.push_back({200.4f, "Short King IJA Type A Landing Boat", 1.7f});
    c.grazes.push_back({201.1f, "Ka-Mi Amphibious", 5.5f});
    c.grazes.push_back({201.2f, "Short King IJA Type A Landing Boat", 1.7f});
    c.grazes.push_back({202.0f, "Short King IJA Type A Landing Boat", 1.7f});
    c.grazes.push_back({202.9f, "Short King IJA Type A Landing Boat", 1.7f});
    c.grazes.push_back({203.7f, "Short King IJA Type A Landing Boat", 1.7f});
    c.grazes.push_back({204.5f, "Short King IJA Type A Landing Boat", 1.7f});
    c.grazes.push_back({205.3f, "Short King IJA Type A Landing Boat", 1.7f});
    c.grazes.push_back({206.1f, "Short King IJA Type A Landing Boat", 1.7f});
    c.grazes.push_back({207.0f, "Short King IJA Type A Landing Boat", 1.7f});
    c.grazes.push_back({207.8f, "Short King IJA Type A Landing Boat", 1.7f});
    c.grazes.push_back({208.6f, "Short King IJA Type A Landing Boat", 1.7f});
    c.grazes.push_back({216.5f, "Ka-Mi Amphibious", 5.5f});
    c.grazes.push_back({231.9f, "Ka-Mi Amphibious", 5.5f});
    c.grazes.push_back({247.3f, "Ka-Mi Amphibious", 5.5f});
    c.grazes.push_back({251.5f, "Armored Ka-Mi Amphibious", 10.0f});
    c.grazes.push_back({262.7f, "Ka-Mi Amphibious", 5.5f});
    c.grazes.push_back({278.1f, "Ka-Mi Amphibious", 5.5f});
    c.grazes.push_back({293.5f, "Ka-Mi Amphibious", 5.5f});
    c.grazes.push_back({308.9f, "Ka-Mi Amphibious", 5.5f});
    c.grazes.push_back({324.3f, "Ka-Mi Amphibious", 5.5f});
    c.grazes.push_back({350.5f, "Armored Ka-Mi Amphibious", 10.0f});
    c.grazes.push_back({449.5f, "Armored Ka-Mi Amphibious", 10.0f});
    c.maxHp = 243; c.endHp = 211; c.outcome = O_WOUNDED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Jack Janovec"; c.female = false; c.role = "on the Vampiric Vickers .50 of Salvage";
    c.gun = "Vampiric Vickers .50 of Salvage"; c.gunBase = "Vickers .50 MG";
    c.ev.push_back(Kill(167, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "Vampiric Vickers .50 of Salvage", {"Earl Davenport", "Bernard Tipper"}, 3));
    c.ev.push_back(Hit(178, R_FAR, 1, "the right leg", "", "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "", false, 12, 205));
    c.ev.push_back(Kill(187, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Vampiric Vickers .50 of Salvage", {"Earl Davenport", "Bernard Tipper"}, 4));
    c.ev.push_back(Kill(197, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Vampiric Vickers .50 of Salvage", {"Earl Davenport", "Bernard Tipper"}, 5));
    c.ev.push_back(Kill(209, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "Vampiric Vickers .50 of Salvage", {"Earl Davenport", "Bernard Tipper"}, 6));
    c.ev.push_back(Kill(216, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Vampiric Vickers .50 of Salvage", {"Bernard Tipper"}, 7));
    c.ev.push_back(Kill(244, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Vampiric Vickers .50 of Salvage", {"Earl Davenport"}, 19));
    c.ev.push_back(Kill(259, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Vampiric Vickers .50 of Salvage", {"Bernard Tipper"}, 8));
    c.ev.push_back(Hit(262, R_CLOSE, 1, "the left leg", "", "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "", true, 13, 174));
    c.ev.push_back(Hit(264, R_CLOSE, 1, "the right leg", "", "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "", true, 17, 157));
    c.ev.push_back(St(264, S_WOUNDED));
    c.ev.push_back(Hit(267, R_CLOSE, 1, "the right leg", "", "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "", true, 18, 138));
    c.ev.push_back(Kill(273, "Soukou-Tei", "Soukou-Tei", "Vampiric Vickers .50 of Salvage", {}, 20));
    c.ev.push_back(Kill(297, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Vampiric Vickers .50 of Salvage", {"Bernard Tipper"}, 9));
    c.ev.push_back(Kill(299, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "Vampiric Vickers .50 of Salvage", {"Earl Davenport"}, 21));
    c.ev.push_back(Kill(323, "Mutsuki Destroyer", "Mutsuki Destroyer", "Vampiric Vickers .50 of Salvage", {"Earl Davenport"}, 22));
    c.ev.push_back(Kill(333, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Vampiric Vickers .50 of Salvage", {"Bernard Tipper"}, 10));
    c.ev.push_back(Kill(333, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "Vampiric Vickers .50 of Salvage", {"Earl Davenport"}, 23));
    c.ev.push_back(Kill(337, "No.101 Aux. Minesweeper", "No.101 Aux. Minesweeper", "Vampiric Vickers .50 of Salvage", {"Earl Davenport"}, 24));
    c.ev.push_back(Kill(480, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Vampiric Vickers .50 of Salvage", {"Earl Davenport", "Bernard Tipper"}, 13));
    c.ev.push_back(Kill(489, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Vampiric Vickers .50 of Salvage", {"Earl Davenport", "Bernard Tipper"}, 14));
    c.ev.push_back(Kill(518, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Vampiric Vickers .50 of Salvage", {"Bernard Tipper", "Earl Davenport"}, 15));
    c.ev.push_back(Kill(523, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Vampiric Vickers .50 of Salvage", {"Bernard Tipper"}, 16));
    c.ev.push_back(Kill(527, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Vampiric Vickers .50 of Salvage", {"Earl Davenport", "Bernard Tipper"}, 17));
    c.ev.push_back(Kill(556, "IJN Motor Gunboat H35", "IJN Motor Gunboat H35", "Vampiric Vickers .50 of Salvage", {"Earl Davenport"}, 25));
    c.ev.push_back(Kill(568, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Vampiric Vickers .50 of Salvage", {"Earl Davenport", "Bernard Tipper"}, 18));
    c.grazes.push_back({190.8f, "Ka-Mi Amphibious", 5.5f});
    c.grazes.push_back({216.5f, "Ka-Mi Amphibious", 5.5f});
    c.grazes.push_back({239.1f, "Armored Ka-Mi Amphibious", 6.0f});
    c.grazes.push_back({242.2f, "Ka-Mi Amphibious", 5.5f});
    c.grazes.push_back({261.4f, "Soukou-Tei", 1.6f});
    c.grazes.push_back({262.4f, "Soukou-Tei", 1.6f});
    c.grazes.push_back({263.2f, "Soukou-Tei", 1.6f});
    c.grazes.push_back({264.1f, "Soukou-Tei", 1.6f});
    c.grazes.push_back({265.1f, "Soukou-Tei", 1.6f});
    c.grazes.push_back({265.9f, "Soukou-Tei", 1.6f});
    c.grazes.push_back({266.9f, "Soukou-Tei", 1.6f});
    c.grazes.push_back({267.8f, "Soukou-Tei", 1.6f});
    c.grazes.push_back({267.8f, "Ka-Mi Amphibious", 5.5f});
    c.grazes.push_back({268.6f, "Soukou-Tei", 1.6f});
    c.grazes.push_back({269.6f, "Soukou-Tei", 1.6f});
    c.grazes.push_back({293.5f, "Ka-Mi Amphibious", 5.5f});
    c.grazes.push_back({313.4f, "Armored Ka-Mi Amphibious", 6.0f});
    c.grazes.push_back({319.2f, "Ka-Mi Amphibious", 5.5f});
    c.grazes.push_back({387.6f, "Armored Ka-Mi Amphibious", 6.0f});
    c.grazes.push_back({461.9f, "Armored Ka-Mi Amphibious", 6.0f});
    c.maxHp = 218; c.endHp = 138; c.outcome = O_WOUNDED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Earl Davenport"; c.female = false; c.role = "on the All-Knowing 60mm Mortar of Salvage";
    c.gun = "All-Knowing 60mm Mortar of Salvage"; c.gunBase = "60mm Mortar";
    c.ev.push_back(Kill(146, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "All-Knowing 60mm Mortar of Salvage", {"Bernard Tipper"}, 1));
    c.ev.push_back(Kill(146, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "All-Knowing 60mm Mortar of Salvage", {}, 26));
    c.ev.push_back(Kill(167, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "All-Knowing 60mm Mortar of Salvage", {}, 27));
    c.ev.push_back(Kill(167, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "All-Knowing 60mm Mortar of Salvage", {"Bernard Tipper", "Jack Janovec"}, 3));
    c.ev.push_back(Kill(187, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "All-Knowing 60mm Mortar of Salvage", {"Bernard Tipper", "Jack Janovec"}, 4));
    c.ev.push_back(Hit(197, R_FAR, 1, "the left leg", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", false, 11, 197));
    c.ev.push_back(Kill(197, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "All-Knowing 60mm Mortar of Salvage", {"Bernard Tipper", "Jack Janovec"}, 5));
    c.ev.push_back(Kill(209, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "All-Knowing 60mm Mortar of Salvage", {"Jack Janovec", "Bernard Tipper"}, 6));
    c.ev.push_back(Kill(240, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "All-Knowing 60mm Mortar of Salvage", {}, 28));
    c.ev.push_back(Kill(244, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "All-Knowing 60mm Mortar of Salvage", {"Jack Janovec"}, 19));
    c.ev.push_back(Kill(246, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "All-Knowing 60mm Mortar of Salvage", {}, 29));
    c.ev.push_back(Hit(261, R_FAR, 1, "the right leg", "", "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "", false, 11, 190));
    c.ev.push_back(Hit(264, R_CLOSE, 1, "the right leg", "", "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "", true, 14, 176));
    c.ev.push_back(Kill(299, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "All-Knowing 60mm Mortar of Salvage", {"Jack Janovec"}, 21));
    c.ev.push_back(Kill(306, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "All-Knowing 60mm Mortar of Salvage", {}, 30));
    c.ev.push_back(Hit(319, R_FAR, 1, "the left leg", "", "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "", false, 11, 182));
    c.ev.push_back(Kill(319, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "All-Knowing 60mm Mortar of Salvage", {}, 31));
    c.ev.push_back(Kill(320, "No.1 Aux. Sub. Chaser", "No.1 Aux. Sub. Chaser", "All-Knowing 60mm Mortar of Salvage", {}, 32));
    c.ev.push_back(St(321, S_WOUNDED));
    c.ev.push_back(Kill(323, "Mutsuki Destroyer", "Mutsuki Destroyer", "All-Knowing 60mm Mortar of Salvage", {"Jack Janovec"}, 22));
    c.ev.push_back(Hit(329, R_CLOSE, 1, "the head", "", "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "", true, 18, 149));
    c.ev.push_back(Kill(333, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "All-Knowing 60mm Mortar of Salvage", {"Jack Janovec"}, 23));
    c.ev.push_back(Hit(336, R_CLOSE, 1, "the head", "", "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "", true, 21, 108));
    c.ev.push_back(Kill(337, "No.101 Aux. Minesweeper", "No.101 Aux. Minesweeper", "All-Knowing 60mm Mortar of Salvage", {"Jack Janovec"}, 24));
    c.ev.push_back(Kill(450, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "All-Knowing 60mm Mortar of Salvage", {"Bernard Tipper"}, 12));
    c.ev.push_back(Kill(471, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "All-Knowing 60mm Mortar of Salvage", {}, 33));
    c.ev.push_back(Kill(471, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "All-Knowing 60mm Mortar of Salvage", {}, 33));
    c.ev.push_back(Kill(472, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "All-Knowing 60mm Mortar of Salvage", {}, 34));
    c.ev.push_back(Kill(472, "IJA Type A Landing Boat", "IJA Type A Landing Boat", "All-Knowing 60mm Mortar of Salvage", {}, 34));
    c.ev.push_back(Kill(480, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "All-Knowing 60mm Mortar of Salvage", {"Bernard Tipper", "Jack Janovec"}, 13));
    c.ev.push_back(Kill(489, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "All-Knowing 60mm Mortar of Salvage", {"Jack Janovec", "Bernard Tipper"}, 14));
    c.ev.push_back(Kill(489, "IJN Motor Gunboat H35", "IJN Motor Gunboat H35", "All-Knowing 60mm Mortar of Salvage", {}, 35));
    c.ev.push_back(Hit(493, R_CLOSE, 1, "the head", "", "Long Boi Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "", true, 14, 101));
    c.ev.push_back(Hit(495, R_CLOSE, 1, "the head", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", true, 20, 80));
    c.ev.push_back(Hit(496, R_CLOSE, 1, "the head", "", "Long Boi Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "", true, 16, 65));
    c.ev.push_back(Kill(518, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "All-Knowing 60mm Mortar of Salvage", {"Bernard Tipper", "Jack Janovec"}, 15));
    c.ev.push_back(Hit(525, R_CLOSE, 1, "the chest", "", "Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "", true, 12, 103));
    c.ev.push_back(Kill(527, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "All-Knowing 60mm Mortar of Salvage", {"Jack Janovec", "Bernard Tipper"}, 17));
    c.ev.push_back(Hit(547, R_CLOSE, 1, "the chest", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", true, 14, 90));
    c.ev.push_back(Hit(552, R_CLOSE, 1, "the head", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", true, 14, 93));
    c.ev.push_back(Hit(555, R_FAR, 1, "the right leg", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", false, 11, 82));
    c.ev.push_back(Kill(556, "IJN Motor Gunboat H35", "IJN Motor Gunboat H35", "All-Knowing 60mm Mortar of Salvage", {"Jack Janovec"}, 25));
    c.ev.push_back(Hit(558, R_CLOSE, 1, "the chest", "", "Armored Ka-Mi Amphibious", "Armored Ka-Mi Amphibious", "Generic Tank Gun", "", true, 14, 68));
    c.ev.push_back(Kill(568, "Ka-Mi Amphibious", "Ka-Mi Amphibious", "All-Knowing 60mm Mortar of Salvage", {"Jack Janovec", "Bernard Tipper"}, 18));
    c.grazes.push_back({202.0f, "Ka-Mi Amphibious", 5.8f});
    c.grazes.push_back({240.0f, "Ka-Mi Amphibious", 5.8f});
    c.grazes.push_back({278.0f, "Ka-Mi Amphibious", 5.8f});
    c.grazes.push_back({316.0f, "Ka-Mi Amphibious", 5.8f});
    c.grazes.push_back({354.0f, "Ka-Mi Amphibious", 5.8f});
    c.grazes.push_back({377.5f, "Armored Ka-Mi Amphibious", 10.0f});
    c.grazes.push_back({392.0f, "Ka-Mi Amphibious", 5.8f});
    c.grazes.push_back({430.0f, "Ka-Mi Amphibious", 5.8f});
    c.grazes.push_back({468.0f, "Ka-Mi Amphibious", 5.8f});
    c.grazes.push_back({506.0f, "Ka-Mi Amphibious", 5.8f});
    c.maxHp = 212; c.endHp = 68; c.outcome = O_WOUNDED;
    b.crew.push_back(c);
  }
  b.kills.push_back({1, "IJA Type A Landing Boat", "IJA Type A Landing Boat", 146});
  b.kills.push_back({2, "Ka-Mi Amphibious", "Ka-Mi Amphibious", 152});
  b.kills.push_back({3, "IJA Type A Landing Boat", "IJA Type A Landing Boat", 167});
  b.kills.push_back({4, "Ka-Mi Amphibious", "Ka-Mi Amphibious", 187});
  b.kills.push_back({5, "Ka-Mi Amphibious", "Ka-Mi Amphibious", 197});
  b.kills.push_back({6, "IJA Type A Landing Boat", "IJA Type A Landing Boat", 209});
  b.kills.push_back({7, "Ka-Mi Amphibious", "Ka-Mi Amphibious", 216});
  b.kills.push_back({8, "Ka-Mi Amphibious", "Ka-Mi Amphibious", 259});
  b.kills.push_back({9, "Ka-Mi Amphibious", "Ka-Mi Amphibious", 297});
  b.kills.push_back({10, "Ka-Mi Amphibious", "Ka-Mi Amphibious", 333});
  b.kills.push_back({11, "A6M Zero", "A6M Zero", 338});
  b.kills.push_back({12, "IJA Type A Landing Boat", "IJA Type A Landing Boat", 450});
  b.kills.push_back({13, "Ka-Mi Amphibious", "Ka-Mi Amphibious", 480});
  b.kills.push_back({14, "Ka-Mi Amphibious", "Ka-Mi Amphibious", 489});
  b.kills.push_back({15, "Ka-Mi Amphibious", "Ka-Mi Amphibious", 518});
  b.kills.push_back({16, "Ka-Mi Amphibious", "Ka-Mi Amphibious", 523});
  b.kills.push_back({17, "Ka-Mi Amphibious", "Ka-Mi Amphibious", 527});
  b.kills.push_back({18, "Ka-Mi Amphibious", "Ka-Mi Amphibious", 568});
  b.kills.push_back({19, "Ka-Mi Amphibious", "Ka-Mi Amphibious", 244});
  b.kills.push_back({20, "Soukou-Tei", "Soukou-Tei", 273});
  b.kills.push_back({21, "IJA Type A Landing Boat", "IJA Type A Landing Boat", 299});
  b.kills.push_back({22, "Mutsuki Destroyer", "Mutsuki Destroyer", 323});
  b.kills.push_back({23, "IJA Type A Landing Boat", "IJA Type A Landing Boat", 333});
  b.kills.push_back({24, "No.101 Aux. Minesweeper", "No.101 Aux. Minesweeper", 337});
  b.kills.push_back({25, "IJN Motor Gunboat H35", "IJN Motor Gunboat H35", 556});
  b.kills.push_back({26, "IJA Type A Landing Boat", "IJA Type A Landing Boat", 146});
  b.kills.push_back({27, "IJA Type A Landing Boat", "IJA Type A Landing Boat", 167});
  b.kills.push_back({28, "IJA Type A Landing Boat", "IJA Type A Landing Boat", 240});
  b.kills.push_back({29, "IJA Type A Landing Boat", "IJA Type A Landing Boat", 246});
  b.kills.push_back({30, "IJA Type A Landing Boat", "IJA Type A Landing Boat", 306});
  b.kills.push_back({31, "IJA Type A Landing Boat", "IJA Type A Landing Boat", 319});
  b.kills.push_back({32, "No.1 Aux. Sub. Chaser", "No.1 Aux. Sub. Chaser", 320});
  b.kills.push_back({33, "IJA Type A Landing Boat", "IJA Type A Landing Boat", 471});
  b.kills.push_back({34, "IJA Type A Landing Boat", "IJA Type A Landing Boat", 472});
  b.kills.push_back({35, "IJN Motor Gunboat H35", "IJN Motor Gunboat H35", 489});
  return b;
}

static RepBattle BattleQuiet() {
  RepBattle b;
  b.len = 67;
  b.seed = SeedOf("2026-11-10 19:15");
  {
    RepCrew c;
    c.name = "George Thomason"; c.female = false; c.role = "passing ammunition";
    c.maxHp = 248; c.endHp = 248; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Myron U. Andrews"; c.female = false; c.role = "passing ammunition";
    c.maxHp = 240; c.endHp = 240; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Mae Cushman"; c.female = true; c.role = "repairing";
    c.maxHp = 260; c.endHp = 260; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Carwood Herron"; c.female = false; c.role = "repairing";
    c.maxHp = 204; c.endHp = 204; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Rosalind Dickerson"; c.female = true; c.role = "at the helm";
    c.maxHp = 270; c.endHp = 270; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Darrell Dominquez"; c.female = false; c.role = "tending the wounded";
    c.maxHp = 258; c.endHp = 258; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Bernard Tipper"; c.female = false; c.role = "on the Ambushing Flak 38 of Ruin";
    c.gun = "Ambushing Flak 38 of Ruin"; c.gunBase = "Flak 38";
    c.maxHp = 243; c.endHp = 243; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Jack Janovec"; c.female = false; c.role = "on the Vampiric Vickers .50 of Salvage";
    c.gun = "Vampiric Vickers .50 of Salvage"; c.gunBase = "Vickers .50 MG";
    c.maxHp = 218; c.endHp = 218; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Earl Davenport"; c.female = false; c.role = "on the All-Knowing 60mm Mortar of Salvage";
    c.gun = "All-Knowing 60mm Mortar of Salvage"; c.gunBase = "60mm Mortar";
    c.maxHp = 212; c.endHp = 212; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  return b;
}

static RepBattle BattleKills() {
  RepBattle b;
  b.len = 195;
  b.seed = SeedOf("2026-11-10 19:33");
  {
    RepCrew c;
    c.name = "George Thomason"; c.female = false; c.role = "passing ammunition";
    c.maxHp = 248; c.endHp = 248; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Myron U. Andrews"; c.female = false; c.role = "passing ammunition";
    c.maxHp = 240; c.endHp = 240; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Mae Cushman"; c.female = true; c.role = "repairing";
    c.maxHp = 260; c.endHp = 260; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Carwood Herron"; c.female = false; c.role = "repairing";
    c.maxHp = 204; c.endHp = 204; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Rosalind Dickerson"; c.female = true; c.role = "at the helm";
    c.maxHp = 270; c.endHp = 270; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Darrell Dominquez"; c.female = false; c.role = "tending the wounded";
    c.maxHp = 258; c.endHp = 258; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Bernard Tipper"; c.female = false; c.role = "on the Ambushing Flak 38 of Ruin";
    c.gun = "Ambushing Flak 38 of Ruin"; c.gunBase = "Flak 38";
    c.ev.push_back(Kill(97, "Ground MG Nest Sand", "Ground MG Nest Sand", "Ambushing Flak 38 of Ruin", {"Earl Davenport", "Jack Janovec"}, 1));
    c.ev.push_back(Kill(107, "Watch Tower", "Watch Tower", "Ambushing Flak 38 of Ruin", {"Earl Davenport", "Jack Janovec"}, 2));
    c.ev.push_back(Kill(110, "Ground AA", "Ground AA", "Ambushing Flak 38 of Ruin", {"Earl Davenport", "Jack Janovec"}, 3));
    c.ev.push_back(Kill(185, "Watch Tower", "Watch Tower", "Ambushing Flak 38 of Ruin", {"Earl Davenport", "Jack Janovec"}, 4));
    c.maxHp = 243; c.endHp = 243; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Jack Janovec"; c.female = false; c.role = "on the Rallying Barge Buster of Economy";
    c.gun = "Rallying Barge Buster of Economy"; c.gunBase = "Barge Buster";
    c.ev.push_back(Kill(79, "Ground Mortar 90 mm", "Ground Mortar 90 mm", "Rallying Barge Buster of Economy", {"Earl Davenport"}, 5));
    c.ev.push_back(Kill(97, "Ground MG Nest Sand", "Ground MG Nest Sand", "Rallying Barge Buster of Economy", {"Earl Davenport", "Bernard Tipper"}, 1));
    c.ev.push_back(Kill(104, "Ground MG Nest Sand", "Ground MG Nest Sand", "Rallying Barge Buster of Economy", {"Earl Davenport"}, 6));
    c.ev.push_back(Kill(107, "Watch Tower", "Watch Tower", "Rallying Barge Buster of Economy", {"Earl Davenport", "Bernard Tipper"}, 2));
    c.ev.push_back(Kill(110, "Ground AA", "Ground AA", "Rallying Barge Buster of Economy", {"Bernard Tipper", "Earl Davenport"}, 3));
    c.ev.push_back(Kill(119, "Ground Mortar 90 mm", "Ground Mortar 90 mm", "Rallying Barge Buster of Economy", {"Earl Davenport"}, 7));
    c.ev.push_back(Kill(136, "Ground MG Nest Sand", "Ground MG Nest Sand", "Rallying Barge Buster of Economy", {"Earl Davenport"}, 8));
    c.ev.push_back(Kill(170, "Ground AA", "Ground AA", "Rallying Barge Buster of Economy", {"Earl Davenport"}, 9));
    c.ev.push_back(Kill(185, "Watch Tower", "Watch Tower", "Rallying Barge Buster of Economy", {"Earl Davenport", "Bernard Tipper"}, 4));
    c.maxHp = 218; c.endHp = 218; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Earl Davenport"; c.female = false; c.role = "on the All-Knowing 60mm Mortar of Salvage";
    c.gun = "All-Knowing 60mm Mortar of Salvage"; c.gunBase = "60mm Mortar";
    c.ev.push_back(Kill(79, "Ground Mortar 90 mm", "Ground Mortar 90 mm", "All-Knowing 60mm Mortar of Salvage", {"Jack Janovec"}, 5));
    c.ev.push_back(Kill(93, "Ground MG Nest Sand", "Ground MG Nest Sand", "All-Knowing 60mm Mortar of Salvage", {}, 10));
    c.ev.push_back(Kill(94, "Watch Tower", "Watch Tower", "All-Knowing 60mm Mortar of Salvage", {}, 11));
    c.ev.push_back(Kill(97, "Ground MG Nest Sand", "Ground MG Nest Sand", "All-Knowing 60mm Mortar of Salvage", {"Jack Janovec", "Bernard Tipper"}, 1));
    c.ev.push_back(Kill(104, "Ground MG Nest Sand", "Ground MG Nest Sand", "All-Knowing 60mm Mortar of Salvage", {"Jack Janovec"}, 6));
    c.ev.push_back(Kill(107, "Watch Tower", "Watch Tower", "All-Knowing 60mm Mortar of Salvage", {"Bernard Tipper", "Jack Janovec"}, 2));
    c.ev.push_back(Kill(110, "Ground AA", "Ground AA", "All-Knowing 60mm Mortar of Salvage", {"Bernard Tipper", "Jack Janovec"}, 3));
    c.ev.push_back(Kill(119, "Ground Mortar 90 mm", "Ground Mortar 90 mm", "All-Knowing 60mm Mortar of Salvage", {"Jack Janovec"}, 7));
    c.ev.push_back(Kill(136, "Ground MG Nest Sand", "Ground MG Nest Sand", "All-Knowing 60mm Mortar of Salvage", {"Jack Janovec"}, 8));
    c.ev.push_back(Kill(152, "Ground MG Nest Sand", "Ground MG Nest Sand", "All-Knowing 60mm Mortar of Salvage", {}, 12));
    c.ev.push_back(Kill(170, "Ground AA", "Ground AA", "All-Knowing 60mm Mortar of Salvage", {"Jack Janovec"}, 9));
    c.ev.push_back(Kill(171, "Watch Tower", "Watch Tower", "All-Knowing 60mm Mortar of Salvage", {}, 13));
    c.ev.push_back(Kill(185, "Watch Tower", "Watch Tower", "All-Knowing 60mm Mortar of Salvage", {"Jack Janovec", "Bernard Tipper"}, 4));
    c.maxHp = 212; c.endHp = 212; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  b.kills.push_back({1, "Ground MG Nest Sand", "Ground MG Nest Sand", 97});
  b.kills.push_back({2, "Watch Tower", "Watch Tower", 107});
  b.kills.push_back({3, "Ground AA", "Ground AA", 110});
  b.kills.push_back({4, "Watch Tower", "Watch Tower", 185});
  b.kills.push_back({5, "Ground Mortar 90 mm", "Ground Mortar 90 mm", 79});
  b.kills.push_back({6, "Ground MG Nest Sand", "Ground MG Nest Sand", 104});
  b.kills.push_back({7, "Ground Mortar 90 mm", "Ground Mortar 90 mm", 119});
  b.kills.push_back({8, "Ground MG Nest Sand", "Ground MG Nest Sand", 136});
  b.kills.push_back({9, "Ground AA", "Ground AA", 170});
  b.kills.push_back({10, "Ground MG Nest Sand", "Ground MG Nest Sand", 93});
  b.kills.push_back({11, "Watch Tower", "Watch Tower", 94});
  b.kills.push_back({12, "Ground MG Nest Sand", "Ground MG Nest Sand", 152});
  b.kills.push_back({13, "Watch Tower", "Watch Tower", 171});
  return b;
}

// Not from the log: exercises the paths the three real battles never reach (down, up, death, fire, torpedo,
// a kill at the helm, missing, overflow). Only the invariants are checked.
static RepBattle BattleRake() {
  RepBattle b;
  b.len = 217;
  b.seed = SeedOf("2026-11-10 21:25");
  { RepBattle::Kill k; k.id = 0; k.enemy = k.enemyBase = "Japanese Tanker Sugar Baker Sugar"; k.t = 105; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 1; k.enemy = k.enemyBase = "Japanese Tanker Sugar Baker Sugar"; k.t = 106; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 2; k.enemy = k.enemyBase = "IJN Patrol T1"; k.t = 140; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 3; k.enemy = k.enemyBase = "IJN Patrol T1"; k.t = 148; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 4; k.enemy = k.enemyBase = "IJN Patrol T1"; k.t = 160; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 5; k.enemy = k.enemyBase = "No.1 Aux. Sub. Chaser"; k.t = 184; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 6; k.enemy = k.enemyBase = "IJN Patrol T1"; k.t = 199; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 7; k.enemy = k.enemyBase = "IJN Patrol T1"; k.t = 213; b.kills.push_back(k); }
  {
    RepCrew c;
    c.name = "George Thomason"; c.female = false; c.role = "passing ammunition";
    c.ev.push_back(Kill(148, "IJN Patrol T1", "IJN Patrol T1", "", {"Jack Janovec", "Bernard Tipper", "Myron U. Andrews"}, 3));
    c.grazes.push_back({134.0f, "Smol IJN Patrol T1", 1.33f});
    c.grazes.push_back({134.5f, "Smol IJN Patrol T1", 1.33f});
    c.grazes.push_back({135.0f, "Smol IJN Patrol T1", 1.33f});
    c.grazes.push_back({140.0f, "IJN Patrol T1", 1.00f});
    c.grazes.push_back({142.7f, "IJN Patrol T1", 1.00f});
    c.grazes.push_back({145.3f, "IJN Patrol T1", 1.00f});
    c.grazes.push_back({148.0f, "IJN Patrol T1", 1.00f});
    c.grazes.push_back({150.7f, "IJN Patrol T1", 1.00f});
    c.grazes.push_back({153.3f, "IJN Patrol T1", 1.00f});
    c.grazes.push_back({156.0f, "IJN Patrol T1", 1.00f});
    c.grazes.push_back({158.7f, "IJN Patrol T1", 1.00f});
    c.grazes.push_back({161.3f, "IJN Patrol T1", 1.00f});
    c.grazes.push_back({164.0f, "IJN Patrol T1", 1.00f});
    c.grazes.push_back({166.7f, "IJN Patrol T1", 1.00f});
    c.grazes.push_back({169.3f, "IJN Patrol T1", 1.00f});
    c.grazes.push_back({172.0f, "IJN Patrol T1", 1.00f});
    c.grazes.push_back({173.0f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({173.3f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({173.6f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({174.0f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({174.3f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({174.6f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({174.7f, "IJN Patrol T1", 1.00f});
    c.grazes.push_back({174.9f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({175.3f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({175.6f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({175.9f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({176.2f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({176.6f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({176.9f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({177.2f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({177.3f, "IJN Patrol T1", 1.00f});
    c.grazes.push_back({177.5f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({177.9f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({178.2f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({178.5f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({178.8f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({179.1f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({179.5f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({179.8f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({180.0f, "IJN Patrol T1", 1.00f});
    c.grazes.push_back({180.1f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({180.4f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({180.8f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({181.1f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({181.4f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({181.7f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({182.1f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({182.4f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({182.7f, "IJN Patrol T1", 1.00f});
    c.grazes.push_back({182.7f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({183.0f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({183.4f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({183.7f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({184.0f, "No.1 Aux. Sub. Chaser", 1.09f});
    c.grazes.push_back({185.3f, "IJN Patrol T1", 1.00f});
    c.grazes.push_back({188.0f, "IJN Patrol T1", 1.00f});
    c.grazes.push_back({190.7f, "IJN Patrol T1", 1.00f});
    c.grazes.push_back({193.3f, "IJN Patrol T1", 1.00f});
    c.grazes.push_back({196.0f, "IJN Patrol T1", 1.00f});
    c.maxHp = 232; c.endHp = 167; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Myron U. Andrews"; c.female = false; c.role = "passing ammunition";
    c.ev.push_back(Kill(148, "IJN Patrol T1", "IJN Patrol T1", "", {"Jack Janovec", "Bernard Tipper", "George Thomason"}, 3));
    c.grazes.push_back({132.0f, "Smol IJN Patrol T1", 1.00f});
    c.grazes.push_back({133.5f, "Smol IJN Patrol T1", 1.00f});
    c.grazes.push_back({135.0f, "Smol IJN Patrol T1", 1.00f});
    c.grazes.push_back({140.0f, "IJN Patrol T1", 1.27f});
    c.grazes.push_back({142.7f, "IJN Patrol T1", 1.27f});
    c.grazes.push_back({145.3f, "IJN Patrol T1", 1.27f});
    c.grazes.push_back({148.0f, "IJN Patrol T1", 1.27f});
    c.grazes.push_back({150.7f, "IJN Patrol T1", 1.27f});
    c.grazes.push_back({153.3f, "IJN Patrol T1", 1.27f});
    c.grazes.push_back({156.0f, "IJN Patrol T1", 1.27f});
    c.grazes.push_back({158.7f, "IJN Patrol T1", 1.27f});
    c.grazes.push_back({161.3f, "IJN Patrol T1", 1.27f});
    c.grazes.push_back({164.0f, "IJN Patrol T1", 1.27f});
    c.grazes.push_back({166.7f, "IJN Patrol T1", 1.27f});
    c.grazes.push_back({169.3f, "IJN Patrol T1", 1.27f});
    c.grazes.push_back({172.0f, "IJN Patrol T1", 1.27f});
    c.grazes.push_back({173.0f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({173.3f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({173.6f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({173.9f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({174.3f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({174.6f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({174.7f, "IJN Patrol T1", 1.27f});
    c.grazes.push_back({174.9f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({175.2f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({175.5f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({175.8f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({176.1f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({176.5f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({176.8f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({177.1f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({177.3f, "IJN Patrol T1", 1.27f});
    c.grazes.push_back({177.4f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({177.7f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({178.0f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({178.3f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({178.7f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({179.0f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({179.3f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({179.6f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({179.9f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({180.0f, "IJN Patrol T1", 1.27f});
    c.grazes.push_back({180.2f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({180.5f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({180.9f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({181.2f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({181.5f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({181.8f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({182.1f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({182.4f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({182.7f, "IJN Patrol T1", 1.27f});
    c.grazes.push_back({182.7f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({183.1f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({183.4f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({183.7f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({184.0f, "No.1 Aux. Sub. Chaser", 1.17f});
    c.grazes.push_back({185.3f, "IJN Patrol T1", 1.27f});
    c.grazes.push_back({188.0f, "IJN Patrol T1", 1.27f});
    c.grazes.push_back({190.7f, "IJN Patrol T1", 1.27f});
    c.grazes.push_back({193.3f, "IJN Patrol T1", 1.27f});
    c.grazes.push_back({196.0f, "IJN Patrol T1", 1.27f});
    c.maxHp = 232; c.endHp = 159; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Mae Cushman"; c.female = true; c.role = "repairing";
    c.grazes.push_back({132.0f, "Smol IJN Patrol T1", 0.00f});
    c.grazes.push_back({141.0f, "IJN Patrol T1", 1.25f});
    c.grazes.push_back({143.9f, "IJN Patrol T1", 1.25f});
    c.grazes.push_back({146.8f, "IJN Patrol T1", 1.25f});
    c.grazes.push_back({149.7f, "IJN Patrol T1", 1.25f});
    c.grazes.push_back({152.6f, "IJN Patrol T1", 1.25f});
    c.grazes.push_back({155.5f, "IJN Patrol T1", 1.25f});
    c.grazes.push_back({158.4f, "IJN Patrol T1", 1.25f});
    c.grazes.push_back({161.3f, "IJN Patrol T1", 1.25f});
    c.grazes.push_back({164.2f, "IJN Patrol T1", 1.25f});
    c.grazes.push_back({167.1f, "IJN Patrol T1", 1.25f});
    c.grazes.push_back({169.9f, "IJN Patrol T1", 1.25f});
    c.grazes.push_back({172.8f, "IJN Patrol T1", 1.25f});
    c.grazes.push_back({173.0f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({173.4f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({173.7f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({174.1f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({174.5f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({174.8f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({175.2f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({175.6f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({175.7f, "IJN Patrol T1", 1.25f});
    c.grazes.push_back({175.9f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({176.3f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({176.7f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({177.0f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({177.4f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({177.8f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({178.1f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({178.5f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({178.6f, "IJN Patrol T1", 1.25f});
    c.grazes.push_back({178.9f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({179.2f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({179.6f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({180.0f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({180.3f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({180.7f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({181.1f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({181.4f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({181.5f, "IJN Patrol T1", 1.25f});
    c.grazes.push_back({181.8f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({182.2f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({182.5f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({182.9f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({183.3f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({183.6f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({184.0f, "No.1 Aux. Sub. Chaser", 1.06f});
    c.grazes.push_back({184.4f, "IJN Patrol T1", 1.25f});
    c.grazes.push_back({187.3f, "IJN Patrol T1", 1.25f});
    c.grazes.push_back({190.2f, "IJN Patrol T1", 1.25f});
    c.grazes.push_back({193.1f, "IJN Patrol T1", 1.25f});
    c.grazes.push_back({196.0f, "IJN Patrol T1", 1.25f});
    c.maxHp = 232; c.endHp = 174; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Carwood Herron"; c.female = false; c.role = "repairing";
    c.grazes.push_back({132.0f, "Smol IJN Patrol T1", 1.00f});
    c.grazes.push_back({143.0f, "IJN Patrol T1", 1.11f});
    c.grazes.push_back({145.9f, "IJN Patrol T1", 1.11f});
    c.grazes.push_back({148.8f, "IJN Patrol T1", 1.11f});
    c.grazes.push_back({151.7f, "IJN Patrol T1", 1.11f});
    c.grazes.push_back({154.6f, "IJN Patrol T1", 1.11f});
    c.grazes.push_back({157.4f, "IJN Patrol T1", 1.11f});
    c.grazes.push_back({160.3f, "IJN Patrol T1", 1.11f});
    c.grazes.push_back({163.2f, "IJN Patrol T1", 1.11f});
    c.grazes.push_back({166.1f, "IJN Patrol T1", 1.11f});
    c.grazes.push_back({169.0f, "IJN Patrol T1", 1.11f});
    c.grazes.push_back({171.9f, "IJN Patrol T1", 1.11f});
    c.grazes.push_back({173.0f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({173.3f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({173.6f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({174.0f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({174.3f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({174.6f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({174.8f, "IJN Patrol T1", 1.11f});
    c.grazes.push_back({174.9f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({175.3f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({175.6f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({175.9f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({176.2f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({176.6f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({176.9f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({177.2f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({177.5f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({177.7f, "IJN Patrol T1", 1.11f});
    c.grazes.push_back({177.9f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({178.2f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({178.5f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({178.8f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({179.1f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({179.5f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({179.8f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({180.1f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({180.4f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({180.6f, "IJN Patrol T1", 1.11f});
    c.grazes.push_back({180.8f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({181.1f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({181.4f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({181.7f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({182.1f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({182.4f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({182.7f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({183.0f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({183.4f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({183.4f, "IJN Patrol T1", 1.11f});
    c.grazes.push_back({183.7f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({184.0f, "No.1 Aux. Sub. Chaser", 1.11f});
    c.grazes.push_back({186.3f, "IJN Patrol T1", 1.11f});
    c.grazes.push_back({189.2f, "IJN Patrol T1", 1.11f});
    c.grazes.push_back({192.1f, "IJN Patrol T1", 1.11f});
    c.grazes.push_back({195.0f, "IJN Patrol T1", 1.11f});
    c.maxHp = 232; c.endHp = 171; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Rosalind Dickerson"; c.female = true; c.role = "at the helm";
    c.ev.push_back(Kill(105, "Japanese Tanker Sugar Baker Sugar", "Japanese Tanker Sugar Baker Sugar", "", {}, 0));
    c.ev.push_back(Kill(106, "Japanese Tanker Sugar Baker Sugar", "Japanese Tanker Sugar Baker Sugar", "", {"Earl Davenport"}, 1));
    c.grazes.push_back({132.0f, "Smol IJN Patrol T1", 0.75f});
    c.grazes.push_back({133.0f, "Smol IJN Patrol T1", 0.75f});
    c.grazes.push_back({134.0f, "Smol IJN Patrol T1", 0.75f});
    c.grazes.push_back({135.0f, "Smol IJN Patrol T1", 0.75f});
    c.grazes.push_back({140.0f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({142.3f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({144.7f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({147.0f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({149.3f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({151.7f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({154.0f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({156.3f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({158.7f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({161.0f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({163.3f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({165.7f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({168.0f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({170.3f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({172.7f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({173.0f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({173.3f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({173.6f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({174.0f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({174.3f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({174.6f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({174.9f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({175.0f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({175.3f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({175.6f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({175.9f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({176.2f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({176.6f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({176.9f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({177.2f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({177.3f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({177.5f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({177.9f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({178.2f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({178.5f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({178.8f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({179.1f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({179.5f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({179.7f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({179.8f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({180.1f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({180.4f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({180.8f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({181.1f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({181.4f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({181.7f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({182.0f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({182.1f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({182.4f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({182.7f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({183.0f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({183.4f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({183.7f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({184.0f, "No.1 Aux. Sub. Chaser", 1.29f});
    c.grazes.push_back({184.3f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({186.7f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({189.0f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({191.3f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({193.7f, "IJN Patrol T1", 1.44f});
    c.grazes.push_back({196.0f, "IJN Patrol T1", 1.44f});
    c.maxHp = 232; c.endHp = 148; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Darrell Dominquez"; c.female = false; c.role = "tending the wounded";
    c.grazes.push_back({132.0f, "Smol IJN Patrol T1", 6.00f});
    c.grazes.push_back({132.0f, "Smol IJN Patrol T1", 6.00f});
    c.grazes.push_back({143.0f, "IJN Patrol T1", 1.19f});
    c.grazes.push_back({146.5f, "IJN Patrol T1", 1.19f});
    c.grazes.push_back({149.9f, "IJN Patrol T1", 1.19f});
    c.grazes.push_back({153.4f, "IJN Patrol T1", 1.19f});
    c.grazes.push_back({156.9f, "IJN Patrol T1", 1.19f});
    c.grazes.push_back({160.3f, "IJN Patrol T1", 1.19f});
    c.grazes.push_back({163.8f, "IJN Patrol T1", 1.19f});
    c.grazes.push_back({167.3f, "IJN Patrol T1", 1.19f});
    c.grazes.push_back({170.7f, "IJN Patrol T1", 1.19f});
    c.grazes.push_back({173.0f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({173.3f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({173.6f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({174.0f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({174.2f, "IJN Patrol T1", 1.19f});
    c.grazes.push_back({174.3f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({174.6f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({174.9f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({175.3f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({175.6f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({175.9f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({176.2f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({176.6f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({176.9f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({177.2f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({177.5f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({177.7f, "IJN Patrol T1", 1.19f});
    c.grazes.push_back({177.9f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({178.2f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({178.5f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({178.8f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({179.1f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({179.5f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({179.8f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({180.1f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({180.4f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({180.8f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({181.1f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({181.1f, "IJN Patrol T1", 1.19f});
    c.grazes.push_back({181.4f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({181.7f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({182.1f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({182.4f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({182.7f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({183.0f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({183.4f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({183.7f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({184.0f, "No.1 Aux. Sub. Chaser", 1.14f});
    c.grazes.push_back({184.6f, "IJN Patrol T1", 1.19f});
    c.grazes.push_back({188.1f, "IJN Patrol T1", 1.19f});
    c.grazes.push_back({191.5f, "IJN Patrol T1", 1.19f});
    c.grazes.push_back({195.0f, "IJN Patrol T1", 1.19f});
    c.maxHp = 232; c.endHp = 160; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Bernard Tipper"; c.female = false; c.role = "on the Ambushing Flak 38 of Ruin";
    c.gun = "Ambushing Flak 38 of Ruin";
    c.ev.push_back(Kill(140, "IJN Patrol T1", "IJN Patrol T1", "Ambushing Flak 38 of Ruin", {"Jack Janovec"}, 2));
    c.ev.push_back(Kill(148, "IJN Patrol T1", "IJN Patrol T1", "Ambushing Flak 38 of Ruin", {"Jack Janovec", "George Thomason", "Myron U. Andrews"}, 3));
    c.ev.push_back(Kill(160, "IJN Patrol T1", "IJN Patrol T1", "Ambushing Flak 38 of Ruin", {"Jack Janovec"}, 4));
    c.ev.push_back(Kill(184, "No.1 Aux. Sub. Chaser", "No.1 Aux. Sub. Chaser", "Ambushing Flak 38 of Ruin", {"Earl Davenport", "Jack Janovec"}, 5));
    c.ev.push_back(Kill(213, "IJN Patrol T1", "IJN Patrol T1", "Ambushing Flak 38 of Ruin", {"Jack Janovec"}, 7));
    c.grazes.push_back({132.0f, "Smol IJN Patrol T1", 0.67f});
    c.grazes.push_back({133.5f, "Smol IJN Patrol T1", 0.67f});
    c.grazes.push_back({135.0f, "Smol IJN Patrol T1", 0.67f});
    c.grazes.push_back({141.0f, "IJN Patrol T1", 1.48f});
    c.grazes.push_back({143.8f, "IJN Patrol T1", 1.48f});
    c.grazes.push_back({146.5f, "IJN Patrol T1", 1.48f});
    c.grazes.push_back({149.2f, "IJN Patrol T1", 1.48f});
    c.grazes.push_back({152.0f, "IJN Patrol T1", 1.48f});
    c.grazes.push_back({154.8f, "IJN Patrol T1", 1.48f});
    c.grazes.push_back({157.5f, "IJN Patrol T1", 1.48f});
    c.grazes.push_back({160.2f, "IJN Patrol T1", 1.48f});
    c.grazes.push_back({163.0f, "IJN Patrol T1", 1.48f});
    c.grazes.push_back({165.8f, "IJN Patrol T1", 1.48f});
    c.grazes.push_back({168.5f, "IJN Patrol T1", 1.48f});
    c.grazes.push_back({171.2f, "IJN Patrol T1", 1.48f});
    c.grazes.push_back({173.0f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({173.4f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({173.7f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({174.0f, "IJN Patrol T1", 1.48f});
    c.grazes.push_back({174.1f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({174.5f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({174.8f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({175.2f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({175.6f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({175.9f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({176.3f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({176.7f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({176.8f, "IJN Patrol T1", 1.48f});
    c.grazes.push_back({177.0f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({177.4f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({177.8f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({178.1f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({178.5f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({178.9f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({179.2f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({179.5f, "IJN Patrol T1", 1.48f});
    c.grazes.push_back({179.6f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({180.0f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({180.3f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({180.7f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({181.1f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({181.4f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({181.8f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({182.2f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({182.2f, "IJN Patrol T1", 1.48f});
    c.grazes.push_back({182.5f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({182.9f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({183.3f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({183.6f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({184.0f, "No.1 Aux. Sub. Chaser", 1.03f});
    c.grazes.push_back({185.0f, "IJN Patrol T1", 1.48f});
    c.grazes.push_back({187.8f, "IJN Patrol T1", 1.48f});
    c.grazes.push_back({190.5f, "IJN Patrol T1", 1.48f});
    c.grazes.push_back({193.2f, "IJN Patrol T1", 1.48f});
    c.grazes.push_back({196.0f, "IJN Patrol T1", 1.48f});
    c.maxHp = 232; c.endHp = 167; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Jack Janovec"; c.female = false; c.role = "on the Rallying Barge Buster of Economy";
    c.gun = "Rallying Barge Buster of Economy";
    c.gunBase = "20 mm Oerlikon";
    c.ev.push_back(Kill(140, "IJN Patrol T1", "IJN Patrol T1", "Rallying Barge Buster of Economy", {"Bernard Tipper"}, 2));
    c.ev.push_back(Kill(148, "IJN Patrol T1", "IJN Patrol T1", "Rallying Barge Buster of Economy", {"Bernard Tipper", "George Thomason", "Myron U. Andrews"}, 3));
    c.ev.push_back(Kill(160, "IJN Patrol T1", "IJN Patrol T1", "Rallying Barge Buster of Economy", {"Bernard Tipper"}, 4));
    c.ev.push_back(Kill(184, "No.1 Aux. Sub. Chaser", "No.1 Aux. Sub. Chaser", "Rallying Barge Buster of Economy", {"Earl Davenport", "Bernard Tipper"}, 5));
    c.ev.push_back(Kill(199, "IJN Patrol T1", "IJN Patrol T1", "Rallying Barge Buster of Economy", {}, 6));
    c.ev.push_back(Kill(213, "IJN Patrol T1", "IJN Patrol T1", "Rallying Barge Buster of Economy", {"Bernard Tipper"}, 7));
    c.grazes.push_back({132.0f, "Smol IJN Patrol T1", 0.67f});
    c.grazes.push_back({133.5f, "Smol IJN Patrol T1", 0.67f});
    c.grazes.push_back({135.0f, "Smol IJN Patrol T1", 0.67f});
    c.grazes.push_back({141.0f, "IJN Patrol T1", 1.40f});
    c.grazes.push_back({143.9f, "IJN Patrol T1", 1.40f});
    c.grazes.push_back({146.8f, "IJN Patrol T1", 1.40f});
    c.grazes.push_back({149.7f, "IJN Patrol T1", 1.40f});
    c.grazes.push_back({152.6f, "IJN Patrol T1", 1.40f});
    c.grazes.push_back({155.5f, "IJN Patrol T1", 1.40f});
    c.grazes.push_back({158.4f, "IJN Patrol T1", 1.40f});
    c.grazes.push_back({161.3f, "IJN Patrol T1", 1.40f});
    c.grazes.push_back({164.2f, "IJN Patrol T1", 1.40f});
    c.grazes.push_back({167.1f, "IJN Patrol T1", 1.40f});
    c.grazes.push_back({169.9f, "IJN Patrol T1", 1.40f});
    c.grazes.push_back({172.8f, "IJN Patrol T1", 1.40f});
    c.grazes.push_back({173.0f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({173.4f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({173.8f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({174.1f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({174.5f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({174.9f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({175.3f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({175.7f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({175.7f, "IJN Patrol T1", 1.40f});
    c.grazes.push_back({176.0f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({176.4f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({176.8f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({177.2f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({177.6f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({177.9f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({178.3f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({178.6f, "IJN Patrol T1", 1.40f});
    c.grazes.push_back({178.7f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({179.1f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({179.4f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({179.8f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({180.2f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({180.6f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({181.0f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({181.3f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({181.5f, "IJN Patrol T1", 1.40f});
    c.grazes.push_back({181.7f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({182.1f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({182.5f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({182.9f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({183.2f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({183.6f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({184.0f, "No.1 Aux. Sub. Chaser", 1.10f});
    c.grazes.push_back({184.4f, "IJN Patrol T1", 1.40f});
    c.grazes.push_back({187.3f, "IJN Patrol T1", 1.40f});
    c.grazes.push_back({190.2f, "IJN Patrol T1", 1.40f});
    c.grazes.push_back({193.1f, "IJN Patrol T1", 1.40f});
    c.grazes.push_back({196.0f, "IJN Patrol T1", 1.40f});
    c.maxHp = 232; c.endHp = 168; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Earl Davenport"; c.female = false; c.role = "on the 81mm Mortar of Cheapness";
    c.gun = "81mm Mortar of Cheapness";
    c.ev.push_back(Kill(106, "Japanese Tanker Sugar Baker Sugar", "Japanese Tanker Sugar Baker Sugar", "81mm Mortar of Cheapness", {"Rosalind Dickerson"}, 1));
    c.ev.push_back(St(181, S_WOUNDED));
    c.ev.push_back(Kill(184, "No.1 Aux. Sub. Chaser", "No.1 Aux. Sub. Chaser", "81mm Mortar of Cheapness", {"Bernard Tipper", "Jack Janovec"}, 5));
    c.grazes.push_back({134.0f, "Smol IJN Patrol T1", 1.33f});
    c.grazes.push_back({134.5f, "Smol IJN Patrol T1", 1.33f});
    c.grazes.push_back({135.0f, "Smol IJN Patrol T1", 1.33f});
    c.grazes.push_back({140.0f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({142.2f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({144.4f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({146.6f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({148.8f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({151.0f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({153.2f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({155.4f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({157.6f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({159.8f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({162.0f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({164.2f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({166.4f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({168.6f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({170.8f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({173.0f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({174.0f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({174.2f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({174.5f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({174.7f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({175.0f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({175.2f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({175.2f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({175.5f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({175.7f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({176.0f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({176.2f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({176.4f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({176.7f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({176.9f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({177.2f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({177.4f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({177.4f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({177.7f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({177.9f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({178.1f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({178.4f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({178.6f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({178.9f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({179.1f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({179.4f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({179.6f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({179.6f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({179.9f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({180.1f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({180.3f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({180.6f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({180.8f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({181.1f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({181.3f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({181.6f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({181.8f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({181.8f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({182.0f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({182.3f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({182.5f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({182.8f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({183.0f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({183.3f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({183.5f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({183.8f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({184.0f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({184.0f, "No.1 Aux. Sub. Chaser", 1.64f});
    c.grazes.push_back({186.2f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({188.4f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({190.6f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({192.8f, "IJN Patrol T1", 1.08f});
    c.grazes.push_back({195.0f, "IJN Patrol T1", 1.08f});
    c.maxHp = 232; c.endHp = 226; c.outcome = O_WOUNDED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Vincent Randleman"; c.female = false; c.role = "tending the wounded";
    c.grazes.push_back({132.0f, "Smol IJN Patrol T1", 0.00f});
    c.grazes.push_back({143.0f, "IJN Patrol T1", 1.55f});
    c.grazes.push_back({145.5f, "IJN Patrol T1", 1.55f});
    c.grazes.push_back({148.0f, "IJN Patrol T1", 1.55f});
    c.grazes.push_back({150.4f, "IJN Patrol T1", 1.55f});
    c.grazes.push_back({152.9f, "IJN Patrol T1", 1.55f});
    c.grazes.push_back({155.4f, "IJN Patrol T1", 1.55f});
    c.grazes.push_back({157.9f, "IJN Patrol T1", 1.55f});
    c.grazes.push_back({160.3f, "IJN Patrol T1", 1.55f});
    c.grazes.push_back({162.8f, "IJN Patrol T1", 1.55f});
    c.grazes.push_back({165.3f, "IJN Patrol T1", 1.55f});
    c.grazes.push_back({167.8f, "IJN Patrol T1", 1.55f});
    c.grazes.push_back({170.2f, "IJN Patrol T1", 1.55f});
    c.grazes.push_back({172.7f, "IJN Patrol T1", 1.55f});
    c.grazes.push_back({173.0f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({173.3f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({173.7f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({174.0f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({174.3f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({174.7f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({175.0f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({175.2f, "IJN Patrol T1", 1.55f});
    c.grazes.push_back({175.3f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({175.7f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({176.0f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({176.3f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({176.7f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({177.0f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({177.3f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({177.7f, "IJN Patrol T1", 1.55f});
    c.grazes.push_back({177.7f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({178.0f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({178.3f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({178.7f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({179.0f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({179.3f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({179.7f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({180.0f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({180.1f, "IJN Patrol T1", 1.55f});
    c.grazes.push_back({180.3f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({180.7f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({181.0f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({181.3f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({181.7f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({182.0f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({182.3f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({182.6f, "IJN Patrol T1", 1.55f});
    c.grazes.push_back({182.7f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({183.0f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({183.3f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({183.7f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({184.0f, "No.1 Aux. Sub. Chaser", 1.12f});
    c.grazes.push_back({185.1f, "IJN Patrol T1", 1.55f});
    c.grazes.push_back({187.6f, "IJN Patrol T1", 1.55f});
    c.grazes.push_back({190.0f, "IJN Patrol T1", 1.55f});
    c.grazes.push_back({192.5f, "IJN Patrol T1", 1.55f});
    c.grazes.push_back({195.0f, "IJN Patrol T1", 1.55f});
    c.maxHp = 232; c.endHp = 160; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  return b;
}
static RepBattle BattleSink() {
  RepBattle b;
  b.len = 220;
  b.seed = SeedOf("2026-11-10 22:22");
  { RepBattle::Kill k; k.id = 0; k.enemy = k.enemyBase = "No.101 Aux. Minesweeper"; k.t = 87; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 1; k.enemy = k.enemyBase = "E8N"; k.t = 116; b.kills.push_back(k); }
  {
    RepCrew c;
    c.name = "George Thomason"; c.female = false; c.role = "passing ammunition";
    c.ev.push_back(St(131, S_SIDE));
    c.ev.push_back(St(220, S_PICKED));
    c.maxHp = 230; c.endHp = 230; c.outcome = O_LOST_PICKED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Myron U. Andrews"; c.female = false; c.role = "passing ammunition";
    c.ev.push_back(St(131, S_SIDE));
    c.ev.push_back(St(220, S_PICKED));
    c.maxHp = 230; c.endHp = 230; c.outcome = O_LOST_PICKED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Mae Cushman"; c.female = true; c.role = "repairing";
    c.ev.push_back(St(131, S_SIDE));
    c.ev.push_back(St(220, S_PICKED));
    c.maxHp = 230; c.endHp = 230; c.outcome = O_LOST_PICKED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Carwood Herron"; c.female = false; c.role = "repairing";
    c.ev.push_back(St(131, S_SANK));
    c.maxHp = 230; c.endHp = 230; c.outcome = O_MISSING;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Rosalind Dickerson"; c.female = true; c.role = "at the helm";
    c.ev.push_back(Kill(87, "No.101 Aux. Minesweeper", "No.101 Aux. Minesweeper", "", {}, 0));
    c.ev.push_back(St(131, S_SANK));
    c.ev.push_back(St(220, S_PICKED));
    c.maxHp = 230; c.endHp = 230; c.outcome = O_LOST_PICKED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Darrell Dominquez"; c.female = false; c.role = "tending the wounded";
    c.ev.push_back(St(131, S_SANK));
    c.ev.push_back(St(220, S_PICKED));
    c.maxHp = 230; c.endHp = 230; c.outcome = O_LOST_PICKED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Bernard Tipper"; c.female = false; c.role = "on the Vampiric Vickers K of Ruin";
    c.gun = "Vampiric Vickers K of Ruin";
    c.ev.push_back(St(131, S_SIDE));
    c.ev.push_back(St(220, S_PICKED));
    c.maxHp = 230; c.endHp = 230; c.outcome = O_LOST_PICKED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Jack Janovec"; c.female = false; c.role = "on the Barge Buster of Fire";
    c.gun = "Barge Buster of Fire";
    c.ev.push_back(Kill(116, "E8N", "E8N", "Barge Buster of Fire", {}, 1));
    c.ev.push_back(St(131, S_SANK));
    c.ev.push_back(St(220, S_PICKED));
    c.maxHp = 230; c.endHp = 230; c.outcome = O_LOST_PICKED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Earl Davenport"; c.female = false; c.role = "on the QF 6-pdr Molins";
    c.gun = "QF 6-pdr Molins";
    c.ev.push_back(St(131, S_SIDE));
    c.ev.push_back(St(220, S_PICKED));
    c.maxHp = 230; c.endHp = 230; c.outcome = O_LOST_PICKED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Vincent Randleman"; c.female = false; c.role = "on the QF 6-pdr Molins";
    c.gun = "QF 6-pdr Molins";
    c.ev.push_back(St(131, S_SIDE));
    c.maxHp = 230; c.endHp = 230; c.outcome = O_MISSING;
    b.crew.push_back(c);
  }
  return b;
}
static RepBattle BattleShore() {
  RepBattle b;
  b.len = 138;
  b.seed = SeedOf("2026-11-11 12:05");
  { RepBattle::Kill k; k.id = 0; k.enemy = k.enemyBase = "Watch Tower"; k.t = 70; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 1; k.enemy = k.enemyBase = "Watch Tower"; k.t = 75; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 2; k.enemy = k.enemyBase = "Ground MG Nest Sand"; k.t = 77; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 3; k.enemy = k.enemyBase = "Watch Tower"; k.t = 111; b.kills.push_back(k); }
  {
    RepCrew c;
    c.name = "George Thomason"; c.female = false; c.role = "passing ammunition";
    c.grazes.push_back({77.0f, "Ground MG Nest Sand", 1.00f});
    c.grazes.push_back({78.0f, "Ground MG Nest Sand", 1.00f});
    c.maxHp = 232; c.endHp = 230; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Myron U. Andrews"; c.female = false; c.role = "passing ammunition";
    c.grazes.push_back({77.0f, "Ground MG Nest Sand", 0.00f});
    c.maxHp = 232; c.endHp = 232; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Mae Cushman"; c.female = true; c.role = "repairing";
    c.maxHp = 232; c.endHp = 232; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Carwood Herron"; c.female = false; c.role = "repairing";
    c.maxHp = 232; c.endHp = 232; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Rosalind Dickerson"; c.female = true; c.role = "at the helm";
    c.grazes.push_back({77.0f, "Ground MG Nest Sand", 1.00f});
    c.maxHp = 232; c.endHp = 231; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Darrell Dominquez"; c.female = false; c.role = "tending the wounded";
    c.maxHp = 232; c.endHp = 232; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Bernard Tipper"; c.female = false; c.role = "on the Vampiric Vickers K of Ruin";
    c.gun = "Vampiric Vickers K of Ruin";
    c.ev.push_back(Kill(77, "Ground MG Nest Sand", "Ground MG Nest Sand", "Vampiric Vickers K of Ruin", {"Earl Davenport"}, 2));
    c.ev.push_back(Kill(111, "Watch Tower", "Watch Tower", "Vampiric Vickers K of Ruin", {"Vincent Randleman"}, 3));
    c.grazes.push_back({77.0f, "Ground MG Nest Sand", 0.00f});
    c.maxHp = 232; c.endHp = 232; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Jack Janovec"; c.female = false; c.role = "on the Homing Oerlikon of the Long Shot";
    c.gun = "Homing Oerlikon of the Long Shot";
    c.gunBase = "20 mm Oerlikon";
    c.ev.push_back(Kill(70, "Watch Tower", "Watch Tower", "Homing Oerlikon of the Long Shot", {"Vincent Randleman"}, 0));
    c.ev.push_back(Kill(75, "Watch Tower", "Watch Tower", "Homing Oerlikon of the Long Shot", {"Vincent Randleman"}, 1));
    c.maxHp = 232; c.endHp = 232; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Earl Davenport"; c.female = false; c.role = "on the Vampiric Chopper of Giants";
    c.gun = "Vampiric Chopper of Giants";
    c.ev.push_back(Kill(77, "Ground MG Nest Sand", "Ground MG Nest Sand", "Vampiric Chopper of Giants", {"Bernard Tipper"}, 2));
    c.maxHp = 232; c.endHp = 232; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Vincent Randleman"; c.female = false; c.role = "on the Rallying Tub 20mm";
    c.gun = "Rallying Tub 20mm";
    c.ev.push_back(Kill(70, "Watch Tower", "Watch Tower", "Rallying Tub 20mm", {"Jack Janovec"}, 0));
    c.ev.push_back(Kill(75, "Watch Tower", "Watch Tower", "Rallying Tub 20mm", {"Jack Janovec"}, 1));
    c.ev.push_back(Kill(111, "Watch Tower", "Watch Tower", "Rallying Tub 20mm", {"Bernard Tipper"}, 3));
    c.grazes.push_back({77.0f, "Ground MG Nest Sand", 1.50f});
    c.grazes.push_back({78.0f, "Ground MG Nest Sand", 1.50f});
    c.maxHp = 232; c.endHp = 229; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  return b;
}
static RepBattle BattleAA() {
  RepBattle b;
  b.len = 278;
  b.seed = SeedOf("2026-11-11 12:10");
  { RepBattle::Kill k; k.id = 0; k.enemy = k.enemyBase = "K7 Subchaser"; k.t = 53; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 1; k.enemy = k.enemyBase = "Coastal Bombard"; k.t = 84; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 2; k.enemy = k.enemyBase = "K7 Subchaser"; k.t = 122; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 3; k.enemy = k.enemyBase = "Soukou-Tei"; k.t = 133; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 4; k.enemy = k.enemyBase = "Soukou-Tei"; k.t = 142; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 5; k.enemy = k.enemyBase = "Soukou-Tei"; k.t = 147; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 6; k.enemy = k.enemyBase = "Soukou-Tei"; k.t = 159; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 7; k.enemy = k.enemyBase = "IJN Patrol T1"; k.t = 165; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 8; k.enemy = k.enemyBase = "Soukou-Tei"; k.t = 170; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 9; k.enemy = k.enemyBase = "IJN Patrol T1"; k.t = 171; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 10; k.enemy = k.enemyBase = "Soukou-Tei"; k.t = 177; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 11; k.enemy = k.enemyBase = "Coastal Bombard"; k.t = 182; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 12; k.enemy = k.enemyBase = "IJN Patrol T1"; k.t = 199; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 13; k.enemy = k.enemyBase = "Watch Tower"; k.t = 230; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 14; k.enemy = k.enemyBase = "Ground AA"; k.t = 233; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 15; k.enemy = k.enemyBase = "Watch Tower"; k.t = 243; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 16; k.enemy = k.enemyBase = "Ground AA"; k.t = 245; b.kills.push_back(k); }
  { RepBattle::Kill k; k.id = 17; k.enemy = k.enemyBase = "Ground MG Nest Sand"; k.t = 274; b.kills.push_back(k); }
  {
    RepCrew c;
    c.name = "George Thomason"; c.female = false; c.role = "passing ammunition";
    c.grazes.push_back({244.0f, "Ground AA", 2.50f});
    c.grazes.push_back({246.0f, "Ground AA", 2.50f});
    c.maxHp = 232; c.endHp = 227; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Myron U. Andrews"; c.female = false; c.role = "passing ammunition";
    c.grazes.push_back({244.0f, "Ground AA", 3.00f});
    c.maxHp = 232; c.endHp = 229; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Mae Cushman"; c.female = true; c.role = "repairing";
    c.grazes.push_back({244.0f, "Ground AA", 2.00f});
    c.grazes.push_back({245.0f, "Ground AA", 2.00f});
    c.grazes.push_back({246.0f, "Ground AA", 2.00f});
    c.maxHp = 232; c.endHp = 226; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Carwood Herron"; c.female = false; c.role = "repairing";
    c.grazes.push_back({245.0f, "Ground AA", 1.00f});
    c.grazes.push_back({246.0f, "Ground AA", 1.00f});
    c.maxHp = 232; c.endHp = 230; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Rosalind Dickerson"; c.female = true; c.role = "at the helm";
    c.ev.push_back(Kill(53, "K7 Subchaser", "K7 Subchaser", "", {"Vincent Randleman", "Earl Davenport", "Jack Janovec"}, 0));
    c.grazes.push_back({244.0f, "Ground AA", 2.67f});
    c.grazes.push_back({245.0f, "Ground AA", 2.67f});
    c.grazes.push_back({246.0f, "Ground AA", 2.67f});
    c.maxHp = 232; c.endHp = 224; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Darrell Dominquez"; c.female = false; c.role = "tending the wounded";
    c.grazes.push_back({245.0f, "Ground AA", 0.00f});
    c.maxHp = 232; c.endHp = 232; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Bernard Tipper"; c.female = false; c.role = "on the Vampiric Vickers K of Ruin";
    c.gun = "Vampiric Vickers K of Ruin";
    c.ev.push_back(Kill(170, "Soukou-Tei", "Soukou-Tei", "Vampiric Vickers K of Ruin", {"Jack Janovec", "Earl Davenport"}, 8));
    c.grazes.push_back({244.0f, "Ground AA", 3.00f});
    c.grazes.push_back({245.0f, "Ground AA", 3.00f});
    c.grazes.push_back({246.0f, "Ground AA", 3.00f});
    c.maxHp = 232; c.endHp = 223; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Jack Janovec"; c.female = false; c.role = "on the Homing Oerlikon of the Long Shot";
    c.gun = "Homing Oerlikon of the Long Shot";
    c.gunBase = "20 mm Oerlikon";
    c.ev.push_back(Kill(53, "K7 Subchaser", "K7 Subchaser", "Homing Oerlikon of the Long Shot", {"Vincent Randleman", "Earl Davenport", "Rosalind Dickerson"}, 0));
    c.ev.push_back(Kill(84, "Coastal Bombard", "Coastal Bombard", "Homing Oerlikon of the Long Shot", {"Earl Davenport", "Vincent Randleman"}, 1));
    c.ev.push_back(Kill(122, "K7 Subchaser", "K7 Subchaser", "Homing Oerlikon of the Long Shot", {"Earl Davenport", "Vincent Randleman"}, 2));
    c.ev.push_back(Kill(133, "Soukou-Tei", "Soukou-Tei", "Homing Oerlikon of the Long Shot", {"Vincent Randleman"}, 3));
    c.ev.push_back(Kill(142, "Soukou-Tei", "Soukou-Tei", "Homing Oerlikon of the Long Shot", {"Earl Davenport"}, 4));
    c.ev.push_back(Kill(147, "Soukou-Tei", "Soukou-Tei", "Homing Oerlikon of the Long Shot", {"Vincent Randleman"}, 5));
    c.ev.push_back(Kill(159, "Soukou-Tei", "Soukou-Tei", "Homing Oerlikon of the Long Shot", {"Earl Davenport"}, 6));
    c.ev.push_back(Kill(170, "Soukou-Tei", "Soukou-Tei", "Homing Oerlikon of the Long Shot", {"Bernard Tipper", "Earl Davenport"}, 8));
    c.ev.push_back(Kill(171, "IJN Patrol T1", "IJN Patrol T1", "Homing Oerlikon of the Long Shot", {"Earl Davenport"}, 9));
    c.ev.push_back(Kill(177, "Soukou-Tei", "Soukou-Tei", "Homing Oerlikon of the Long Shot", {"Vincent Randleman"}, 10));
    c.ev.push_back(Kill(199, "IJN Patrol T1", "IJN Patrol T1", "Homing Oerlikon of the Long Shot", {}, 12));
    c.ev.push_back(Kill(199, "IJN Patrol T1", "IJN Patrol T1", "Homing Oerlikon of the Long Shot", {"Earl Davenport"}, 12));
    c.ev.push_back(Kill(243, "Watch Tower", "Watch Tower", "Homing Oerlikon of the Long Shot", {"Vincent Randleman", "Earl Davenport"}, 15));
    c.ev.push_back(Kill(274, "Ground MG Nest Sand", "Ground MG Nest Sand", "Homing Oerlikon of the Long Shot", {"Vincent Randleman", "Earl Davenport"}, 17));
    c.grazes.push_back({244.0f, "Ground AA", 2.00f});
    c.grazes.push_back({245.0f, "Ground AA", 2.00f});
    c.grazes.push_back({246.0f, "Ground AA", 2.00f});
    c.maxHp = 232; c.endHp = 226; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Earl Davenport"; c.female = false; c.role = "on the Vampiric Chopper of Giants";
    c.gun = "Vampiric Chopper of Giants";
    c.ev.push_back(Kill(53, "K7 Subchaser", "K7 Subchaser", "Vampiric Chopper of Giants", {"Vincent Randleman", "Jack Janovec", "Rosalind Dickerson"}, 0));
    c.ev.push_back(Kill(84, "Coastal Bombard", "Coastal Bombard", "Vampiric Chopper of Giants", {"Vincent Randleman", "Jack Janovec"}, 1));
    c.ev.push_back(Kill(122, "K7 Subchaser", "K7 Subchaser", "Vampiric Chopper of Giants", {"Vincent Randleman", "Jack Janovec"}, 2));
    c.ev.push_back(Kill(142, "Soukou-Tei", "Soukou-Tei", "Vampiric Chopper of Giants", {"Jack Janovec"}, 4));
    c.ev.push_back(Kill(159, "Soukou-Tei", "Soukou-Tei", "Vampiric Chopper of Giants", {"Jack Janovec"}, 6));
    c.ev.push_back(Kill(165, "IJN Patrol T1", "IJN Patrol T1", "Vampiric Chopper of Giants", {"Vincent Randleman"}, 7));
    c.ev.push_back(Kill(170, "Soukou-Tei", "Soukou-Tei", "Vampiric Chopper of Giants", {"Jack Janovec", "Bernard Tipper"}, 8));
    c.ev.push_back(Kill(171, "IJN Patrol T1", "IJN Patrol T1", "Vampiric Chopper of Giants", {"Jack Janovec"}, 9));
    c.ev.push_back(Kill(182, "Coastal Bombard", "Coastal Bombard", "Vampiric Chopper of Giants", {}, 11));
    c.ev.push_back(Kill(199, "IJN Patrol T1", "IJN Patrol T1", "Vampiric Chopper of Giants", {"Jack Janovec"}, 12));
    c.ev.push_back(Kill(233, "Ground AA", "Ground AA", "Vampiric Chopper of Giants", {}, 14));
    c.ev.push_back(Kill(243, "Watch Tower", "Watch Tower", "Vampiric Chopper of Giants", {"Jack Janovec", "Vincent Randleman"}, 15));
    c.ev.push_back(Kill(245, "Ground AA", "Ground AA", "Vampiric Chopper of Giants", {}, 16));
    c.ev.push_back(Kill(274, "Ground MG Nest Sand", "Ground MG Nest Sand", "Vampiric Chopper of Giants", {"Jack Janovec", "Vincent Randleman"}, 17));
    c.maxHp = 232; c.endHp = 232; c.outcome = O_UNHURT;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Vincent Randleman"; c.female = false; c.role = "on the Rallying Tub 20mm";
    c.gun = "Rallying Tub 20mm";
    c.ev.push_back(Kill(53, "K7 Subchaser", "K7 Subchaser", "Rallying Tub 20mm", {"Earl Davenport", "Jack Janovec", "Rosalind Dickerson"}, 0));
    c.ev.push_back(Kill(84, "Coastal Bombard", "Coastal Bombard", "Rallying Tub 20mm", {"Earl Davenport", "Jack Janovec"}, 1));
    c.ev.push_back(Kill(122, "K7 Subchaser", "K7 Subchaser", "Rallying Tub 20mm", {"Earl Davenport", "Jack Janovec"}, 2));
    c.ev.push_back(Kill(133, "Soukou-Tei", "Soukou-Tei", "Rallying Tub 20mm", {"Jack Janovec"}, 3));
    c.ev.push_back(Kill(147, "Soukou-Tei", "Soukou-Tei", "Rallying Tub 20mm", {"Jack Janovec"}, 5));
    c.ev.push_back(Kill(165, "IJN Patrol T1", "IJN Patrol T1", "Rallying Tub 20mm", {"Earl Davenport"}, 7));
    c.ev.push_back(Kill(177, "Soukou-Tei", "Soukou-Tei", "Rallying Tub 20mm", {"Jack Janovec"}, 10));
    c.ev.push_back(Kill(230, "Watch Tower", "Watch Tower", "Rallying Tub 20mm", {}, 13));
    c.ev.push_back(Kill(243, "Watch Tower", "Watch Tower", "Rallying Tub 20mm", {"Jack Janovec", "Earl Davenport"}, 15));
    c.ev.push_back(Kill(274, "Ground MG Nest Sand", "Ground MG Nest Sand", "Rallying Tub 20mm", {"Jack Janovec", "Earl Davenport"}, 17));
    c.grazes.push_back({245.0f, "Ground AA", 0.00f});
    c.maxHp = 232; c.endHp = 232; c.outcome = O_LIGHT;
    b.crew.push_back(c);
  }
  return b;
}
static RepBattle BattleNagara() {
  RepBattle b;
  b.len = 487;
  b.seed = SeedOf("2026-11-11 17:37");
  { RepBattle::Kill k; k.id = 0; k.enemy = k.enemyBase = "Tanker"; k.t = 107; b.kills.push_back(k); }
  {
    RepCrew c;
    c.name = "George Thomason"; c.female = false; c.role = "passing ammunition"; c.maxHp = 230;
    c.ev.push_back(Hit(234, R_CLOSE, 1, "the right leg", "", "Nagara Cruiser", "Nagara Cruiser", "140 mm Gun", "", true, 15, 200));
    c.ev.push_back(Hit(242, R_CLOSE, 1, "the left leg", "", "Nagara Cruiser", "Nagara Cruiser", "140 mm Gun", "", true, 15, 185));
    c.ev.push_back(Hit(250, R_CLOSE, 1, "the back of the head", "", "Nagara Cruiser", "Nagara Cruiser", "140 mm Gun", "", true, 15, 170));
    c.ev.push_back(St(300, S_SIDE));
    c.ev.push_back(St(420, S_PICKED));
    for (int k = 0; k < 60; ++k) c.grazes.push_back({200.0f + k * 2, "Nagara Cruiser", 1.4f});
    for (int k = 0; k < 377; ++k) c.grazes.push_back({271.0f + k * 0.01f, "fire", 0.03f});
    std::sort(c.grazes.begin(), c.grazes.end(), [](const RepGraze& a, const RepGraze& z) { return a.t < z.t; });
    c.endHp = 80; c.outcome = O_LOST_PICKED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Myron U. Andrews"; c.female = false; c.role = "passing ammunition"; c.maxHp = 230;
    c.ev.push_back(Hit(234, R_CLOSE, 1, "the left leg", "", "Nagara Cruiser", "Nagara Cruiser", "140 mm Gun", "", true, 15, 200));
    c.ev.push_back(Hit(242, R_CLOSE, 1, "the left leg", "", "Nagara Cruiser", "Nagara Cruiser", "140 mm Gun", "", true, 15, 185));
    c.ev.push_back(Hit(250, R_CLOSE, 1, "the back of the head", "", "Nagara Cruiser", "Nagara Cruiser", "140 mm Gun", "", true, 15, 170));
    c.ev.push_back(St(305, S_SIDE));
    c.ev.push_back(St(423, S_PICKED));
    for (int k = 0; k < 60; ++k) c.grazes.push_back({200.0f + k * 2, "Nagara Cruiser", 1.4f});
    for (int k = 0; k < 377; ++k) c.grazes.push_back({271.0f + k * 0.01f, "fire", 0.03f});
    std::sort(c.grazes.begin(), c.grazes.end(), [](const RepGraze& a, const RepGraze& z) { return a.t < z.t; });
    c.endHp = 80; c.outcome = O_LOST_PICKED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Mae Cushman"; c.female = true; c.role = "repairing"; c.maxHp = 230;
    c.ev.push_back(Hit(234, R_CLOSE, 1, "the right leg", "", "Nagara Cruiser", "Nagara Cruiser", "140 mm Gun", "", true, 15, 200));
    c.ev.push_back(Hit(242, R_CLOSE, 1, "the left leg", "", "Nagara Cruiser", "Nagara Cruiser", "140 mm Gun", "", true, 15, 185));
    c.ev.push_back(Hit(250, R_CLOSE, 1, "the left leg", "", "Nagara Cruiser", "Nagara Cruiser", "140 mm Gun", "", true, 15, 170));
    c.ev.push_back(St(310, S_SIDE));
    c.ev.push_back(St(426, S_PICKED));
    for (int k = 0; k < 60; ++k) c.grazes.push_back({200.0f + k * 2, "Nagara Cruiser", 1.4f});
    for (int k = 0; k < 377; ++k) c.grazes.push_back({271.0f + k * 0.01f, "fire", 0.03f});
    std::sort(c.grazes.begin(), c.grazes.end(), [](const RepGraze& a, const RepGraze& z) { return a.t < z.t; });
    c.endHp = 80; c.outcome = O_LOST_PICKED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Carwood Herron"; c.female = false; c.role = "repairing"; c.maxHp = 230;
    c.ev.push_back(Hit(234, R_CLOSE, 1, "the right leg", "", "Nagara Cruiser", "Nagara Cruiser", "140 mm Gun", "", true, 15, 200));
    c.ev.push_back(Hit(242, R_CLOSE, 1, "the left leg", "", "Nagara Cruiser", "Nagara Cruiser", "140 mm Gun", "", true, 15, 185));
    c.ev.push_back(Hit(250, R_CLOSE, 1, "the back of the head", "", "Nagara Cruiser", "Nagara Cruiser", "140 mm Gun", "", true, 15, 170));
    c.ev.push_back(St(315, S_SIDE));
    c.ev.push_back(St(429, S_PICKED));
    for (int k = 0; k < 60; ++k) c.grazes.push_back({200.0f + k * 2, "Nagara Cruiser", 1.4f});
    for (int k = 0; k < 377; ++k) c.grazes.push_back({271.0f + k * 0.01f, "fire", 0.03f});
    std::sort(c.grazes.begin(), c.grazes.end(), [](const RepGraze& a, const RepGraze& z) { return a.t < z.t; });
    c.endHp = 80; c.outcome = O_LOST_PICKED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Harvey Motowski"; c.female = false; c.role = "tending the wounded"; c.maxHp = 230;
    c.ev.push_back(Hit(234, R_CLOSE, 1, "the left leg", "", "Nagara Cruiser", "Nagara Cruiser", "140 mm Gun", "", true, 15, 200));
    c.ev.push_back(Hit(242, R_CLOSE, 1, "the left leg", "", "Nagara Cruiser", "Nagara Cruiser", "140 mm Gun", "", true, 15, 185));
    c.ev.push_back(Hit(250, R_CLOSE, 1, "the back of the head", "", "Nagara Cruiser", "Nagara Cruiser", "140 mm Gun", "", true, 15, 170));
    c.ev.push_back(St(320, S_SIDE));
    c.ev.push_back(St(432, S_PICKED));
    for (int k = 0; k < 60; ++k) c.grazes.push_back({200.0f + k * 2, "Nagara Cruiser", 1.4f});
    for (int k = 0; k < 377; ++k) c.grazes.push_back({271.0f + k * 0.01f, "fire", 0.03f});
    std::sort(c.grazes.begin(), c.grazes.end(), [](const RepGraze& a, const RepGraze& z) { return a.t < z.t; });
    c.endHp = 80; c.outcome = O_LOST_PICKED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Bernard Tipper"; c.female = false; c.role = "repairing"; c.maxHp = 230;
    c.ev.push_back(Hit(234, R_CLOSE, 1, "the right leg", "", "Nagara Cruiser", "Nagara Cruiser", "140 mm Gun", "", true, 15, 200));
    c.ev.push_back(Hit(242, R_CLOSE, 1, "the left leg", "", "Nagara Cruiser", "Nagara Cruiser", "140 mm Gun", "", true, 15, 185));
    c.ev.push_back(Hit(250, R_CLOSE, 1, "the left leg", "", "Nagara Cruiser", "Nagara Cruiser", "140 mm Gun", "", true, 15, 170));
    c.ev.push_back(St(325, S_SIDE));
    c.ev.push_back(St(435, S_PICKED));
    for (int k = 0; k < 60; ++k) c.grazes.push_back({200.0f + k * 2, "Nagara Cruiser", 1.4f});
    for (int k = 0; k < 377; ++k) c.grazes.push_back({271.0f + k * 0.01f, "fire", 0.03f});
    std::sort(c.grazes.begin(), c.grazes.end(), [](const RepGraze& a, const RepGraze& z) { return a.t < z.t; });
    c.endHp = 80; c.outcome = O_LOST_PICKED;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Vincent Randleman"; c.female = false; c.role = "standing by"; c.maxHp = 230;
    c.ev.push_back(Hit(234, R_CLOSE, 1, "the right leg", "", "Nagara Cruiser", "Nagara Cruiser", "140 mm Gun", "", true, 15, 200));
    c.ev.push_back(Hit(242, R_CLOSE, 1, "the left leg", "", "Nagara Cruiser", "Nagara Cruiser", "140 mm Gun", "", true, 15, 185));
    c.ev.push_back(Hit(250, R_CLOSE, 1, "the back of the head", "", "Nagara Cruiser", "Nagara Cruiser", "140 mm Gun", "", true, 15, 170));
    c.ev.push_back(St(330, S_SIDE));
    c.ev.push_back(St(438, S_PICKED));
    for (int k = 0; k < 60; ++k) c.grazes.push_back({200.0f + k * 2, "Nagara Cruiser", 1.4f});
    for (int k = 0; k < 377; ++k) c.grazes.push_back({271.0f + k * 0.01f, "fire", 0.03f});
    std::sort(c.grazes.begin(), c.grazes.end(), [](const RepGraze& a, const RepGraze& z) { return a.t < z.t; });
    c.endHp = 80; c.outcome = O_LOST_PICKED;
    b.crew.push_back(c);
  }
  return b;
}
static RepBattle BattleSynthetic() {
  RepBattle b;
  b.len = 400;
  b.seed = SeedOf("synthetic");
  {
    RepCrew c;
    c.name = "Ann Gray"; c.female = true; c.role = "on the Heavy Lewis of Salvage";
    c.gun = "Heavy Lewis of Salvage"; c.gunBase = "30 Cal Dual MG (Lewis)";
    c.ev.push_back(Hit(70, R_ROUND, 3, "the chest", "", "Long Boi Ka-Mi Amphibious", "Ka-Mi Amphibious", "Generic Tank Gun", "57 mm Tank Round (JAP)", false, 120, 0));
    c.ev.push_back(St(70.5f, S_DOWN));
    c.ev.push_back(St(110, S_UP));
    c.ev.push_back(Hit(150, R_FIRE, 1, "", "", "", "", "", "", false, 5, 60));
    c.ev.push_back(Hit(155, R_FIRE, 1, "", "", "", "", "", "", false, 6, 54));
    c.ev.push_back(Hit(200, R_TORPEDO, 2, "the back", "", "", "", "", "", false, 30, 24));
    c.ev.push_back(St(201, S_OVER_BLAST, true));
    c.ev.push_back(St(260, S_PULLED));
    c.ev.push_back(Hit(300, R_WRECK, 3, "", "", "Long Boi Ka-Mi Amphibious", "Ka-Mi Amphibious", "", "", false, 30, 0));
    c.ev.push_back(St(300.5f, S_DOWN));
    c.ev.push_back(St(320, S_KILLED));
    c.maxHp = 200; c.endHp = 0; c.outcome = O_KIA;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Bob Gray"; c.role = "at the helm";
    c.ev.push_back(Kill(90, "Long Boi Ka-Mi Amphibious", "Ka-Mi Amphibious", "", {"Ann Gray"}, 1));
    c.ev.push_back(St(95, S_SIDE));
    c.maxHp = 200; c.endHp = 200; c.outcome = O_MISSING;
    b.crew.push_back(c);
  }
  {
    RepCrew c;
    c.name = "Cy Diaz"; c.role = "repairing";
    for (int i = 0; i < 9; ++i)
      c.ev.push_back(Hit(60.f + i * 30, i % 2 ? R_CLOSE : R_FAR, 1 + i % 3, i % 3 ? "the left arm" : "", "", i % 2 ? "IJN Patrol T1" : "Soukou-Tei",
                         i % 2 ? "IJN Patrol T1" : "Soukou-Tei", "", "", false, 10.f + i, 150.f - i * 10));
    c.ev.push_back(St(330, S_ROLE));
    c.ev.back().part = "tending the wounded";
    c.grazes.push_back({100, "fire", 1});
    c.maxHp = 220; c.endHp = 60; c.outcome = O_DOWN_AT_END;
    b.crew.push_back(c);
  }
  {
    // stress: sixty hits, down and up again every fifteen, a trailing-space name and an unknown enemy kill
    RepCrew c;
    c.name = "Di Heavy "; c.female = true; c.role = "repairing";
    for (int i = 0; i < 60; ++i) {
      float t = 20.f + i * 6;
      c.ev.push_back(Hit(t, i % 3 ? R_CLOSE : R_ROUND, 1 + i % 3, i % 2 ? "the left leg" : "the right leg", "", i % 4 ? "Soukou-Tei" : "IJN Patrol T1",
                         i % 4 ? "Soukou-Tei" : "IJN Patrol T1", "", "57 mm Tank Round (JAP)", false, 5, 200.f - i * 3));
      if (i % 15 == 14) { c.ev.push_back(St(t + 0.5f, S_DOWN)); c.ev.push_back(St(t + 3, S_UP)); }
    }
    c.ev.push_back(Kill(395, "", "", "", {}, 2));
    for (int k = 0; k < 4; ++k) c.ev.push_back(Kill(380.f + k * 4, "", "", "", {}, 3 + k));
    c.maxHp = 220; c.endHp = 20; c.outcome = O_WOUNDED;
    b.crew.push_back(c);
  }
  b.kills.push_back({1, "Long Boi Ka-Mi Amphibious", "Ka-Mi Amphibious", 90});
  b.kills.push_back({2, "", "", 395});
  for (int k = 0; k < 4; ++k) b.kills.push_back({3 + k, "", "", 380.f + k * 4});
  return b;
}

// ---- checks ----
static std::string Dump(const std::vector<RepPara>& r) {
  std::string s;
  for (auto& p : r) s += (p.style == 2 ? "## " : p.style == 1 ? "# " : "") + p.text + "\n";
  return s;
}
static void CheckText(const char* name, const std::vector<RepPara>& r) {
  char msg[512];
  CHECK(!r.empty() && r[0].style == 2, "first paragraph is the title");
  std::string last, sect;   // the current crew header's surname, upper case; its paragraphs so far
  for (size_t pi = 0; pi < r.size(); ++pi) {
    auto& p = r[pi];
    snprintf(msg, sizeof msg, "%s: empty paragraph", name);
    CHECK(!p.text.empty(), msg);
    snprintf(msg, sizeof msg, "%s: token left in: %.200s", name, p.text.c_str());
    CHECK(p.text.find('{') == std::string::npos && p.text.find('}') == std::string::npos, msg);
    snprintf(msg, sizeof msg, "%s: tag left in: %.200s", name, p.text.c_str());
    CHECK(p.text.find("[h]") == std::string::npos && p.text.find("[down]") == std::string::npos, msg);
    snprintf(msg, sizeof msg, "%s: double space: %.200s", name, p.text.c_str());
    CHECK(p.text.find("  ") == std::string::npos, msg);
    if (p.style == 1) { std::string h = p.text.substr(0, p.text.find(" - ")); last = h.substr(h.rfind(' ') + 1); sect.clear(); }
    if (p.style != 0) continue;
    const std::string& t = p.text;
    for (const char* bad : {"twice over", "times over", " skull", "The crew fished", "smaller hits followed", "one minutes in",
                            "raked the deck around him,", "raked the deck around her,", "kept going. At once"}) {
      snprintf(msg, sizeof msg, "%s: '%s' in: %.160s", name, bad, t.c_str());
      CHECK(t.find(bad) == std::string::npos, msg);
    }
    if (!last.empty())   // a crew sentence never stacks two ", and" clauses
      for (size_t a = 0; a < t.size();) {
        size_t e = t.find(". ", a);
        std::string sn = t.substr(a, e == std::string::npos ? std::string::npos : e - a);
        size_t f = sn.find(", and ");
        snprintf(msg, sizeof msg, "%s: two ', and' clauses in: %.160s", name, sn.c_str());
        CHECK(f == std::string::npos || sn.find(", and ", f + 1) == std::string::npos, msg);
        if (e == std::string::npos) break;
        a = e + 2;
      }
    if (!last.empty() && (pi + 1 == r.size() || r[pi + 1].style != 0)) {   // the outcome line does not play down bleeding or stitches
      bool heavy = false, light = false;
      for (const char* w : {"stitch", "bled ", "loss of blood", "lot of blood", "grey and shaky", "badly drained"}) if (sect.find(w) != std::string::npos) heavy = true;
      for (const char* w : {"Nothing a dressing", "Nothing serious", "Fit for duty after a few dressings", "none the worse", "fit for duty", "Lightly hurt"}) if (t.find(w) != std::string::npos) light = true;
      snprintf(msg, sizeof msg, "%s: '%.60s' after heavy wounds: %.120s", name, t.c_str(), sect.c_str());
      CHECK(!(heavy && light), msg);
    }
    sect += t + " ";
    if (!last.empty() && pi + 1 < r.size() && r[pi + 1].style == 0) {   // a crew paragraph (not the outcome line) names the man before any him/his
      std::string first = " " + t.substr(0, t.find(". ")) + " ", up = first;
      for (auto& ch : up) ch = (char)toupper((unsigned char)ch);
      bool pron = false;
      for (const char* w : {" he ", " him ", " his ", " she ", " her ", " him,", " her,", " him.", " her."}) if (first.find(w) != std::string::npos) pron = true;
      snprintf(msg, sizeof msg, "%s: paragraph uses a pronoun before the name: %.120s", name, t.c_str());
      CHECK(!pron || up.find(last) != std::string::npos, msg);
    }
    for (size_t i = 0; i < t.size(); ++i) {
      bool start = i == 0 || (i >= 2 && (t[i - 2] == '.' || t[i - 2] == '!' || t[i - 2] == '?') && t[i - 1] == ' ');
      if (!start) continue;
      bool ok = (t[i] >= 'A' && t[i] <= 'Z') || (t[i] >= '0' && t[i] <= '9');
      snprintf(msg, sizeof msg, "%s: sentence without a capital at %d: %.120s", name, (int)i, t.c_str() + i);
      CHECK(ok, msg);
    }
  }
}
static int SectionHeaders(const char* ini) {
  int n = 0;
  std::string s(ini);
  size_t i = 0;
  while (i < s.size()) {
    size_t e = s.find('\n', i);
    if (e == std::string::npos) e = s.size();
    std::string l = s.substr(i, e - i);
    while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.pop_back();
    if (!l.empty() && l[0] == '[' && l.back() == ']' && l.find(']') == l.size() - 1) ++n;
    i = e + 1;
  }
  return n;
}

static char g_msg[512];
static std::string ReadFileText(const std::string& path) {
  std::string out;
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return out;
  char buf[4096]; size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
  fclose(f);
  if (out.size() >= 3 && (unsigned char)out[0] == 0xEF && (unsigned char)out[1] == 0xBB && (unsigned char)out[2] == 0xBF) out.erase(0, 3);
  return out;
}
// crew_report_test.exe render <folder or .bcb files> [--ini reports.ini]: re-renders real battles (BoatCrewMod\battles)
// with the current rows and runs the text checks on each
static int RenderMode(int argc, char** argv) {
  std::map<std::string, std::vector<std::string>> tables;
  std::vector<std::string> files;
  for (int i = 2; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--ini" && i + 1 < argc) { tables = ParseReportIni(ReadFileText(argv[++i])); continue; }
    if (a.size() > 4 && a.substr(a.size() - 4) == ".bcb") { files.push_back(a); continue; }
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((a + "\\*.bcb").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) continue;
    std::vector<std::string> found;
    do found.push_back(a + "\\" + fd.cFileName); while (FindNextFileA(h, &fd));
    FindClose(h);
    std::sort(found.begin(), found.end());
    files.insert(files.end(), found.begin(), found.end());
  }
  if (files.empty()) { printf("no .bcb battle files found\n"); return 1; }
  for (auto& path : files) {
    RepBattle b;
    if (!ParseBattle(ReadFileText(path), b)) { printf("FAIL: %s is not a battle file\n", path.c_str()); ++g_fail; continue; }
    auto r = RenderReport(b, tables);
    printf("==== %s ====\n%s\n", path.c_str(), Dump(r).c_str());
    CheckText(path.c_str(), r);
  }
  printf(g_fail ? "%d CHECKS FAILED\n" : "ALL CHECKS PASSED (%d battles)\n", g_fail ? g_fail : (int)files.size());
  return g_fail ? 1 : 0;
}
// ---- crew lives (crew_life.h): facts, memorials and letters ----
static void CheckLife(const std::string& what, const std::vector<std::string>& ps) {
  for (auto& p : ps) {
    snprintf(g_msg, sizeof g_msg, "%s: no token left: %s", what.c_str(), p.c_str()); CHECK(p.find('{') == std::string::npos && p.find('}') == std::string::npos && p.find('[') == std::string::npos, g_msg);
    snprintf(g_msg, sizeof g_msg, "%s: no double space or space before a comma: %s", what.c_str(), p.c_str());
    CHECK(p.find("  ") == std::string::npos && p.find(" ,") == std::string::npos && p.find(" .") == std::string::npos && p.find("..") == std::string::npos, g_msg);
    snprintf(g_msg, sizeof g_msg, "%s: not empty, starts with a capital: '%s'", what.c_str(), p.c_str()); CHECK(!p.empty() && !(p[0] >= 'a' && p[0] <= 'z'), g_msg);
  }
}
static const char* const kTraits[] = { "Has Kids", "Orphaned", "Drunkard", "High Seas Fisherman", "Cowboy", "Peacetime Crook",
  "Olympian", "National Hero", "Actually Underage", "Gambler", "Congenital Optimist", "Dilbert", "Acrobat", "Steady", "" };
static std::vector<std::string> FakeTraits(uint32_t id) {           // the game's traits are not in the dumps: a spread of them
  std::vector<std::string> t; size_t n = sizeof kTraits / sizeof *kTraits;
  t.push_back(kTraits[id % n]); if (id % 3 == 0) t.push_back(kTraits[(id / 7) % n]);
  return t;
}
static void PrintLetter(const char* head, const std::vector<std::string>& l) { printf("  -- %s --\n", head); for (auto& p : l) printf("  %s\n", p.c_str()); }
// The bond rules on a made-up battle: a twin mount, two mounts of one name, kills at the helm, swims apart, re-runs, ties.
static const BondTally* BondOf(BondLedger& L, uint32_t a, uint32_t b) { for (auto& t : L[a]) if (t.other == b) return &t; return nullptr; }
static void BondRulesCheck() {
  {   // the 64 cap keeps this battle's mates: 70 old close mates and one newcomer from battle 80
    std::vector<BondTally> v;
    for (int i = 0; i < 70; i++) { BondTally t; t.other = 100 + i; t.battles = 30; t.sank = 3; t.last = 40; v.push_back(t); }
    BondTally n; n.other = 999; n.battles = 1; n.last = 80; v.push_back(n);
    BondTrim(v, 80);
    bool kept = false; for (auto& t : v) if (t.other == 999) kept = true;
    CHECK(v.size() == 64 && kept, "BondTrim keeps a newcomer of this battle past the cap of 64");
  }
  RepBattle b; b.len = 300;
  const char* names[4] = { "Al Able", "Bo Baker", "Cy Charlie", "Di Dog" };
  for (int i = 0; i < 4; i++) { RepCrew c; c.name = names[i]; c.role = "on the 20mm"; c.outcome = O_UNHURT; b.crew.push_back(c); }
  for (int k = 0; k < 3; k++)                                         // three kills credited to all four; D took the helm mid-battle: no gun on his
    for (int i = 0; i < 4; i++) b.crew[i].ev.push_back(Kill(50.0f + k, "Boat", "Boat", i == 3 ? "" : "20mm", {}, k));
  b.crew[0].ev.push_back(St(100, S_OVER_BLAST)); b.crew[1].ev.push_back(St(101.5f, S_OVER_BLAST));   // A and B: one swim
  b.crew[2].ev.push_back(St(200, S_OVER_BLAST));                                                     // C: over alone, later
  std::vector<uint32_t> ids = { 1, 2, 3, 4 };
  std::vector<uint64_t> guns = { 11, 11, 12, 0 };                     // A and B on one mount; C on another mount of the same name
  BondLedger L;
  BondsAfterBattle(b, ids, 1, L, &guns);
  const BondTally *ab = BondOf(L, 1, 2), *ba = BondOf(L, 2, 1), *ac = BondOf(L, 1, 3), *bc = BondOf(L, 2, 3), *ad = BondOf(L, 1, 4);
  CHECK(ab && ba && ac && bc && ad, "bonds: every pair has a tally");
  if (!ab || !ba || !ac || !bc || !ad) return;
  CHECK(ab->team == 1 && ba->team == 1, "bonds: one mount, two crew = the same gun");
  CHECK(ac->team == 0 && bc->team == 0, "bonds: two mounts of one name are not the same gun");
  CHECK(ab->kills == 2, "bonds: shared kills capped at 2 a battle");
  CHECK(ad->kills == 0, "bonds: kills at the helm (no gun on the K_KILL) are not shared");
  CHECK(ab->water == 1, "bonds: over within 3 s = a swim together");
  CHECK(ac->water == 0 && bc->water == 0, "bonds: over at different times is not a swim together");
  BondLedger before = L;
  BondsAfterBattle(b, ids, 1, L, &guns);                              // the same battle again adds nothing
  bool same = true;
  for (auto& kv : L) for (size_t i = 0; i < kv.second.size(); i++) {
    const BondTally &x = kv.second[i], &y = before[kv.first][i];
    if (x.battles != y.battles || x.team != y.team || x.kills != y.kills || x.water != y.water || x.sank != y.sank) same = false;
  }
  CHECK(same, "bonds: a re-run of a battle adds nothing");
  BondLedger N; BondsAfterBattle(b, ids, 1, N);                       // no mounts: by gun name, so C counts with A
  CHECK(BondOf(N, 1, 3) && BondOf(N, 1, 3)->team == 1, "bonds: without mounts the gun name decides");
  std::vector<BondTally> tie(2); tie[0].other = 7; tie[1].other = 8; tie[0].battles = tie[1].battles = 20;   // 10 each: a tie
  CHECK(!ClosestBond(tie), "bonds: a tie names nobody");
  tie[0].team = 1;                                                    // 12 against 10: a clear lead
  CHECK(ClosestBond(tie) == &tie[0], "bonds: a clear lead names the mate");
  std::set<uint32_t> only8 = { 8 };
  CHECK(ClosestBond(tie, &only8) == &tie[1], "bonds: among limits who can be named");
  CHECK(BondWhy(tie[1]).find("side by side") != std::string::npos, "bonds: why reads");
  BondTally k; k.kills = 6;
  CHECK(BondWhy(k) == "shared kills", "bonds: the capped kill tally is not printed as a count");
  BondTally w; w.water = 1; w.battles = 3;
  CHECK(BondWhy(w, true).find("water") == std::string::npos, "bonds: noWater drops the swim");
  {                                                                   // two separate two-man swims: each is a swim together
    RepBattle w; w.len = 500;
    for (int i = 0; i < 4; i++) { RepCrew c; c.name = names[i]; c.role = "passing ammunition"; c.outcome = O_UNHURT; c.ev.push_back(St(i < 2 ? 60.0f : 400.0f, S_OVER_HIT)); w.crew.push_back(c); }
    BondLedger W; BondsAfterBattle(w, ids, 1, W);
    CHECK(BondOf(W, 1, 2)->water == 1 && BondOf(W, 3, 4)->water == 1 && BondOf(W, 1, 3)->water == 0, "bonds: two small swims in one battle both count");
    for (auto& c : w.crew) c.ev[0].t = 60;                            // all four together: the whole crew's swim says nothing
    BondLedger W2; BondsAfterBattle(w, ids, 1, W2);
    CHECK(BondOf(W2, 1, 2)->water == 0, "bonds: a swim of four is not a bond");
  }
  printf("bond rules: checked on a made-up battle\n");
}
// crew_report_test.exe life <folder or .bcb files>: plays the battles in order as one campaign, rolls each crew
// member's facts once, and prints the file block, then the memorial and both letters for everyone lost
// ---- nicknames and the dead man's station (crew_nick.h, crew_life.h StationRole, the report's .nick and station rows) ----
struct NickEntry { int battle = 0; std::string when, len, role, outcome; int kills = 0, hits = 0, dmg = 0, wounds = 0, downs = 0, over = 0; std::vector<std::string> paras; };
static void NickChecks() {
  using namespace crew_report_detail;
  const std::map<std::string, std::vector<std::string>> none;
  // 1. the rules
  std::vector<NickEntry> en;
  for (int i = 1; i <= 3; ++i) { NickEntry x; x.battle = i; x.role = "ammunition party"; x.outcome = "unhurt"; x.over = 1; en.push_back(x); }
  std::set<std::string> noTaken;
  const char* n = NickEarned(en, 0, {}, noTaken);
  CHECK(n && std::string(n) == "Lucky", "nick: three survived battles over the side give Lucky");
  n = NickEarned(en, 0, { NickRec{ 'v', "Lucky", 3 } }, noTaken);
  CHECK(!n || std::string(n) != "Lucky", "nick: a vetoed Lucky is not offered again");
  n = NickEarned(en, 0, {}, { "Lucky" });
  CHECK(!n || std::string(n) != "Lucky", "nick: Lucky held by another is not offered");
  CHECK(!NickEarned(en, 0, { NickRec{ 'k', "Doc", 2 } }, noTaken) && !NickEarned(en, 0, { NickRec{ 'p', "Doc", 2 } }, noTaken), "nick: a kept or offered name on file gives nothing");
  CHECK(!NickEarned(en, 3, {}, noTaken), "nick: the lost earn nothing");
  { auto j = en; j[1].outcome = "lost at sea, then picked up";
    n = NickEarned(j, 0, {}, noTaken); CHECK(n && std::string(n) == "Jonah", "nick: lost at sea and picked up gives Jonah first"); }
  { auto k = en; k[2].outcome = "killed in action";   // a battle he did not survive does not count
    n = NickEarned(k, 0, {}, noTaken); CHECK(!n || std::string(n) != "Lucky", "nick: only survived battles count for Lucky"); }
  for (auto& r : kNickRules) {
    std::string s = r.nick;
    snprintf(g_msg, sizeof g_msg, "nick: '%s' is clean and has a reason", r.nick);
    CHECK(!s.empty() && s.find('|') == std::string::npos && s.find(" - ") == std::string::npos && s.find_first_of("\r\n\"{}") == std::string::npos &&
          r.why[0] && std::string(NickWhy(s)) == r.why && std::string(r.why).find("--") == std::string::npos, g_msg);
  }
  // 2. the report with a kept nickname
  RepBattle plain = BattleSynthetic();
  std::string base = Dump(RenderReport(plain, none));
  RepBattle b = BattleSynthetic(); b.crew[0].nick = "Lucky";
  auto r = RenderReport(b, none);
  std::string d = Dump(r);
  printf("==== synthetic, Ann Gray called Lucky ====\n%s\n", d.c_str());
  CheckText("nickname", r);
  CHECK(d.find("# ANN \"LUCKY\" GRAY - ") != std::string::npos, "nick: the heading names him by his nickname");
  size_t h1 = d.find("\n# BOB GRAY");
  CHECK(h1 != std::string::npos && d.substr(0, d.find("\n# ")).find(NickName("Ann Gray", "Lucky")) != std::string::npos, "nick: the summary's casualties carry the nickname");
  CHECK(d.find("{Nick") == std::string::npos && d.find("{dead") == std::string::npos && d.find("{oldgun") == std::string::npos, "nick: no token left");
  CHECK(h1 != std::string::npos && d.find("Lucky", h1) == std::string::npos, "nick: only his own paragraphs name the nickname");
  { RepBattle big = BattleBig(); bool lostOne = true;                // kill lists and mates keep the plain name
    for (auto& c : big.crew) if (c.name == "Earl Davenport") { c.nick = "Lucky"; lostOne = c.outcome == O_KIA || c.outcome == O_MISSING || c.outcome == O_CAPTURED; }
    std::string db = Dump(RenderReport(big, none));
    CHECK(!lostOne && db.find("# EARL \"LUCKY\" DAVENPORT - ") != std::string::npos && db.find("with Earl Davenport") != std::string::npos && db.find("Lucky") == std::string::npos,
          "nick: others' kill lists name the plain name"); }
  CHECK(d.find("\"Lucky\"", d.find("# ANN")) < h1, "nick: the outcome line of the dead names the nickname");
  b.crew[0].nick.clear();
  CHECK(Dump(RenderReport(b, none)) == base, "nick: no nickname renders as before");
  { auto t = ParseReportIni(ReportDefaultIni()); t["out.kia.nick"] = { "[h]A joke row." };   // a section that gives nothing: the plain one
    RepBattle q = BattleSynthetic(); q.crew[0].nick = "Lucky";
    std::string dq = Dump(RenderReport(q, t)); size_t a = dq.find("# ANN"), z = dq.find("\n# BOB");
    CHECK(dq.find("Killed in action", a) < z, "nick: an empty .nick section falls back to the plain outcome"); }
  // 3. the station
  RepBattle s = BattleSynthetic();
  s.crew[0].tookDead = "Vernon Coviello"; s.crew[0].tookGun = s.crew[0].gun; s.crew[0].tookJob = RC_GUN;
  auto rs = RenderReport(s, none);
  printf("==== synthetic, Ann Gray at Coviello's gun ====\n%s\n", Dump(rs).c_str());
  CheckText("station", rs);
  int cov = 0; size_t at = 0;
  for (size_t i = 0; i < rs.size(); ++i) if (rs[i].text.find("Coviello") != std::string::npos) { ++cov; at = i; }
  CHECK(cov == 1 && at > 0 && rs[at - 1].style == 1 && rs[at - 1].text.rfind("ANN GRAY", 0) == 0 && rs[at].style == 0, "station: one line, right after the heir's heading");
  CHECK(cov == 1 && rs[at].text.find(s.crew[0].gun) != std::string::npos, "station: the line names the gun");
  s.crew[0].tookJob = RC_AMMO;
  CHECK(Dump(RenderReport(s, none)).find("Gray passed ammunition in Coviello's place.") != std::string::npos, "station: the ammunition line");
  s.crew[0].tookJob = RC_GUN; s.crew[0].tookGun.clear();
  CHECK(Dump(RenderReport(s, none)) == base, "station: a gun station with no gun says nothing");
  // 4. the dump keeps the new fields
  RepBattle w = BattleSynthetic(); w.crew[0].nick = "Lucky"; w.crew[1].tookDead = "Vernon Coviello"; w.crew[1].tookJob = RC_HELM; w.crew[2].tookGun = "M1919";
  RepBattle back;
  CHECK(ParseBattle(DumpBattle(w), back) && back.crew[0].nick == "Lucky" && back.crew[1].tookDead == "Vernon Coviello" && back.crew[1].tookJob == RC_HELM &&
        back.crew[2].tookGun == "M1919" && back.crew[0].tookJob == -1 && Dump(RenderReport(back, none)) == Dump(RenderReport(w, none)), "dump: nick and station read back");
  // 5. StationRole
  LifeFate fg; fg.battle = 2; fg.role = RC_GUN; fg.gun = "M1919";
  LifeFate fa; fa.battle = 2; fa.role = RC_AMMO;
  LifeFate f0; f0.battle = 2; f0.role = RC_GUN;
  CHECK(StationRole("on the M1919", fg) && StationRole("gunner, M1919", fg) && !StationRole("on the Lewis", fg), "station: a gun by name");
  CHECK(StationRole("ammunition party", fa) && StationRole("passing ammunition", fa) && !StationRole("on the M1919", fa), "station: a job by kind");
  CHECK(!StationRole("on the M1919", f0) && !StationRole("gunner", f0) && !StationRole("on a gun", f0), "station: a gun fate with no gun matches nothing");
  // 6. the memorial and the letter
  LifeTables lt;
  LifeFacts lf = RollFacts(4242u, false, "Vernon Coviello", {}, {}, lt);
  LifeService sv; sv.first = 1; sv.last = 5; sv.battles = 5;
  LifeFate ft = fg; ft.kind = 'k'; ft.by = "ship";
  int lines0 = LetterLineCount(lf, sv, ft, lt);
  LifeService sn = sv; sn.nick = "Lucky"; sn.nickWhy = NickWhy("Lucky"); sn.heir = "Ann Gray";
  auto mem = MemorialText("Vernon Coviello", false, 4242u, lf, sn, ft, lt);
  std::string m; for (auto& p : mem) m += p + "\n";
  printf("==== memorial with a nickname and an heir ====\n%s\n", m.c_str());
  CheckLife("nick memorial", mem);
  CHECK(m.find("Vernon \"Lucky\" Coviello") != std::string::npos && m.find("\"Lucky\", for " + std::string(NickWhy("Lucky"))) != std::string::npos, "life: the memorial names the nickname and why");
  CHECK(m.find("His M1919 went to Ann Gray.") != std::string::npos, "life: the memorial says who has his gun");
  CHECK(LetterLineCount(lf, sn, ft, lt) == lines0 + 1, "life: the letter gets one more line to pick with a nickname");
  bool nickLine = false;
  for (int p = 0; p < lines0 + 1; ++p) {
    auto l = DraftLetter("Vernon Coviello", false, 4242u, lf, sn, ft, true, p, lt);
    CheckLife("nick letter", l);
    CHECK(l[0].find("Lucky") == std::string::npos, "life: the letter's opening stays formal");
    for (auto& x : l) if (x.find("\"Lucky\"") != std::string::npos) nickLine = true;
  }
  CHECK(nickLine, "life: a letter can say the nickname");
}
static int LifeMode(int argc, char** argv) {
  LifeTables t;
  std::vector<std::string> files;
  for (int i = 2; i < argc; ++i) {
    std::string a = argv[i];
    if (a.size() > 4 && a.substr(a.size() - 4) == ".bcb") { files.push_back(a); continue; }
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA((a + "\\*.bcb").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) continue;
    std::vector<std::string> found; do found.push_back(a + "\\" + fd.cFileName); while (FindNextFileA(h, &fd)); FindClose(h);
    std::sort(found.begin(), found.end()); files.insert(files.end(), found.begin(), found.end());
  }
  struct Man { uint32_t id; std::string name; bool female; LifeFacts f; LifeService s; LifeFate fate; MailState mail; bool fought = false; };
  std::vector<Man> men; men.reserve(4096); BondLedger ledger; int bn = 0, lost = 0, mailCall = 0, letters = 0, returned = 0;
  for (auto& path : files) {
    RepBattle b; if (!ParseBattle(ReadFileText(path), b)) continue;
    ++bn;
    for (auto& x : men) x.fought = false;
    for (auto& c : b.crew) {
      Man* m = nullptr; for (auto& x : men) if (x.name == c.name && !x.fate.battle) m = &x;
      if (!m) {
        Man x; x.id = crew_report_detail::Fnv(c.name + std::to_string(bn)); x.name = c.name; x.female = c.female;
        std::vector<LifeFacts> taken; for (auto& o : men) taken.push_back(o.f);
        x.f = RollFacts(x.id, x.female, x.name, FakeTraits(x.id), taken, t);
        CHECK(RollFacts(x.id, x.female, x.name, FakeTraits(x.id), taken, t).job == x.f.job, "facts roll the same every time");
        auto ft = LifeFileText(x.name, x.female, x.f, t);
        printf("%s [%s]: %s / %s\n", x.name.c_str(), FakeTraits(x.id)[0].c_str(), ft[0].c_str(), ft[1].c_str());
        CheckLife(x.name + " file", ft);
        men.push_back(x); m = &men.back();
      }
      m->fought = true;
      LifeService& s = m->s;
      if (!s.first) s.first = bn;
      s.last = bn; s.battles++;
      for (auto& e : c.ev) { if (e.kind == S_WOUNDED) s.wounds++; if (e.kind == S_DOWN || e.kind == S_BLEED) s.downs++;
        if (e.kind == S_OVER_HIT || e.kind == S_OVER_BLAST || e.kind == S_SIDE) s.over++; if (e.kind == K_KILL) s.kills++; }
      if (c.outcome == O_KIA || c.outcome == O_MISSING || c.outcome == O_CAPTURED) {
        m->fate = FateOf(b, &c - &b.crew[0], bn); ++lost;
        for (auto& e : c.ev) { if (e.kind == S_WOUNDED) s.wounds--; if (e.kind == S_DOWN || e.kind == S_BLEED) s.downs--;   // as LifeServiceOf: what they came through
          if (e.kind == S_OVER_HIT || e.kind == S_OVER_BLAST || e.kind == S_SIDE) s.over--; }
        printf("\n==== %s, lost in %s (kind %c, by '%s', part '%s', role %d, %.1f min) ====\n", m->name.c_str(), path.c_str(),
               m->fate.kind, m->fate.by.c_str(), m->fate.part.c_str(), m->fate.role, m->fate.mins);
        auto mem = MemorialText(m->name, m->female, m->id, m->f, s, m->fate, t);
        PrintLetter("memorial", mem); CheckLife(m->name + " memorial", mem);
        int n = LetterLineCount(m->f, s, m->fate, t);
        for (int truth = 1; truth >= 0; --truth)
          for (int p = 0; p < n; ++p) {
            auto l = DraftLetter(m->name, m->female, m->id, m->f, s, m->fate, truth != 0, p, t);
            if (p < 2) PrintLetter(truth ? "letter, the truth" : "letter, kind", l);
            CheckLife(m->name + " letter", l);
            CHECK(l.size() >= 5 && l[0].rfind("Dear ", 0) == 0, "a letter starts Dear and has its parts");
          }
        printf("\n");
      }
    }
    {                                                                  // bonds: what each pair shared this battle
      std::vector<uint32_t> ids;
      for (auto& c : b.crew) { uint32_t id = 0; for (auto& x : men) if (x.name == c.name && x.fought) id = x.id; ids.push_back(id); }
      BondsAfterBattle(b, ids, bn, ledger);
      BondsAfterBattle(b, ids, bn, ledger);                            // a re-run adds nothing
    }
    std::vector<MailCrew> mc;
    for (auto& x : men) { MailCrew c; c.id = x.id; c.name = x.name; c.female = x.female; c.facts = &x.f; c.fate = &x.fate; c.fought = x.fought; c.st = &x.mail; mc.push_back(c); }
    auto got = MailCall(bn, mailCall, b.seed, mc, t);
    if (!got.empty()) {
      mailCall = bn;
      printf("\n#### MAIL CALL after battle %d (%s): %d letter(s)\n", bn, path.c_str(), (int)got.size());
      for (auto& d : got) {
        const Man& x = men[d.crew]; ++letters; if (d.letter.kind) ++returned;
        std::string stamp = d.letter.kind ? std::string("  [") + MailStamp(d.letter.kind) + "]" : std::string();
        printf("  FOR %s, FROM %s (%s step %d)%s\n    %s\n    %s\n    %s\n", x.name.c_str(), MailFrom(d.letter, x.female).c_str(), d.letter.thread.c_str(), d.letter.step + 1,
               stamp.c_str(), d.letter.dear.c_str(), d.letter.body.c_str(), d.letter.sign.c_str());
        CheckLife(x.name + " mail", { d.letter.dear, d.letter.body, d.letter.sign, crew_report_detail::CapFirst(MailFrom(d.letter, x.female)) });
        CHECK(!d.letter.body.empty(), "a delivered letter has a body");
      }
    }
  }
  // every thread, both sexes, every kin it allows: no token left
  int threadLetters = 0;
  for (int fem = 0; fem < 2; ++fem)
    for (const char* kin : { "mother", "father", "parents", "spouse", "sister", "brother", "grandmother", "aunt", "uncle" })
      for (int kids = 0; kids < 2; ++kids) {
        LifeFacts f = RollFacts(77 + fem * 3 + kids, fem != 0, "Pat Doe", {}, {}, t); f.kin = kin; f.kids = kids != 0; f.kinFirst = std::string(kin) == "parents" ? "Walter and Ellen" : "Ellen";
        int fromKin = 0;
        for (int k = 0; k < MailThreadCount() * 2; ++k) {
          MailState st; MailCrew c; c.id = 1000u + (uint32_t)k * 31u + (uint32_t)fem; c.name = "Pat Doe"; c.female = fem != 0; c.facts = &f; c.fought = true; c.st = &st;
          std::vector<MailCrew> one = { c };
          std::map<std::string, std::string> fam = crew_mail_detail::BaseFamily(f, c.female);   // every thread he gets agrees on the family
          std::string lastThread; std::set<std::string> roles;
          for (int b2 = 3; b2 < 60; b2 += 3) for (auto& d : MailCall(b2, b2 - 3, (uint32_t)k, one, t)) {
            ++threadLetters;
            CheckLife(std::string("thread ") + d.letter.thread, { d.letter.dear, d.letter.body, d.letter.sign, crew_report_detail::CapFirst(MailFrom(d.letter, c.female)) });
            if (d.letter.who == kin || (std::string(kin) == "spouse" && (d.letter.who == "wife" || d.letter.who == "husband"))) ++fromKin;
            if (d.letter.thread == lastThread) continue;
            lastThread = d.letter.thread;
            const auto* th = crew_mail_detail::FindThread(d.letter.thread);
            for (auto& kv : th ? th->keys : std::vector<std::pair<std::string, std::string>>())
              if (kv.second == "@sender" && crew_mail_detail::IsRole(kv.first)) {
                snprintf(g_msg, sizeof g_msg, "thread %s: a second %s story for one man", d.letter.thread.c_str(), kv.first.c_str());
                CHECK(roles.insert(kv.first).second, g_msg);
              }
            bool born = th && crew_mail_detail::Needs(*th, "nokids");
            for (auto& kv : th ? th->keys : std::vector<std::pair<std::string, std::string>>()) {
              if (kv.first == "sendersex" || (born && (kv.first == "son" || kv.first == "daughter" || kv.first == "children"))) continue;
              std::string v = kv.second == "@sender" ? d.letter.sender : kv.second;
              auto it = fam.find(kv.first);
              snprintf(g_msg, sizeof g_msg, "thread %s: family key %s=%s clashes with %s", d.letter.thread.c_str(), kv.first.c_str(), v.c_str(), it == fam.end() ? "" : it->second.c_str());
              CHECK(it == fam.end() || it->second == v, g_msg);
              fam[kv.first] = v;
            }
            if (born) fam["children"] = "yes";
          }
        }
        snprintf(g_msg, sizeof g_msg, "next of kin '%s' (%s, kids %d) writes at least once", kin, fem ? "woman" : "man", kids);
        CHECK(fromKin > 0 || std::string(kin) == "parents", g_msg);
      }
  // every fate, kin and gender through the letter, with and without facts
  const char kinds[] = "kwsmc"; const char* bys[] = { "plane", "ship", "shore", "torpedo", "fire", "sank", "" };
  const char* kins[] = { "mother", "father", "parents", "spouse", "sister", "brother", "grandmother", "aunt", "uncle" };
  int combos = 0;
  for (char k : std::string(kinds)) for (auto by : bys) for (auto kin : kins) for (int fem = 0; fem < 2; ++fem) {
    LifeFacts f = RollFacts(combos * 7919u + 1, fem != 0, "Ann Smith", { combos % 2 ? "Has Kids" : "" }, {}, t); f.kin = kin;
    LifeService s; s.first = 1; s.last = 3 + combos % 5; s.battles = s.last; s.wounds = combos % 3; s.kills = combos % 4;
    LifeFate fate; fate.battle = s.last; fate.kind = k; fate.by = by; fate.part = combos % 2 ? "the chest" : ""; fate.role = combos % 6; fate.mins = (float)(combos % 9);
    for (int truth = 0; truth < 2; ++truth) {
      auto l = DraftLetter(fem ? "Ann Smith" : "Joe Smith", fem != 0, 99 + combos, f, s, fate, truth != 0, combos, t);
      CheckLife("combo letter", l);
      if (combos % 97 == 0 && truth) PrintLetter(kin, l);
    }
    CheckLife(std::string("combo memorial ") + k + by, MemorialText("Joe Smith", fem != 0, 99 + combos, f, s, fate, t));
    CheckLife("combo memorial, no facts", MemorialText("Joe Smith", fem != 0, 99 + combos, LifeFacts(), s, fate, t));
    CheckLife("combo letter, no facts", DraftLetter("Joe Smith", fem != 0, 9, LifeFacts(), s, fate, true, combos, t));
    ++combos;
  }
  CHECK(!ParseLifeIni(LifeDefaultIni()).empty(), "the built-in life tables parse");
  {                                                                    // real rolls: does the next of kin always have a thread that fits?
    const char* tr[] = { "Has Kids", "Orphaned", "Actually Underage", "High Seas Fisherman", "Cowboy", "", "", "" };
    std::map<std::string, int> rolled, none, one;
    for (uint32_t id = 1; id < 20001; ++id) {
      bool fem = id % 4 == 0;
      std::vector<std::string> traits = { tr[id % 8], tr[(id / 8) % 8] };
      LifeFacts f = RollFacts(id * 2654435761u, fem, "Pat Doe", traits, {}, t);
      auto fam = crew_mail_detail::BaseFamily(f, fem);
      int n = 0;
      for (auto& th : crew_mail_detail::Threads()) {
        if (crew_mail_detail::WhoWeight(th.who, f, fem) < 3 || !crew_mail_detail::NeedsOk(th, f, fem, t)) continue;   // the kin's own threads
        std::string sender;
        if (crew_mail_detail::FitsFamily(th, fam, {}, &sender)) ++n;
      }
      std::string key = f.kin + (f.orphan ? " (orphan)" : "") + (f.kids ? " +kids" : "") + (fem ? " F" : " M");
      rolled[key]++; if (n == 0) none[key]++; if (n == 1) one[key]++;
      if (n == 0 && none[key] <= 2) printf("  no kin thread: %s | job %s | keep %s | plan %s | home %s\n", key.c_str(), f.job.c_str(), f.keep.c_str(), f.plan.c_str(), f.home.c_str());
    }
    for (auto& kv : rolled) {
      int z = none.count(kv.first) ? none[kv.first] : 0, o = one.count(kv.first) ? one[kv.first] : 0;
      printf("kin %-28s %5d rolled, %4d with no kin thread, %4d with one\n", kv.first.c_str(), kv.second, z, o);
      bool orphanSibling = kv.first.find("(orphan)") != std::string::npos && (kv.first.rfind("sister", 0) == 0 || kv.first.rfind("brother", 0) == 0);
      snprintf(g_msg, sizeof g_msg, "next of kin %s: %d of %d have no thread of their own", kv.first.c_str(), z, kv.second);
      CHECK(z == 0 || orphanSibling, g_msg);
    }
  }
  BondRulesCheck();
  int closeN = 0;
  for (auto& x : men) {
    const BondTally* cb = ledger.count(x.id) ? ClosestBond(ledger[x.id]) : nullptr;
    if (!cb) continue;
    ++closeN;
    std::string other; for (auto& y : men) if (y.id == cb->other) other = y.name;
    if (closeN <= 12) printf("bond: %s and %s %s (score %.1f)\n", x.name.c_str(), other.c_str(), BondWhy(*cb).c_str(), BondScore(*cb));
    LifeService s; s.battles = 3; s.first = 1; s.last = 3; s.friendName = other; s.friendWhy = BondWhy(*cb);
    LifeFate fate; fate.battle = 3; fate.kind = 'k';
    CheckLife(x.name + " letter with a friend", DraftLetter(x.name, x.female, x.id, x.f, s, fate, true, 0, t));
    CheckLife(x.name + " memorial with a friend", MemorialText(x.name, x.female, x.id, x.f, s, fate, t));
    bool hasFriendLine = false;
    for (int pk = 0; pk < 12; ++pk) for (auto& para : DraftLetter(x.name, x.female, x.id, x.f, s, fate, true, pk, t)) if (para.find(other) != std::string::npos) hasFriendLine = true;
    CHECK(hasFriendLine, "the letter can name the closest mate");
  }
  printf("bonds: %d crew have a close mate\n", closeN);
  CHECK(closeN > 0, "real battles make close mates");
  printf("mail: %d thread(s), %d letter(s) delivered, %d returned or forwarded; %d letters in the thread sweep\n", MailThreadCount(), letters, returned, threadLetters);
  printf(g_fail ? "%d CHECKS FAILED\n" : "ALL CHECKS PASSED (%d crew, %d lost, %d combinations)\n", g_fail ? g_fail : (int)men.size(), lost, combos);
  return g_fail ? 1 : 0;
}
// crew_report_test.exe talk: every voice and event, both sexes, with and without facts, and every letter step's line;
// prints a sample of each section and checks that tokens fill, lines stay short and every section has lines.
static int TalkMode() {
  TalkTables t; LifeTables lt;
  const char* events[] = { "battle_start", "wounded", "kill", "kill_plane", "crew_down", "crew_killed", "crew_overboard", "fire", "quiet",
    "quiet_home", "letter", "grief", "grief_start", "reply_quiet", "reply_letter", "reply_grief", "rollcall_here", "rollcall_open",
    "rollcall_silence", "rollcall_close", "letter_returned", "man_overboard", "reply_letter_bad",
    "combat_gun", "combat_ammo", "combat_repair", "combat_medic", "combat_helm", "combat_any", "raked",
    "crew_killed_close", "grief_close", "grief_start_close", "rollcall_silence_close", "quiet_partner", "reply_partner", "reply_partner_warm",
    "combat_partner", "reply_combat_partner", "reply_combat_partner_warm", "station_gun", "station_job" };
  int lines = 0, samples = 0;
  for (int fem = 0; fem < 2; ++fem) {
    LifeFacts f = RollFacts(4242u + fem, fem != 0, "Pat Doe", {}, {}, lt), df = RollFacts(999u + fem, fem == 0, "Lee Roe", {}, {}, lt);
    MailState ms, dms;
    MailLetter l; l.who = "mother"; l.sender = "Ellen"; l.thread = "none"; ms.got.push_back(l);
    MailLetter dl; dl.who = "wife"; dl.sender = "Ruth"; dl.kind = 1; dms.got.push_back(dl);
    TalkCrew dead; dead.id = 77; dead.name = "Lee Roe"; dead.female = fem == 0; dead.facts = &df; dead.mail = &dms;
    TalkCrew mate; mate.id = 78; mate.name = "Sam Poe"; mate.voice = "joker";
    for (const char* v : kTalkVoices) {
      TalkCrew sp; sp.id = 5u + (uint32_t)fem; sp.name = fem ? "Ann Doe" : "Pat Doe"; sp.female = fem != 0; sp.voice = v; sp.facts = &f; sp.mail = &ms;
      int k2 = 0;
      for (const char* e : events) {
        ++k2;
        TalkCtx c; c.mate = &mate; c.down = &mate; c.dead = &dead; c.speakerGun = c.deadGun = k2 % 3 != 0; c.why = k2 % 2 ? "manned the same gun for four battles" : "came through a sinking together";
        if (std::string(e) == "letter") { c.dead = nullptr; c.letter = &l; }
        if (std::string(e) == "letter_returned") c.letter = &dl;
        std::set<std::string> used, said; std::string first;
        for (int k = 0; k < 40; ++k) {
          std::string s = TalkLine(e, sp, c, (uint32_t)k, t, &used);
          if (s.empty() || !said.insert(s).second) break;                 // a repeat: all said
          if (first.empty()) first = s;
          ++lines;
          snprintf(g_msg, sizeof g_msg, "talk %s.%s: no token left, at most 90 chars: '%s'", v, e, s.c_str());
          CHECK(s.find('{') == std::string::npos && s.find('[') == std::string::npos && s.size() <= 90, g_msg);
        }
        snprintf(g_msg, sizeof g_msg, "talk %s.%s (%s) has a line", v, e, fem ? "woman" : "man");
        CHECK(!first.empty(), g_msg);
        if (fem == 0 && ++samples % 3 == 0) printf("%-13s %-17s %s\n", v, e, first.c_str());
      }
      TalkCrew bare = sp; bare.facts = nullptr; bare.mail = nullptr;    // a crew member not on file yet
      for (const char* e : { "quiet", "battle_start", "wounded" }) {
        snprintf(g_msg, sizeof g_msg, "talk %s.%s works with no facts", v, e);
        CHECK(!TalkLine(e, bare, TalkCtx(), 1, t, nullptr).empty(), g_msg);
      }
    }
  }
  {                                                                   // a row with a name: once a battle, whatever the name; last battle's lines come last
    TalkTables r = ParseTalkIni("[joker.zz]\nHey {mate}, one.\nTwo.\nThree.\n");
    TalkCrew sp; sp.id = 9; sp.name = "Pat Doe"; sp.voice = "joker";
    TalkCrew m1, m2; m1.name = "Al Able"; m2.name = "Bo Baker";
    TalkCtx c1, c2; c1.mate = &m1; c2.mate = &m2;
    std::set<std::string> used; int hey = 0;
    for (int k = 0; k < 3; ++k) hey += TalkLine("zz", sp, k % 2 ? c2 : c1, (uint32_t)k * 7, r, &used).compare(0, 3, "Hey") == 0;
    CHECK(hey == 1, "talk: a row with a name is not said again this battle with another name");
    std::set<std::string> last = { "~Two.", "~Three." };
    CHECK(TalkLine("zz", sp, c2, 5, r, &last) == "Hey Baker, one.", "talk: a line not said last battle comes first");
    std::string x = TalkLine("zz", sp, c2, 5, r, &last), y = TalkLine("zz", sp, c2, 6, r, &last);
    CHECK(x != y && x.compare(0, 3, "Hey") != 0 && y.compare(0, 3, "Hey") != 0, "talk: last battle's lines before this battle's");
    CHECK(!TalkLine("zz", sp, c2, 7, r, &last).empty(), "talk: all said: any line again");
  }
  {                                                                   // a kept nickname: {dead} is the nickname some of the time
    TalkTables r = ParseTalkIni("[any.zz]\nFor {dead}.\n");
    TalkCrew dead; dead.id = 77; dead.name = "Lee Roe"; dead.nick = "Lucky";
    TalkCtx c; c.dead = &dead;
    bool nick = false, sur = false, bad = false;
    int uses0 = TalkNickUses();
    for (uint32_t seed = 0; seed < 50; ++seed) {
      TalkCrew sp; sp.id = 5; sp.name = "Pat Doe"; sp.voice = "joker";
      std::string s = TalkLine("zz", sp, c, seed, r, nullptr);
      if (s == "For Lucky.") nick = true; else if (s == "For Roe.") sur = true; else bad = true;
    }
    CHECK(nick && sur && !bad, "talk: {dead} is the nickname or the surname, never a token");
    CHECK(TalkNickUses() > uses0, "talk: nickname uses are counted");
    TalkCrew sp; sp.id = 5; sp.name = "Pat Doe"; sp.voice = "stoic";
    CHECK(!TalkLine("station_gun", sp, c, 1, t, nullptr).empty() && !TalkLine("station_job", sp, c, 1, t, nullptr).empty(), "talk: the station lines fill");
  }
  // every mail step has its own line, and it fills
  int steps = 0, own = 0, badSteps = 0;
  for (auto& th : crew_mail_detail::Threads())
    for (size_t st = 0; st < th.steps.size(); ++st) {
      ++steps;
      LifeFacts f = RollFacts(5u, false, "Pat Doe", {}, {}, lt);
      MailState ms; MailLetter l; l.thread = th.id; l.step = (int)st; l.who = th.who; l.sender = "Dot"; ms.got.push_back(l);
      TalkCrew sp; sp.id = 9; sp.name = "Pat Doe"; sp.voice = "stoic"; sp.facts = &f; sp.mail = &ms;
      bool bad = false; std::string s = TalkLetterLine(sp, 0, 1, t, nullptr, &bad); if (bad) ++badSteps;
      if (!crew_talk_detail::TalkSec(t, "letter." + th.id + "." + std::to_string(st + 1)).empty()) ++own;
      snprintf(g_msg, sizeof g_msg, "letter line %s step %d: '%s'", th.id.c_str(), (int)st + 1, s.c_str());
      CHECK(!s.empty() && s.find('{') == std::string::npos && s.size() <= 90, g_msg);
      if (steps % 40 == 1) printf("letter %-45s %d: %s\n", th.id.c_str(), (int)st + 1, s.c_str());
    }
  // voices: every one rolls, and the traits steer them
  std::map<std::string, int> roll;
  for (uint32_t id = 1; id < 5001; ++id) roll[RollVoice(id * 2654435761u, { id % 3 == 0 ? "STEADY" : "", id % 5 == 0 ? "FURIOUS" : "" })]++;
  for (auto& kv : roll) printf("voice %-13s %d of 5000\n", kv.first.c_str(), kv.second);
  CHECK(roll.size() == 8, "all eight voices roll");
  printf("talk: %d lines checked; %d of %d letter steps have their own lines, %d bring bad news\n", lines, own, steps, badSteps);
  CHECK(badSteps > 10, "bad-news letter steps are tagged");
  printf(g_fail ? "%d CHECKS FAILED\n" : "ALL CHECKS PASSED\n", g_fail);
  return g_fail ? 1 : 0;
}
int main(int argc, char** argv) {
  if (argc > 1 && std::string(argv[1]) == "render") return RenderMode(argc, argv);
  if (argc > 1 && std::string(argv[1]) == "talk") return TalkMode();
  if (argc > 1 && std::string(argv[1]) == "life") return LifeMode(argc, argv);
  const std::map<std::string, std::vector<std::string>> none;
  struct B { const char* name; RepBattle b; } battles[] = {{"18:45 big", BattleBig()}, {"19:15 quiet", BattleQuiet()}, {"19:33 kills", BattleKills()}, {"21:25 rake", BattleRake()}, {"22:22 sink", BattleSink()}, {"12:10 AA", BattleAA()}, {"12:05 shore", BattleShore()}, {"17:37 Nagara", BattleNagara()}};
  for (auto& x : battles) {
    auto r = RenderReport(x.b, none);
    printf("==== %s ====\n%s\n", x.name, Dump(r).c_str());
    CheckText(x.name, r);
    CHECK(Dump(RenderReport(x.b, none)) == Dump(r), "rendering twice gives the same output");
    {
      RepBattle back;
      snprintf(g_msg, sizeof g_msg, "%s: a battle dump reads back to the same report", x.name);
      CHECK(ParseBattle(DumpBattle(x.b), back) && Dump(RenderReport(back, none)) == Dump(r), g_msg);
    }
    {
      std::string d = Dump(r);
      int openers = 0;
      for (const char* w : {"opened the boat's account", "drew first blood"}) for (size_t p = d.find(w); p != std::string::npos; p = d.find(w, p + 1)) ++openers;
      snprintf(g_msg, sizeof g_msg, "%s: the first kill is claimed %d times", x.name, openers);
      CHECK(openers <= 1, g_msg);
      for (const char* bad : {"and another ", " Ground ", " the IJN ", " and Jack Janovec and ", ": three watch towers", " one other.", " one other,", " two kills: all ",
                              "share of the minesweeper", "kills: all watch towers in the"}) {
        snprintf(g_msg, sizeof g_msg, "%s: '%s' in the text", x.name, bad);
        CHECK(d.find(bad) == std::string::npos, g_msg);
      }
      for (auto& p : r) {
        snprintf(g_msg, sizeof g_msg, "%s: paragraph opens with a pronoun: %.80s", x.name, p.text.c_str());
        CHECK(p.style != 0 || (p.text.compare(0, 3, "He ") != 0 && p.text.compare(0, 4, "She ") != 0), g_msg);
        std::string first = p.text.substr(0, p.text.find(". "));
        snprintf(g_msg, sizeof g_msg, "%s: 'besides' or 'his/her name' with nothing before it: %.80s", x.name, p.text.c_str());
        CHECK(p.style != 0 || (first.find("besides") == std::string::npos && first.find(" his name") == std::string::npos && first.find(" her name") == std::string::npos), g_msg);
      }
    }
  }
  {
    auto r = RenderReport(BattleSynthetic(), none);
    printf("==== synthetic (not from the log) ====\n%s\n", Dump(r).c_str());
    CheckText("synthetic", r);
    std::string d = Dump(r);
    CHECK(d.find("an and") == std::string::npos && d.find(" s ") == std::string::npos, "an unknown enemy kill reads 'an enemy vessel'");
    CHECK(d.find("was her alone") == std::string::npos && d.find("were her alone") == std::string::npos && d.find("hers alone") != std::string::npos, "a woman's solo kills are 'hers alone'");
    CHECK(d.find("# DI HEAVY - ") != std::string::npos, "names are trimmed");
    CHECK(d.find("five kills: five") == std::string::npos, "a list of one unknown type is left out");
    size_t longest = 0;
    for (auto& p : r) if (p.style == 0) longest = (std::max)(longest, p.text.size());
    CHECK(longest < 1600, "overflow keeps a sixty-hit paragraph short");
  }
  {
    RepBattle b = BattleBig();
    std::string a = Dump(RenderReport(b, none));
    b.seed += 1;
    CHECK(Dump(RenderReport(b, none)) != a, "a different seed changes at least one line");
  }
  {
    auto t = ParseReportIni(ReportDefaultIni());
    CHECK((int)t.size() == SectionHeaders(ReportDefaultIni()), "ParseReportIni(ReportDefaultIni()) keeps every section");
    CHECK(t.count("hit.round.1") && t["hit.round.1"].size() >= 4, "hit.round.1 has its rows");
    CHECK(t.count("ships") && t.count("settings"), "ships and settings sections parse");
    auto tables = t;
    tables["summary.action"] = {"TEST summary row."};
    auto r = RenderReport(BattleBig(), tables);
    CHECK(r.size() > 1 && r[1].text.compare(0, 17, "TEST summary row.") == 0, "an overriding section's single row is used");
    std::map<std::string, std::vector<std::string>> quiet = {{"settings", {"report_times=0", "report_numbers=0"}}};
    std::string d = Dump(RenderReport(BattleBig(), quiet));
    CHECK(d.find(":0") == std::string::npos && d.find(" of 2") == std::string::npos, "report_times=0 and report_numbers=0 drop the numbers");
    CheckText("settings off", RenderReport(BattleBig(), quiet));
  }
  {   // 91 s, one tanker, a gunner the mod logged as "on a gun", the whole crew lightly hurt
    RepBattle b;
    b.len = 91; b.seed = 7;
    const char* tk = "Japanese Tanker Sugar Baker Sugar";
    RepCrew g; g.name = "Vincent Randleman"; g.role = "on a gun"; g.maxHp = 300; g.endHp = 296; g.outcome = O_LIGHT;
    g.ev.push_back(Kill(59.6f, tk, tk, "", {}, 1));
    g.grazes.push_back({50.7f, tk, 4});
    RepCrew a; a.name = "George Thomason"; a.role = "passing ammunition"; a.maxHp = 200; a.endHp = 196; a.outcome = O_LIGHT;
    a.grazes.push_back({52.2f, tk, 4});
    b.crew = {g, a};
    b.kills.push_back({1, tk, tk, 59.6f});
    auto r = RenderReport(b, none);
    std::string d = Dump(r);
    printf("==== one tanker ====\n%s\n", d.c_str());
    CheckText("one tanker", r);
    CHECK(d.find("A minute and a half of action against a tanker.") != std::string::npos, "91 s is a minute and a half; one tanker is 'a tanker'");
    CHECK(d.find("# VINCENT RANDLEMAN - gunner\n") != std::string::npos && d.find("on a gun") == std::string::npos, "'on a gun' reads as a gunner");
    CHECK(d.find("(00:59)") != std::string::npos && d.find("(01:00)") == std::string::npos, "the clock truncates like the game log");
    CHECK(d.find("Both were lightly hurt") != std::string::npos, "the whole crew lightly hurt is 'both', not a count");
  }
  BondRulesCheck();                                                   // needs no dumps: in the default run too
  NickChecks();
  printf(g_fail ? "%d CHECKS FAILED\n" : "ALL CHECKS PASSED\n", g_fail);
  return g_fail ? 1 : 0;
}
