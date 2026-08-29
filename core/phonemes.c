//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Letter-to-sound rules for `say` (P16 M2). See phonemes.h and
//  docs/say-design.md §7.
//
//  A rule is written the way NRL Report 7948 writes it:
//
//      left-context [ item ] right-context = phonemes
//
//  and the scan is left to right: at each letter, try the rules bucketed
//  under it in order, and the first whose contexts match wins -- its item is
//  consumed and the scan resumes after it. Bucketing is why 300-odd rules
//  cost about a dozen comparisons a letter rather than 300.
//
//  Context characters, from the report:
//
//      #   one or more vowels (AEIOUY)
//      :   zero or more consonants
//      ^   one consonant
//      .   one voiced consonant (BDVGJLMNRWZ)
//      +   one front vowel (E, I, Y)
//      %   a suffix: E, ER, ES, ED, ELY, ING (right context only)
//      &   one sibilant (S C G Z X J, or CH/SH)
//      @   a consonant that colours a following long U (T S R D L Z N J,
//          or TH/CH/SH)
//      ' ' a word boundary
//
//  Left contexts are matched right to left from the item, right contexts
//  left to right, and neither backtracks -- which is the algorithm as
//  published, and the reason rule order inside a bucket is load-bearing.
//

#include "phonemes.h"
#include "speech_synth.h"

#include <stdbool.h>
#include <string.h>

//==========================================================================
// The text, as the rules see it
//==========================================================================

typedef struct Text
{
    const char *s;
    int len;
} Text;

// Letters fold to upper case, apostrophes stand, and every other character
// -- digits, punctuation, the ends of the string -- reads as a word
// boundary. That is what lets the rules run straight off the caller's string
// with no normalized copy of it anywhere.
static char fold(char c)
{
    if (c >= 'a' && c <= 'z')
    {
        return (char)(c - 'a' + 'A');
    }
    return ((c >= 'A' && c <= 'Z') || c == '\'') ? c : ' ';
}

static char at(Text t, int i)
{
    return (i < 0 || i >= t.len) ? ' ' : fold(t.s[i]);
}

static bool is_vowel(char c)
{
    return c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U' || c == 'Y';
}

static bool is_consonant(char c)
{
    return c >= 'A' && c <= 'Z' && !is_vowel(c);
}

static bool is_voiced(char c)
{
    return c != ' ' && strchr("BDVGJLMNRWZ", c) != NULL;
}

static bool is_sibilant(char c)
{
    return c != ' ' && strchr("SCGZXJ", c) != NULL;
}

static bool is_long_u_consonant(char c)
{
    return c != ' ' && strchr("TSRDLZNJ", c) != NULL;
}

// Length of the suffix at i, for `%`: E, ER, ES, ED, ELY, ING. 0 if none.
static int suffix_len(Text t, int i)
{
    if (at(t, i) == 'E')
    {
        char c = at(t, i + 1);
        if (c == 'L' && at(t, i + 2) == 'Y')
        {
            return 3;
        }
        return (c == 'R' || c == 'S' || c == 'D') ? 2 : 1;
    }
    if (at(t, i) == 'I' && at(t, i + 1) == 'N' && at(t, i + 2) == 'G')
    {
        return 3;
    }
    return 0;
}

//==========================================================================
// Context matching
//==========================================================================

// Right context: `pat`..`end` against the text from i forward.
static bool match_right(Text t, int i, const char *pat, const char *end)
{
    for (const char *p = pat; p < end; p++)
    {
        char c = at(t, i);
        switch (*p)
        {
        case '#':
            if (!is_vowel(c))
            {
                return false;
            }
            do
            {
                i++;
            } while (is_vowel(at(t, i)));
            break;
        case ':':
            while (is_consonant(at(t, i)))
            {
                i++;
            }
            break;
        case '^':
            if (!is_consonant(c))
            {
                return false;
            }
            i++;
            break;
        case '.':
            if (!is_voiced(c))
            {
                return false;
            }
            i++;
            break;
        case '+':
            if (c != 'E' && c != 'I' && c != 'Y')
            {
                return false;
            }
            i++;
            break;
        case '%':
        {
            int n = suffix_len(t, i);
            if (n == 0)
            {
                return false;
            }
            i += n;
            break;
        }
        case '&':
            if (is_sibilant(c))
            {
                i++;
            }
            else if ((c == 'C' || c == 'S') && at(t, i + 1) == 'H')
            {
                i += 2;
            }
            else
            {
                return false;
            }
            break;
        case '@':
            if (is_long_u_consonant(c))
            {
                i++;
            }
            else if ((c == 'T' || c == 'C' || c == 'S') && at(t, i + 1) == 'H')
            {
                i += 2;
            }
            else
            {
                return false;
            }
            break;
        default:
            if (c != *p)
            {
                return false;
            }
            i++;
            break;
        }
    }
    return true;
}

