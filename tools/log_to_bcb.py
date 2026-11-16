"""log_to_bcb.py: rebuilds .bcb battle dumps (src/crew_report.h, DumpBattle "BCB 1") from the text of
BoatCrewMod/crew-reports.txt, so battles from before the mod dumped them can be re-rendered offline with
tools/out/crew_report_test.exe render <outdir>. Only reports with a "---- the log ----" section are converted.

Usage: python tools/log_to_bcb.py [reports.txt] [outdir]

The log is inverted from CrewLogSummary / CrewLogPost / CrewLogKill / CrewLogPoll (src/weaponmod.cpp) and the events
are built the way CrewRepBattle builds them. What the log does not keep is approximated (see the comments): grazes
(only summed per enemy), the shrapnel part of a round+burst pair, elite names in kill lines, gunBase, role changes.
The input file is only read.
"""
import datetime
import os
import re
import sys
from collections import Counter, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_IN = r"D:/Steam/steamapps/common/Boat Crew/BoatCrewMod/crew-reports.txt"
DEFAULT_OUT = os.path.join(HERE, "battles")

# RepKind / RepOutcome enum numbers (crew_report.h)
(R_ROUND, R_PAIR, R_CLOSE, R_FAR, R_TORPEDO, R_FIRE, R_ABOARD, R_WRECK, R_OTHER, S_WOUNDED, S_DOWN, S_BLEED, S_UP,
 S_OVER_HIT, S_OVER_BLAST, S_SIDE, S_PULLED, S_SANK, S_DIED_WOUNDS, S_KILLED, S_PICKED, S_PRISONER, S_ROLE,
 K_KILL) = range(24)
O_UNHURT, O_LIGHT, O_WOUNDED, O_DOWN_AT_END, O_KIA, O_MISSING, O_CAPTURED, O_LOST_PICKED = range(8)

OUTCOMES = {  # CrewOutcome text -> CrewRepOutcome
    "killed in action": O_KIA, "in the water when the battle ended (missing)": O_MISSING, "captured": O_CAPTURED,
    "lost at sea, then picked up": O_LOST_PICKED, "down when the battle ended": O_DOWN_AT_END, "wounded": O_WOUNDED,
    "lightly hurt": O_LIGHT, "unhurt": O_UNHURT,
}
STATES = {
    "died of their wounds": S_DIED_WOUNDS, "was killed": S_KILLED, "went down with the boat": S_SANK,
    "went over the side": S_SIDE, "picked up after the battle": S_PICKED, "taken prisoner after the battle": S_PRISONER,
    "pulled back aboard": S_PULLED, "collapsed from blood loss": S_BLEED,
}
TIERS = {"a wound": 1, "a serious wound": 2, "a grievous wound": 3}
ROLE_NOUNS = {"ammunition party": "passing ammunition", "damage control": "repairing", "medic": "tending the wounded",
              "helmsman": "at the helm", "standing by": "standing by"}

HEAD_RE = re.compile(r"^==== After-action report, (\d{4})-(\d\d)-(\d\d) (\d\d):(\d\d) \(battle time (\d+):(\d\d)\) ====$")
SUMMARY_RE = re.compile(r"^(.+?) \((.*)\): (" + "|".join(re.escape(k) for k in OUTCOMES) + r")\.(.*)$")
LINE_RE = re.compile(r"^    (\d+):(\d\d)  (.*)$")
TIER_ALT = "a wound|a serious wound|a grievous wound"
HIT_RE = re.compile(r"^(?P<what>.+?)(?:: (?P<tier>" + TIER_ALT + r")|(?P<down> and went down)|, then by its shrapnel: (?P<ptier>"
                    + TIER_ALT + r")|(?P<pdown>, then by its shrapnel, and went down)) \(-(?P<lost>\d+) HP, (?P<hp>-?\d+) of "
                    r"(?P<max>\d+) left\)(?P<rest>.*)$")
ROUND_RE = re.compile(r"^struck(?: in (?P<part>.+?))? by a round(?: \((?P<shell>.*?)\))?(?P<src> from .*)?$")
FIRE_RE = re.compile(r"^burned by fire(?: on (?P<part>.+))?$")
SHRAP_RE = re.compile(r"^hit(?: in (?P<part>.+?))? by shrapnel from a shell(?P<close> that burst close by)?(?: \((?P<src>.*)\))?$")
CAUGHT_RE = re.compile(r"^caught by (?P<how>a torpedo blast|the blast of a dying ship|an explosion aboard|a close blast|a blast|fire)"
                       r"(?P<src> from .*?)?(?:, hit in (?P<part>.+))?$")