// Left context: `pat`..`end` against the text from i backward, the pattern
// itself read backward.
static bool match_left(Text t, int i, const char *pat, const char *end)
{
    for (const char *p = end - 1; p >= pat; p--)
    {
        char c = at(t, i);
        switch (*p)
        {
        case '#':
            if (!is_vowel(c))
            {
                return false;
            }
            do
            {
                i--;
            } while (is_vowel(at(t, i)));
            break;
        case ':':
            while (is_consonant(at(t, i)))
            {
                i--;
            }
            break;
        case '^':
            if (!is_consonant(c))
            {
                return false;
            }
            i--;
            break;
        case '.':
            if (!is_voiced(c))
            {
                return false;
            }
            i--;
            break;
        case '+':
            if (c != 'E' && c != 'I' && c != 'Y')
            {
                return false;
            }
            i--;
            break;
        case '%':
            return false; // a suffix is a right-context form only
        case '&':
            if (is_sibilant(c))
            {
                i--;
            }
            else if (c == 'H' && (at(t, i - 1) == 'C' || at(t, i - 1) == 'S'))
            {
                i -= 2;
            }
            else
            {
                return false;
            }
            break;
        case '@':
            if (is_long_u_consonant(c))
            {
                i--;
            }
            else if (c == 'H' && (at(t, i - 1) == 'T' || at(t, i - 1) == 'C' || at(t, i - 1) == 'S'))
            {
                i -= 2;
            }
            else
            {
                return false;
            }
            break;
        default:
            if (c != *p)
            {
                return false;
            }
            i--;
            break;
        }
    }
    return true;
}

//==========================================================================
// The rules, bucketed by the item's first letter
//==========================================================================
//
// Transcribed from NRL Report 7948's appendix. Phoneme names are the §6
// ARPABET ones, lower case, which is what `phonemes` prints and what
// `sayphonemes` reads back -- so a rule's right-hand side is a Logo phoneme
// list, and a reader can check one against the report line by line.
//
// The report's compound symbols expand here: UL, UM and UN are schwa plus
// the consonant, NX is `ng`, WH is `w` (we have no voiceless w), and its
// J and H are `jh` and `hh`.

static const char *const rules_a[] = {
    " [A] =ax",
    " [ARE] =aa r",
    " [AR]O=ax r",
    "[AR]#=eh r",
    "^[AS]#=ey s",
    "[A]WA=ax",
    "[AW]=ao",
    " :[ANY]=eh n iy",
    "[A]^+#=ey",
    "#:[ALLY]=ax l iy",
    " [AL]#=ax l",
    "[AGAIN]=ax g eh n",
    "#:[AG]E=ih jh",
    "[A]^%=ey",
    "[A]^+:#=ae",
    " :[A]^+ =ey",
    " [ARR]=ax r",
    "[ARR]=ae r",
    " ^[AR] =aa r",
    "[AR]=aa r",
    "[AIR]=eh r",
    "[AI]=ey",
    "[AY]=ey",
    "[AU]=ao",
    "#:[AL] =ax l",
    "#:[ALS] =ax l z",
    "[ALK]=ao k",
    "[A]L^=ao",
    " :[ABLE]=ey b ax l",
    "[ABLE]=ax b ax l",
    "[A]VO=ey",
    "[ANG]+=ey n jh",
    "[A]TOM=ae",
    "[A]TTI=ae",
    " [AT] =ae t",
    " [A]T=ax",
    "[A]=ae",
    NULL};

static const char *const rules_b[] = {
    " [B] =b iy",
    " [BE]^#=b ih",
    "[BEING]=b iy ih ng",
    " [BOTH] =b ow th",
    " [BUS]#=b ih z",
    "[BUIL]=b ih l",
    "B[B]=",
    "[B]=b",
    NULL};

static const char *const rules_c[] = {
    " [C] =s iy",
    " [CH]^=k",
    "^E[CH]=k",
    "[CH]=ch",
    " S[CI]#=s ay",
    "[CI]A=sh",
    "[CI]O=sh",
    "[CI]EN=sh",
    "[CITY]=s ih t iy",
    "[C]+=s",
    "[CK]=k",
    "[COM]%=k ah m",
    "[C]=k",
    NULL};