KILL_RE = re.compile(r"^killed '(?P<who>.*?)'(?= with the |, shared with |$)(?: with the (?P<gun>.*?))?(?:, shared with (?P<mates>.*))?$")
GROUP_RE = re.compile(r"('(?:[^']|'(?! \())*'|our own guns|fire|explosions aboard|unknown) \((\d+) HP in (\d+) hits?, "
                      r"(\d+):(\d\d)(?:-(\d+):(\d\d))?")
NUM_LEFT_RE = re.compile(r"\((\d+) of (\d+) (?:HP )?left\)")
ANY_LEFT_RE = re.compile(r"(-?\d+) of (\d+) (?:HP )?left\)")   # also the hit lines' "(-16 HP, 221 of 238 left)"
HALF = 0.5   # the log truncates times to whole seconds: put each event mid-second


def shape(s):
    return re.sub(r"\d+", "N", re.sub(r"'[^']*'", "'E'", s))


def esc(s):
    return s.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n").replace("\r", "")


def fnum(v):
    return "%.9g" % v


def ship_bases(text):
    """Plain ship names: every kill line's name (kills use ShipTitle, never the elite name) + crew_report.h [ships]."""
    bases = set(m.group(1) for m in re.finditer(r"^    \d+:\d\d  killed '(.*?)'(?= with the |, shared with |$)", text, re.M))
    try:
        h = open(os.path.join(HERE, "..", "src", "crew_report.h"), encoding="utf-8", errors="replace").read()
        sec = re.search(r"^\[ships\]\n(.*?)^\[", h, re.M | re.S)
        if sec:
            bases.update(m.group(1).strip() for m in re.finditer(r"^([^;#\n=]+?) = [^|\n]+\|", sec.group(1), re.M))
    except OSError:
        pass
    return bases


def base_of(enemy, bases):
    """'Thicc No.1 Aux. Sub. Chaser' -> 'No.1 Aux. Sub. Chaser' (the longest known plain name it ends with)."""
    if enemy in ("", "our own guns"):
        return enemy
    best = ""
    for b in bases:
        if (enemy == b or enemy.endswith(" " + b)) and len(b) > len(best):
            best = b
    return best or enemy


def parse_src(src):
    """CrewFrom's text without the leading ' from ' -> (gun, enemy)."""
    if src is None:
        return "", ""
    m = re.match(r"^our own (.*)$", src)
    if m:
        return ("" if m.group(1) == "guns" else m.group(1)), "our own guns"
    m = re.match(r"^the (.*) of '(.*)'$", src)
    if m:
        return m.group(1), m.group(2)
    m = re.match(r"^'(.*)'$", src)
    if m:
        return "", m.group(1)
    m = re.match(r"^the (.*)$", src)
    if m:
        return m.group(1), ""
    return "", ""


def new_ev(t, kind, **kw):
    e = dict(t=t, kind=kind, tier=1, part="", part2="", enemy="", enemyBase="", gun="", shell="", close=False,
             whileDown=False, lost=0.0, hpLeft=0.0, killId=-1, mates=[])
    e.update(kw)
    return e


def parse_hit(t, text, bases):
    """A narrated hit line -> (events, key for the 'Hurt by' groups, hits it stands for, HP lost, max HP) or None."""
    m = HIT_RE.match(text)
    if not m:
        return None
    what, lost, hp, mx = m.group("what"), float(m.group("lost")), float(m.group("hp")), float(m.group("max"))
    pair = bool(m.group("ptier") or m.group("pdown"))
    downed = bool(m.group("down") or m.group("pdown"))
    tier = 3 if downed else TIERS[m.group("tier") or m.group("ptier")]
    part = shell = gun = enemy = how = ""
    close = False
    r = ROUND_RE.match(what)
    if r:
        kind, part, shell = R_ROUND, r.group("part") or "", r.group("shell") or ""
        gun, enemy = parse_src(r.group("src")[6:] if r.group("src") else None)
    elif FIRE_RE.match(what):
        kind, part = R_FIRE, FIRE_RE.match(what).group("part") or ""
    elif SHRAP_RE.match(what):
        s = SHRAP_RE.match(what)
        close = bool(s.group("close"))
        kind, part = (R_CLOSE if close else R_FAR), s.group("part") or ""
        gun, enemy = parse_src(s.group("src"))
    elif CAUGHT_RE.match(what):
        c = CAUGHT_RE.match(what)
        how, part = c.group("how"), c.group("part") or ""
        kind = {"a torpedo blast": R_TORPEDO, "the blast of a dying ship": R_WRECK, "an explosion aboard": R_ABOARD,
                "fire": R_FIRE}.get(how, R_OTHER)
        gun, enemy = parse_src(c.group("src")[6:] if c.group("src") else None)
    elif what == "hurt":
        kind = R_OTHER
    else:
        return None
    if pair and kind != R_ROUND:
        return None
    if pair:
        kind = R_PAIR   # the round's event becomes the pair; the burst's part (part2) and close flag are not in the log
    rm = re.match(r"^, (.+?)(?:; now wounded)?$", m.group("rest"))   # CrewLogPost's ", " + r->role (not dumped)
    evs = [new_ev(t, kind, tier=tier, part=part, enemy=enemy, enemyBase=base_of(enemy, bases), gun=gun, shell=shell,
                  close=close, lost=lost, hpLeft=hp, role=rm.group(1) if rm else "")]
    if downed:
        evs.append(new_ev(t, S_DOWN))
    elif m.group("rest").endswith("; now wounded"):
        evs.append(new_ev(t, S_WOUNDED))
    # CrewLogPost's dmgBy key
    key = ("our own guns" if enemy == "our own guns" else "'" + enemy + "'" if enemy else "fire" if kind == R_FIRE
           else "explosions aboard" if how == "an explosion aboard" else "unknown")
    return evs, key, (2 if pair else 1), lost, mx


def split_reports(text):
    lines = text.split("\n")
    reps, cur = [], None
    for ln in lines:
        ln = ln.rstrip("\r")
        if HEAD_RE.match(ln):
            cur = {"head": ln, "prose": [], "log": [], "inlog": False}
            reps.append(cur)
        elif cur is not None:
            if ln == "---- the log ----":
                cur["inlog"] = True
            else:
                cur["log" if cur["inlog"] else "prose"].append(ln)
    return [r for r in reps if r["inlog"]]


def crew_blocks(log_lines):
    blocks, skipped = [], []
    for ln in log_lines:
        if not ln.strip():
            continue
        if ln.startswith("    "):
            if blocks:
                blocks[-1]["lines"].append(ln)
            else:
                skipped.append(ln)
            continue
        m = SUMMARY_RE.match(ln)
        if not m:
            skipped.append(ln)
            continue
        blocks.append({"name": m.group(1), "role": m.group(2), "outcome": m.group(3), "tail": m.group(4), "lines": []})
    return blocks, skipped


def prose_by_crew(prose, names):
    """Crew heading 'NAME UPPER - role noun' -> (role noun, the paragraphs under it)."""
    heads = {n.upper() + " - ": n for n in names}
    out, cur = {}, None
    for ln in prose:
        hit = next((n for h, n in heads.items() if ln.startswith(h)), None)
        if hit:
            cur = hit
            out[cur] = [ln[len(hit) + 3:], []]
        elif cur and ln.strip():
            out[cur][1].append(ln)
    return out


def parse_tail(tail, skipped):
    """The rest of a crew summary line -> dict(hits, dmg, grazes, groups, others)."""
    info = {"hits": 0, "dmg": 0.0, "grazes": 0, "groups": []}
    s = tail
    while s:
        m = re.match(r"^ (\d+) hits? for (\d+) HP: (.*?)(?:, (all grazes|a graze|one of them a graze|(\d+) of them grazes))?\.(?= |$)", s)
        if m:
            info["hits"], info["dmg"] = int(m.group(1)), float(m.group(2))
            g = m.group(4)
            info["grazes"] = (info["hits"] if g in ("all grazes", "a graze") else 1 if g == "one of them a graze"
                              else int(m.group(5)) if g else 0)
            s = s[m.end():]
            continue
        m = re.match(r"^ Hurt by (.*?\))( and others)?\.(?= |$)", s)
        if m:
            for g in GROUP_RE.finditer(m.group(1)):
                key = g.group(1)
                t1 = int(g.group(4)) * 60 + int(g.group(5))
                t2 = int(g.group(6)) * 60 + int(g.group(7)) if g.group(6) else t1
                info["groups"].append((key, float(g.group(2)), int(g.group(3)), t1, t2))
            s = s[m.end():]
            continue
        m = re.match(r"^ (?:Not hit\.|Hit \d+ (?:more )?times? while down\.|\d+ kills?(?: \(\d+ shared\))?\.|Went down (?:once|\d+ times)\."
                     r"|Overboard (?:once|\d+ times)\.)", s)
        if m:
            s = s[m.end():]
            continue
        skipped.append("(summary) " + s)
        break
    return info