static const char *const rules_d[] = {
    " [D] =d iy",
    "#:[DED] =d ih d",
    ".E[D] =d",
    "#:^E[D] =t",
    " [DE]^#=d ih",
    " [DO] =d uw",
    " [DOES]=d ah z",
    "[DONE] =d ah n",
    "[DOING]=d uw ih ng",
    " [DOW]=d aw",
    "#[DU]A=jh uw",
    "#[DU]^#=jh ax",
    "D[D]=",
    "[D]=d",
    NULL};

static const char *const rules_e[] = {
    " [E] =iy",
    "#:[E] =",
    " :[E] =iy",
    "#[ED] =d",
    "#:[E]D =",
    "[EV]ER=eh v",
    "[E]^%=iy",
    "[ERI]#=iy r iy",
    "[ERI]=eh r ih",
    "#:[ER]#=er",
    "[ER]#=eh r",
    "[ER]=er",
    " [EVEN]=iy v eh n",
    "#:[E]W=",
    "@[EW]=uw",
    "[EW]=y uw",
    "[E]O=iy",
    "#:&[ES] =ih z",
    "#:[E]S =",
    "#:[ELY] =l iy",
    "#:[EMENT]=m eh n t",
    "[EFUL]=f uh l",
    "[EE]=iy",
    "[EARN]=er n",
    " [EAR]^=er",
    "[EAD]=eh d",
    "#:[EA] =iy ax",
    "[EA]SU=eh",
    "[EA]=iy",
    "[EIGH]=ey",
    "[EI]=iy",
    " [EYE]=ay",
    "[EY]=iy",
    "[EU]=y uw",
    "[E]=eh",
    NULL};

static const char *const rules_f[] = {
    " [F] =eh f",
    "[FUL]=f uh l",
    "F[F]=",
    "[F]=f",
    NULL};

static const char *const rules_g[] = {
    " [G] =jh iy",
    "[GIV]=g ih v",
    " [G]I^=g",
    "[GE]T=g eh",
    "SU[GGES]=g jh eh s",
    "[GG]=g",
    " B#[G]=g",
    "[G]+=jh",
    "[GREAT]=g r ey t",
    "#[GH]=",
    "[G]=g",
    NULL};

static const char *const rules_h[] = {
    " [H] =ey ch",
    " [HAV]=hh ae v",
    " [HERE]=hh iy r",
    " [HOUR]=aw er",
    "[HOW]=hh aw",
    "[H]#=hh",
    "[H]=",
    NULL};

static const char *const rules_i[] = {
    " [IN]=ih n",
    " [I] =ay",
    "[I] =ay",
    "[IN]D=ay n",
    "SEM[I]=iy",
    " ANT[I]=ay",
    "[IER]=iy er",
    "#:R[IED] =iy d",
    "[IED] =ay d",
    "[IEN]=iy eh n",
    "[IE]T=ay eh",
    " :[I]^%=ay",
    " :[IE] =ay",
    "[I]%=iy",
    "[IE]=iy",
    " [IDEA]=ay d iy ax",
    "[I]^+:#=ih",
    "[IR]#=ay r",
    "[IZ]%=ay z",
    "[IS]%=ay z",
    "I^[I]^#=ih",
    "+^[I]^+=ay",
    "#:^[I]^+=ih",
    "[I]^+=ay",
    "[IR]=er",
    "[IGH]=ay",
    "[ILD]=ay l d",
    " [IGN]=ih g n",
    "[IGN] =ay n",
    "[IGN]^=ay n",
    "[IGN]%=ay n",
    "[IQUE]=iy k",
    "[I]=ih",
    NULL};

static const char *const rules_j[] = {
    " [J] =jh ey",
    "[J]=jh",
    NULL};

static const char *const rules_k[] = {
    " [K] =k ey",
    " [K]N=",
    "[K]=k",
    NULL};

static const char *const rules_l[] = {
    " [L] =eh l",
    "[LO]C#=l ow",
    "L[L]=",
    "#:^[L]%=ax l",
    "[LEAD]=l iy d",
    " [LAUGH]=l ae f",
    "[L]=l",
    NULL};

static const char *const rules_m[] = {
    " [M] =eh m",
    "[MOV]=m uw v",
    "[MACHIN]=m ax sh iy n",
    "M[M]=",
    "[M]=m",
    NULL};