def convert(rep, bases, global_max, global_fem, when=0.0):
    hm = HEAD_RE.match(rep["head"])
    seed = 2166136261
    for byte in rep["head"].encode("utf-8", "surrogateescape"):
        seed = ((seed ^ byte) * 16777619) & 0xFFFFFFFF
    b = {"len": float(int(hm.group(6)) * 60 + int(hm.group(7))), "seed": seed, "crew": [], "kills": []}
    blocks, skipped = crew_blocks(rep["log"])
    names = [bl["name"] for bl in blocks]
    prose = prose_by_crew(rep["prose"], names)
    stats = Counter()
    kill_evs = []   # (crew index, event, participants)
    for ci, bl in enumerate(blocks):
        info = parse_tail(bl["tail"], skipped)
        c = {"name": bl["name"], "female": False, "role": bl["role"], "gun": "", "gunBase": "", "maxHp": 0.0, "endHp": 0.0,
             "outcome": OUTCOMES[bl["outcome"]], "ev": [], "grazes": []}
        if c["role"] == "not seen aboard":
            c["role"] = "standing by"
        narrated = defaultdict(lambda: [0, 0.0])   # dmgBy key -> [hits, HP] already told as lines
        n_hits, n_lost, maxes = 0, 0.0, []
        last_hp = last_t = None
        for ln in bl["lines"]:
            m = LINE_RE.match(ln)
            if not m:
                skipped.append(ln.strip())
                continue
            t, text = float(int(m.group(1)) * 60 + int(m.group(2))) + HALF, m.group(3)
            if text in STATES:
                c["ev"].append(new_ev(t, STATES[text]))
                continue
            w = re.match(r"^is wounded \((\d+) of (\d+) HP left\)$", text)
            if w:
                c["ev"].append(new_ev(t, S_WOUNDED))
                last_hp, last_t = float(w.group(1)), t
                maxes.append(float(w.group(2)))
                continue
            w = re.match(r"^back on their feet \((\d+) HP\)$", text)
            if w:
                c["ev"].append(new_ev(t, S_UP))
                last_hp, last_t = float(w.group(1)), t
                continue
            w = re.match(r"^thrown overboard by the (hit|blast)( while down)?$", text)
            if w:
                c["ev"].append(new_ev(t, S_OVER_HIT if w.group(1) == "hit" else S_OVER_BLAST, whileDown=bool(w.group(2))))
                continue
            k = KILL_RE.match(text)
            if k:
                mates_s = k.group("mates") or ""
                # CrewJoin(", " / " and "): match the battle's own crew names, in the order written
                found = sorted((mates_s.find(n), n) for n in names if n != c["name"] and re.search(r"(^|, | and )" + re.escape(n) + r"(?=$|, | and )", mates_s))
                mates = [n for _, n in found]
                if mates_s and not mates:
                    skipped.append("(kill mates) " + text)
                e = new_ev(t, K_KILL, enemy=k.group("who"), enemyBase=k.group("who"), gun=k.group("gun") or "", mates=mates)
                c["ev"].append(e)
                kill_evs.append((ci, e, frozenset([c["name"]] + mates)))
                continue
            h = parse_hit(t, text, bases)
            if h:
                evs, key, nh, lost, mx = h
                c["ev"].extend(evs)
                narrated[key][0] += nh
                narrated[key][1] += lost
                n_hits += nh
                n_lost += lost
                maxes.append(mx)
                last_hp, last_t = evs[0]["hpLeft"], t
                continue
            skipped.append(text)
        # grazes: the hits that got no line, spread over each enemy's span from the 'Hurt by' groups
        g_total = max(0, info["hits"] - n_hits)
        g_hp = max(0.0, info["dmg"] - n_lost)
        listed_n, listed_hp = 0, 0.0
        for key, hp, n, t1, t2 in info["groups"]:
            gn = min(max(0, n - narrated[key][0]), g_total - listed_n)
            ghp = max(0.0, hp - narrated[key][1])
            if gn <= 0:
                continue
            enemy = key[1:-1] if key.startswith("'") else key if key in ("our own guns", "fire") else ""
            for i in range(gn):
                c["grazes"].append({"t": (float(t1 + (t2 - t1) * i / (gn - 1)) if gn > 1 else float(t1)) + HALF, "enemy": enemy, "hp": ghp / gn})
            listed_n += gn
            listed_hp += ghp
        rest_n, rest_hp = g_total - listed_n, max(0.0, g_hp - listed_hp)
        for i in range(rest_n):   # 'and others' and rounding: unknown enemy, spread over the battle
            c["grazes"].append({"t": b["len"] * (i + 0.5) / rest_n, "enemy": "", "hp": rest_hp / rest_n, "syn": True})
        c["grazes"].sort(key=lambda g: g["t"])
        if info["grazes"] and g_total < info["grazes"]:
            # the mod counts a small burst of a narrated round+burst pair as a graze too (no graze event): expected
            stats["graze count below the summary's"] += 1
        stats["grazes"] += len(c["grazes"])
        # prose: female, first role, end HP
        pr = prose.get(c["name"])
        if pr:
            words = " ".join(pr[1])
            she = len(re.findall(r"\b(?:she|her|She|Her)\b", words))
            he = len(re.findall(r"\b(?:he|his|him|He|His|Him)\b", words))
            c["female"] = she > he if she or he else global_fem.get(c["name"], False)
            noun = pr[0]   # RoleNoun(c): the first job (RepCrew.role is firstRole); a gunner's gun is c.gun
            c["role"] = "on the " + noun[8:] if noun.startswith("gunner, ") else ROLE_NOUNS.get(noun, noun)
            nums = [NUM_LEFT_RE.search(p) for p in pr[1]]
            nums = [x for x in nums if x]
            if nums:
                c["endHp"], c["maxHp"] = float(nums[-1].group(1)), float(nums[-1].group(2))
        else:
            c["female"] = global_fem.get(c["name"], False)
            ev_role = next((e["role"] for e in c["ev"] if e.get("role")), "")
            kill_gun = next((e["gun"] for e in c["ev"] if e["kind"] == K_KILL and e["gun"]), "")
            c["role"] = ev_role or ("on the " + kill_gun if kill_gun else c["role"])
        if not c["maxHp"]:
            # shortcut: 200 when this crew member has no HP figure anywhere in the file (an unhurt man never logs one)
            near = min((x for x in global_max.get(c["name"], []) if x[0] != when), key=lambda x: (abs(x[0] - when), x[0]), default=None)
            c["maxHp"] = max(maxes) if maxes else near[1] if near else 200.0
        if not nums_or(pr):
            # no prose figure: KIA/down = 0 (CrewRepBattle's lastHp clamped); else the last logged HP less the grazes after it
            if c["outcome"] in (O_KIA, O_DOWN_AT_END):
                c["endHp"] = 0.0
            else:
                # shortcut: a synthetic 'and others' graze has no real time, so it only counts when there is no figure at all
                if last_hp is None:
                    hp = c["maxHp"] - info["dmg"]
                else:
                    hp = last_hp - sum(g["hp"] for g in c["grazes"] if not g.get("syn") and g["t"] > last_t)
                lost = c["outcome"] in (O_MISSING, O_CAPTURED, O_LOST_PICKED)
                c["endHp"] = max(0.0 if lost else 1.0, min(hp, c["maxHp"]))
        c["gun"] = c["role"][7:] if c["role"].startswith("on the ") else ""
        b["crew"].append(c)
    # kills: one id per distinct kill (same enemy, same credited crew, within 2 s, each crew member once)
    groups = []
    for ci, e, who in sorted(kill_evs, key=lambda x: x[1]["t"]):
        g = next((g for g in groups if g["enemy"] == e["enemy"] and g["who"] == who and abs(e["t"] - g["t"]) <= 2 and ci not in g["crew"]), None)
        if not g:
            g = {"enemy": e["enemy"], "who": who, "t": e["t"], "crew": set(), "id": len(groups)}
            groups.append(g)
        g["crew"].add(ci)
        e["killId"] = g["id"]   # not groups.index(g): two kills of one name at the same second compare equal
    for i, g in enumerate(groups):
        b["kills"].append({"id": i, "t": g["t"], "enemy": g["enemy"], "enemyBase": g["enemy"]})
        if len(g["crew"]) != len(g["who"]):
            stats["kill credits missing a partner"] += 1
    for c in b["crew"]:   # a gunner is named for the gun that made most of his kills (CrewRepBattle)
        n, best, bn = Counter(), "", 0
        for e in c["ev"]:
            if e["kind"] == K_KILL and e["gun"]:
                n[e["gun"]] += 1
                if n[e["gun"]] > bn:
                    bn, best = n[e["gun"]], e["gun"]
        if best:
            c["gun"] = best
        elif not c["gun"]:
            c["gun"] = next((e["gun"] for e in c["ev"] if e["kind"] == K_KILL and e["gun"]), "")
        # shortcut: gunBase (FindItem(gun).base) is not in the log; left empty, the renderer then says the full gun name
    stats["crew"] = len(b["crew"])
    stats["events"] = sum(len(c["ev"]) for c in b["crew"])
    stats["kills"] = len(b["kills"])
    return b, stats, skipped