static const char *const rules_n[] = {
    " [N] =eh n",
    "E[NG]+=n jh",
    "[NG]R=ng g",
    "[NG]#=ng g",
    "[NGL]%=ng g ax l",
    "[NG]=ng",
    "[NK]=ng k",
    " [NOW] =n aw",
    "N[N]=",
    "[NON]E=n ah n",
    "[N]=n",
    NULL};

static const char *const rules_o[] = {
    " [O] =ow",
    "[OF] =ah v",
    " [OH] =ow",
    "[OROUGH]=er ow",
    "#:[OR] =er",
    "#:[ORS] =er z",
    "[OR]=ao r",
    " [ONE]=w ah n",
    "#[ONE] =w ah n",
    "[OW]=ow",
    " [OVER]=ow v er",
    "PR[O]V=uw",
    "[OV]=ah v",
    "[O]^%=ow",
    "[O]^EN=ow",
    "[O]^I#=ow",
    "[OL]D=ow l",
    "[OUGHT]=ao t",
    "[OUGH]=ah f",
    " [OU]=aw",
    "H[OU]S#=aw",
    "[OUS]=ax s",
    "[OUR]=ao r",
    "[OULD]=uh d",
    "^[OU]^L=ah",
    "[OUP]=uw p",
    "[OU]=aw",
    "[OY]=oy",
    "[OING]=ow ih ng",
    "[OI]=oy",
    "[OOR]=ao r",
    "[OOK]=uh k",
    "F[OOD]=uw d",
    "L[OOD]=ah d",
    "M[OOD]=uw d",
    "[OOD]=uh d",
    "F[OOT]=uh t",
    "[OO]=uw",
    "[O']=ow",
    "[O]E=ow",
    "[O] =ow",
    "[OA]=ow",
    " [ONLY]=ow n l iy",
    " [ONCE]=w ah n s",
    "[ON'T]=ow n t",
    "C[O]N=aa",
    "[O]NG=ao",
    " :^[O]N=ah",
    "I[ON]=ax n",
    "#:[ON] =ax n",
    "#^[ON]=ax n",
    "[O]ST =ow",
    "[OF]^=ao f",
    "[OTHER]=ah dh er",
    "R[O]B=aa",
    "^R[O]:#=ow",
    "[OSS] =ao s",
    "#[O]M=ah",
    "[O]=aa",
    NULL};

static const char *const rules_p[] = {
    " [P] =p iy",
    "[PH]=f",
    "[PEOP]=p iy p",
    "[POW]=p aw",
    "[PUT] =p uh t",
    "P[P]=",
    "[P]=p",
    NULL};

static const char *const rules_q[] = {
    " [Q] =k y uw",
    "[QUAR]=k w ao r",
    "[QU]=k w",
    "[Q]=k",
    NULL};

static const char *const rules_r[] = {
    " [R] =aa r",
    " [RE]^#=r iy",
    "R[R]=",
    "[R]=r",
    NULL};

static const char *const rules_s[] = {
    " [S] =eh s",
    "[SH]=sh",
    "#[SION]=zh ax n",
    "[SOME]=s ah m",
    "#[SUR]#=zh er",
    "[SUR]#=sh er",
    "#[SU]#=zh uw",
    "#[SSU]#=sh uw",
    "#[SED] =z d",
    "#[S]#=z",
    "[SAID]=s eh d",
    "^[SION]=sh ax n",
    "[S]S=",
    ".[S] =z",
    "#:.E[S] =z",
    "U[S] =s",
    "#[S] =z",
    " [SCH]=s k",
    "[S]C+=",
    "#[SM]=z ax m",
    "#[SN]'=z ax n",
    "[STLE]=s ax l",
    "[S]=s",
    NULL};

static const char *const rules_t[] = {
    " [T] =t iy",
    " [THE] #=dh iy",
    " [THE] =dh ax",
    "[TO] =t uw",
    " [THAT]=dh ae t",
    " [THIS] =dh ih s",
    " [THEY]=dh ey",
    " [THERE]=dh eh r",
    "[THEIR]=dh eh r",
    " [THAN] =dh ae n",
    " [THEM] =dh eh m",
    "[THESE] =dh iy z",
    " [THEN]=dh eh n",
    "[THROUGH]=th r uw",
    "[THOSE]=dh ow z",
    "[THOUGH] =dh ow",
    " [THUS]=dh ah s",
    "[THER]=dh er",
    "[TH]=th",
    "#:[TED] =t ih d",
    "S[TI]#N=ch",
    "[TI]O=sh",
    "[TI]A=sh",
    "[TIEN]=sh ax n",
    "[TUR]#=ch er",
    "[TU]A=ch uw",
    " [TWO]=t uw",
    "T[T]=",
    "[T]=t",
    NULL};