def when_of(head):
    """Report heading -> minutes since 2000-01-01 (for the nearest battle's HP figure)."""
    y, mo, d, h, mi = (int(x) for x in HEAD_RE.match(head).groups()[:5])
    return (datetime.datetime(y, mo, d, h, mi) - datetime.datetime(2000, 1, 1)).total_seconds() / 60


def nums_or(pr):
    return bool(pr and any(NUM_LEFT_RE.search(p) for p in pr[1]))


def dump(b):
    o = ["BCB 1", "battle\t%s\t%d" % (fnum(b["len"]), b["seed"])]
    for k in b["kills"]:
        o.append("kill\t%d\t%s\t%s\t%s" % (k["id"], fnum(k["t"]), esc(k["enemy"]), esc(k["enemyBase"])))
    for c in b["crew"]:
        o.append("\t".join(["crew", esc(c["name"]), "1" if c["female"] else "0", esc(c["role"]), esc(c["gun"]), esc(c["gunBase"]),
                            fnum(c["maxHp"]), fnum(c["endHp"]), str(c["outcome"])]))
        for e in c["ev"]:
            o.append("\t".join(["ev", fnum(e["t"]), str(e["kind"]), str(e["tier"]), esc(e["part"]), esc(e["part2"]), esc(e["enemy"]),
                                esc(e["enemyBase"]), esc(e["gun"]), esc(e["shell"]), "1" if e["close"] else "0",
                                "1" if e["whileDown"] else "0", fnum(e["lost"]), fnum(e["hpLeft"]), str(e["killId"]),
                                "\x1f".join(esc(m) for m in e["mates"])]))
        for g in c["grazes"]:
            o.append("gz\t%s\t%s\t%s" % (fnum(g["t"]), esc(g["enemy"]), fnum(g["hp"])))
    return "\n".join(o) + "\n"