static const char *const rules_u[] = {
    " [U] =y uw",
    " [UN]I=y uw n",
    " [UN]=ah n",
    " [UPON]=ax p ao n",
    "@[UR]#=uh r",
    "[UR]#=y uh r",
    "[UR]=er",
    "[U]^ =ah",
    "[U]^^=ah",
    "[UY]=ay",
    " G[U]#=",
    "G[U]%=",
    "G[U]#=w",
    "#N[U]=y uw",
    "@[U]=uw",
    "[U]=y uw",
    NULL};

static const char *const rules_v[] = {
    " [V] =v iy",
    "[VIEW]=v y uw",
    "[V]=v",
    NULL};

static const char *const rules_w[] = {
    " [W] =d ah b ax l y uw",
    " [WERE]=w er",
    "[WA]SH=w aa",
    "[WA]ST=w ey",
    "[WA]S=w ah",
    "[WA]T=w aa",
    "[WHERE]=w eh r",
    "[WHAT]=w ah t",
    "[WHOL]=hh ow l",
    "[WHO]=hh uw",
    "[WH]=w",
    "[WAR]#=w eh r",
    "[WAR]=w ao r",
    "[WOR]^=w er",
    "[WR]=r",
    "[WOM]A=w uh m",
    "[WOM]E=w ih m",
    "[WEA]R=w eh",
    "[WANT]=w aa n t",
    "ANS[WER]=er",
    "[W]=w",
    NULL};

static const char *const rules_x[] = {
    " [X] =eh k s",
    " [X]=z",
    "[X]=k s",
    NULL};

static const char *const rules_y[] = {
    " [Y] =w ay",
    "[YOUNG]=y ah ng",
    " [YOUR]=y ao r",
    " [YOU]=y uw",
    " [YES]=y eh s",
    " [Y]=y",
    "F[Y]=ay",
    "PS[YCH]=ay k",
    "#:^[Y] =iy",
    "#:^[Y]I=iy",
    " :[Y] =ay",
    " :[Y]#=ay",
    " :[Y]^+:#=ih",
    " :[Y]^#=ay",
    "[Y]=ih",
    NULL};

static const char *const rules_z[] = {
    " [Z] =z iy",
    "Z[Z]=",
    "[Z]=z",
    NULL};

// Contractions. The apostrophe is a letter to the rules (they match on it)
// but a word of its own to the scan, so its clitics live in their own
// bucket rather than being spelled out in every verb's.
static const char *const rules_apostrophe[] = {
    ".['S] =z",
    "['S] =z",
    "['T] =t",
    "['LL] =l",
    "['VE] =v",
    "['RE] =er",
    "['M] =m",
    "['D] =d",
    "[']=",
    NULL};

static const char *const *const rule_buckets[27] = {
    rules_a, rules_b, rules_c, rules_d, rules_e, rules_f, rules_g,
    rules_h, rules_i, rules_j, rules_k, rules_l, rules_m, rules_n,
    rules_o, rules_p, rules_q, rules_r, rules_s, rules_t, rules_u,
    rules_v, rules_w, rules_x, rules_y, rules_z, rules_apostrophe};

//==========================================================================
// The exception list
//==========================================================================
//
// The words the rules get wrong and that matter, checked whole-word before
// the rules run (§7). It grows by test failure -- every entry here is a word
// tests/test_phonemes.c caught the rules mispronouncing -- rather than by
// imagination, which is the only way to keep it from becoming a dictionary
// and the item from becoming bottomless (§14 R5).

typedef struct PhonemeException
{
    const char *word;
    const char *phonemes;
} PhonemeException;

static const PhonemeException exceptions[] = {
    {"father", "f aa dh er"},
    {"house", "hh aw s"},
    {"live", "l ih v"},
    {"of", "ah v"},
    {"once", "w ah n s"},
    {"one", "w ah n"},
    {"people", "p iy p ax l"},
    {"river", "r ih v er"},
    {"said", "s eh d"},
    {"study", "s t ah d iy"},
    {"two", "t uw"},
};