def self_check():
    bases = {"No.1 Aux. Sub. Chaser", "IJN Patrol T1"}
    assert base_of("Thicc No.1 Aux. Sub. Chaser", bases) == "No.1 Aux. Sub. Chaser"
    evs, key, n, lost, mx = parse_hit(5, "struck in the head by a round (57 mm Tank Round (JAP)) from the Type 96 of 'IJN Patrol T1', then by its "
                                         "shrapnel: a grievous wound (-90 HP, 20 of 200 left), repairing; now wounded", bases)
    assert evs[0]["kind"] == R_PAIR and evs[0]["shell"] == "57 mm Tank Round (JAP)" and evs[0]["gun"] == "Type 96" and evs[1]["kind"] == S_WOUNDED
    assert key == "'IJN Patrol T1'" and n == 2 and lost == 90 and mx == 200 and evs[0]["role"] == "repairing"
    rep = {"head": "==== After-action report, 2026-11-11 17:00 (battle time 01:00) ====", "prose": [], "log": [
        "Al Bo (on the Gun B): wounded. 3 hits for 35 HP: 1 round and 2 blasts, 2 of them grazes. Hurt by 'X' (35 HP in 3 hits, 00:10-00:20).",
        "    00:10  struck by a round from 'X': a serious wound (-25 HP, 75 of 100 left), on the Gun A; now wounded"]}
    b, _, _ = convert(rep, set(), {}, {})
    c = b["crew"][0]
    assert c["role"] == "on the Gun A" and c["ev"][0]["t"] == 10.5 and c["grazes"][1]["t"] == 20.5 and c["endHp"] == 70 and c["maxHp"] == 100
    rep["log"] = ["Al Bo (on the Gun B): unhurt. Not hit."]
    assert convert(rep, set(), {"Al Bo": [(0, 300.0), (when_of(rep["head"]) - 5, 250.0)]}, {}, when_of(rep["head"]))[0]["crew"][0]["maxHp"] == 250
    evs, key, n, _, _ = parse_hit(5, "hit in the left leg by shrapnel from a shell that burst close by (our own guns) and went down "
                                     "(-30 HP, 0 of 200 left), at the helm", bases)
    assert evs[0]["kind"] == R_CLOSE and evs[0]["close"] and evs[0]["tier"] == 3 and evs[1]["kind"] == S_DOWN and key == "our own guns"
    info = parse_tail(" 3 hits for 10 HP: 3 blasts, all grazes. Hurt by 'No.1 Aux. Sub. Chaser' (10 HP in 3 hits, 01:00-01:10, its "
                      "20 mm Cannon AIR (Japan) and X) and others. 2 kills (1 shared). Overboard once.", [])
    assert info["hits"] == 3 and info["grazes"] == 3 and info["groups"] == [("'No.1 Aux. Sub. Chaser'", 10.0, 3, 60, 70)]


def main():
    self_check()
    src = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_IN
    out = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUT
    with open(src, encoding="utf-8", errors="surrogateescape", newline="") as f:
        text = f.read().replace("\r\n", "\n")
    bases = ship_bases(text)
    global_max, global_fem = {}, {}
    reps = split_reports(text)
    # HP maxima per battle (name -> [(when, max)]) and genders, for crew with no figure in their own battle
    for rep in reps:
        rep["when"] = when_of(rep["head"])
        blocks, _ = crew_blocks(rep["log"])
        mx = defaultdict(float)
        for bl in blocks:
            for ln in bl["lines"]:
                for x in ANY_LEFT_RE.finditer(ln):
                    mx[bl["name"]] = max(mx[bl["name"]], float(x.group(2)))
        for n, (noun, paras) in prose_by_crew(rep["prose"], [bl["name"] for bl in blocks]).items():
            words = " ".join(paras)
            she = len(re.findall(r"\b(?:she|her|She|Her)\b", words))
            he = len(re.findall(r"\b(?:he|his|him|He|His|Him)\b", words))
            if she or he:
                global_fem[n] = she > he
            for x in NUM_LEFT_RE.finditer(words):
                mx[n] = max(mx[n], float(x.group(2)))
        for n, v in mx.items():
            global_max.setdefault(n, []).append((rep["when"], v))
    os.makedirs(out, exist_ok=True)
    shapes = Counter()
    written = 0
    for rep in reps:
        hm = HEAD_RE.match(rep["head"])
        name = "%s-%s-%s_%s%s.bcb" % hm.groups()[:5]
        b, st, skipped = convert(rep, bases, global_max, global_fem, rep["when"])
        with open(os.path.join(out, name), "w", encoding="utf-8", errors="surrogateescape", newline="\n") as f:
            f.write(dump(b))
        written += 1
        for s in skipped:
            shapes[shape(s)] += 1
        extra = "".join(", %s %d" % (k, v) for k, v in st.items() if k not in ("crew", "events", "grazes", "kills"))
        print("%s: %d crew, %d events, %d grazes, %d kills, %d skipped lines%s" % (name, st["crew"], st["events"], st["grazes"], st["kills"], len(skipped), extra))
    print("%d battle(s) written to %s" % (written, out))
    if shapes:
        print("skipped line shapes:")
        for s, n in shapes.most_common():
            print("  %4d  %s" % (n, s))
    else:
        print("no skipped lines")


if __name__ == "__main__":
    main()