#define EXCEPTION_COUNT ((int)(sizeof exceptions / sizeof exceptions[0]))

// Digits are spoken one at a time, per §5.1: `say 42` is "four two", not
// "forty two". The rules never see them.
static const char *const digit_names[10] = {
    "z iy r ow", "w ah n", "t uw", "th r iy", "f ao r",
    "f ay v", "s ih k s", "s eh v ax n", "ey t", "n ay n"};

//==========================================================================
// The scan
//==========================================================================

// Append the phoneme names in `p` (a rule's right-hand side, space
// separated) to `out`. Returns the new count.
static int emit(const char *p, uint8_t *out, int max_out, int n)
{
    while (*p)
    {
        while (*p == ' ')
        {
            p++;
        }
        char name[4];
        int k = 0;
        while (*p && *p != ' ' && k < (int)sizeof name - 1)
        {
            name[k++] = *p++;
        }
        if (k == 0)
        {
            break;
        }
        name[k] = '\0';
        int id = speech_phoneme_from_name(name);
        if (id >= 0 && n < max_out)
        {
            out[n++] = (uint8_t)id;
        }
    }
    return n;
}

// Characters the rules own: letters and the apostrophe inside a word.
static int word_len(Text t, int i)
{
    int n = 0;
    while (at(t, i + n) != ' ')
    {
        n++;
    }
    return n;
}

static int exception_lookup(Text t, int i, int len)
{
    for (int e = 0; e < EXCEPTION_COUNT; e++)
    {
        const char *w = exceptions[e].word;
        if ((int)strlen(w) != len)
        {
            continue;
        }
        int k = 0;
        while (k < len && at(t, i + k) == fold(w[k]))
        {
            k++;
        }
        if (k == len)
        {
            return e;
        }
    }
    return -1;
}

// Try the rules bucketed under the letter at i. Returns the number of
// characters consumed, or 0 if nothing matched.
static int apply_rules(Text t, int i, uint8_t *out, int max_out, int *n)
{
    char c = at(t, i);
    const char *const *bucket;
    if (c >= 'A' && c <= 'Z')
    {
        bucket = rule_buckets[c - 'A'];
    }
    else if (c == '\'')
    {
        bucket = rule_buckets[26];
    }
    else
    {
        return 0;
    }

    for (; *bucket; bucket++)
    {
        const char *rule = *bucket;
        const char *lb = strchr(rule, '[');
        const char *rb = strchr(lb, ']');
        const char *eq = strchr(rb, '=');
        int item_len = (int)(rb - lb - 1);

        int k = 0;
        while (k < item_len && at(t, i + k) == lb[1 + k])
        {
            k++;
        }
        if (k < item_len)
        {
            continue;
        }
        if (!match_left(t, i - 1, rule, lb) || !match_right(t, i + item_len, rb + 1, eq))
        {
            continue;
        }
        *n = emit(eq + 1, out, max_out, *n);
        return item_len;
    }
    return 0;
}

int phonemes_translate(const char *text, int start, uint8_t *out, int max_out, int *next)
{
    Text t = {text, (int)strlen(text)};
    int n = 0;
    int i = start;

    // Stop with PHONEMES_MIN_OUT slots still free, so no single rule or
    // exception is ever cut in half: the caller resumes at *next and gets
    // the rest.
    while (i < t.len && n + PHONEMES_MIN_OUT <= max_out)
    {
        if (at(t, i) == ' ')
        {
            char raw = text[i];
            if (raw >= '0' && raw <= '9')
            {
                n = emit(digit_names[raw - '0'], out, max_out, n);
            }
            else if (raw == '.' || raw == '!' || raw == '?')
            {
                out[n++] = SPEECH_PH_PAUSE;
            }
            // Anything else is skipped: a character the rules cannot place
            // is better silent than spelled out (§5.1).
            i++;
            continue;
        }

        if (at(t, i - 1) == ' ')
        {
            int len = word_len(t, i);
            int e = exception_lookup(t, i, len);
            if (e >= 0)
            {
                n = emit(exceptions[e].phonemes, out, max_out, n);
                i += len;
                continue;
            }
        }

        int used = apply_rules(t, i, out, max_out, &n);
        i += (used > 0) ? used : 1;
    }

    *next = i;
    return n;
}

int phonemes_rule_count(void)
{
    int n = 0;
    for (int b = 0; b < 27; b++)
    {
        for (const char *const *r = rule_buckets[b]; *r; r++)
        {
            n++;
        }
    }
    return n;
}
