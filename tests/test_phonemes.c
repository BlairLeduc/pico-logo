//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the letter-to-sound rules (core/phonemes.c), the front end of
//  `say`. docs/say-design.md §10.
//
//  The load-bearing one is the accuracy table: NRL Report 7948 claims ~90 %
//  of the words in average text, so the table is a claim about *our
//  transcription of the rules* rather than about English -- a failure here
//  means a rule was typed wrong. §10 asks for 200 words; this is 241 of
//  them, drawn from the commonest words in written English, which is what
//  "average text" means.
//
//  ctest also writes speech_frontend_cost.txt into the build directory it
//  runs from: §9.3's estimate of the translation cost is the least-supported
//  number in the design and M2 is where it gets measured.
//

#include "unity.h"
#include "core/phonemes.h"
#include "core/speech_synth.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

void setUp(void) {}
void tearDown(void) {}

// Translate `text` into a space-separated phoneme string, looping the way a
// caller with a small buffer does so that every test also exercises the
// resumption seam.
static void transcribe(const char *text, char *out, size_t out_size)
{
    uint8_t buf[16];
    int pos = 0;
    out[0] = '\0';

    while (text[pos])
    {
        int next = pos;
        int n = phonemes_translate(text, pos, buf, (int)sizeof buf, &next);
        if (next <= pos)
        {
            break;
        }
        for (int i = 0; i < n; i++)
        {
            if (out[0] != '\0')
            {
                strncat(out, " ", out_size - strlen(out) - 1);
            }
            strncat(out, speech_phoneme_names[buf[i]], out_size - strlen(out) - 1);
        }
        pos = next;
    }
}

static void assert_says(const char *text, const char *expected)
{
    char got[256];
    transcribe(text, got, sizeof got);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, got, text);
}

//==========================================================================
// The accuracy table (§10)
//==========================================================================

typedef struct WordCase
{
    const char *word;
    const char *phonemes;
} WordCase;

// General American, transcribed independently of what the rules do -- the
// point is to catch a mistyped rule, so reading the answers off the engine
// would make the test say nothing at all.
static const WordCase accuracy_table[] = {
    {"the", "dh ax"},
    {"of", "ah v"},
    {"and", "ae n d"},
    {"a", "ax"},
    {"to", "t uw"},
    {"in", "ih n"},
    {"is", "ih z"},
    {"you", "y uw"},
    {"that", "dh ae t"},
    {"it", "ih t"},
    {"he", "hh iy"},
    {"was", "w ah z"},
    {"for", "f ao r"},
    {"on", "aa n"},
    {"are", "aa r"},
    {"as", "ae z"},
    {"with", "w ih th"},
    {"his", "hh ih z"},
    {"they", "dh ey"},
    {"at", "ae t"},
    {"be", "b iy"},
    {"this", "dh ih s"},
    {"have", "hh ae v"},
    {"from", "f r ah m"},
    {"or", "ao r"},
    {"one", "w ah n"},
    {"had", "hh ae d"},
    {"by", "b ay"},
    {"word", "w er d"},
    {"but", "b ah t"},
    {"not", "n aa t"},
    {"what", "w ah t"},
    {"all", "ao l"},
    {"were", "w er"},
    {"we", "w iy"},
    {"when", "w eh n"},
    {"your", "y ao r"},
    {"can", "k ae n"},
    {"said", "s eh d"},
    {"there", "dh eh r"},
    {"use", "y uw z"},
    {"an", "ae n"},
    {"each", "iy ch"},
    {"which", "w ih ch"},
    {"she", "sh iy"},
    {"do", "d uw"},
    {"how", "hh aw"},
    {"their", "dh eh r"},
    {"if", "ih f"},
    {"will", "w ih l"},
    {"up", "ah p"},
    {"other", "ah dh er"},
    {"about", "ax b aw t"},
    {"out", "aw t"},
    {"many", "m eh n iy"},
    {"then", "dh eh n"},
    {"them", "dh eh m"},
    {"these", "dh iy z"},
    {"so", "s ow"},
    {"some", "s ah m"},
    {"her", "hh er"},
    {"would", "w uh d"},
    {"make", "m ey k"},
    {"like", "l ay k"},
    {"him", "hh ih m"},
    {"into", "ih n t uw"},
    {"time", "t ay m"},
    {"has", "hh ae z"},
    {"look", "l uh k"},
    {"two", "t uw"},
    {"more", "m ao r"},
    {"write", "r ay t"},
    {"go", "g ow"},
    {"see", "s iy"},
    {"number", "n ah m b er"},
    {"no", "n ow"},
    {"way", "w ey"},
    {"could", "k uh d"},
    {"people", "p iy p ax l"},
    {"my", "m ay"},
    {"than", "dh ae n"},
    {"first", "f er s t"},
    {"water", "w ao t er"},
    {"been", "b ih n"},
    {"call", "k ao l"},
    {"who", "hh uw"},
    {"oil", "oy l"},
    {"its", "ih t s"},
    {"now", "n aw"},
    {"find", "f ay n d"},
    {"long", "l ao ng"},
    {"down", "d aw n"},
    {"day", "d ey"},
    {"did", "d ih d"},
    {"get", "g eh t"},
    {"come", "k ah m"},
    {"made", "m ey d"},
    {"may", "m ey"},
    {"part", "p aa r t"},
    {"over", "ow v er"},
    {"new", "n uw"},
    {"sound", "s aw n d"},
    {"take", "t ey k"},
    {"only", "ow n l iy"},
    {"little", "l ih t ax l"},
    {"work", "w er k"},
    {"know", "n ow"},
    {"place", "p l ey s"},
    {"year", "y ih r"},
    {"live", "l ih v"},
    {"me", "m iy"},
    {"back", "b ae k"},
    {"give", "g ih v"},
    {"most", "m ow s t"},
    {"very", "v eh r iy"},
    {"after", "ae f t er"},
    {"thing", "th ih ng"},
    {"just", "jh ah s t"},
    {"name", "n ey m"},
    {"good", "g uh d"},
    {"man", "m ae n"},
    {"think", "th ih ng k"},
    {"say", "s ey"},
    {"great", "g r ey t"},
    {"where", "w eh r"},
    {"help", "hh eh l p"},
    {"through", "th r uw"},
    {"much", "m ah ch"},
    {"before", "b ih f ao r"},
    {"line", "l ay n"},
    {"right", "r ay t"},
    {"too", "t uw"},
    {"mean", "m iy n"},
    {"old", "ow l d"},
    {"any", "eh n iy"},
    {"same", "s ey m"},
    {"tell", "t eh l"},
    {"boy", "b oy"},
    {"follow", "f aa l ow"},
    {"came", "k ey m"},
    {"want", "w aa n t"},
    {"show", "sh ow"},
    {"also", "ao l s ow"},
    {"around", "ax r aw n d"},
    {"form", "f ao r m"},
    {"three", "th r iy"},
    {"small", "s m ao l"},
    {"set", "s eh t"},
    {"put", "p uh t"},
    {"end", "eh n d"},
    {"does", "d ah z"},
    {"well", "w eh l"},
    {"large", "l aa r jh"},
    {"must", "m ah s t"},
    {"big", "b ih g"},
    {"even", "iy v ax n"},
    {"such", "s ah ch"},
    {"turn", "t er n"},
    {"here", "hh iy r"},
    {"why", "w ay"},
    {"ask", "ae s k"},
    {"went", "w eh n t"},
    {"men", "m eh n"},
    {"read", "r iy d"},
    {"need", "n iy d"},
    {"land", "l ae n d"},
    {"home", "hh ow m"},
    {"us", "ah s"},
    {"move", "m uw v"},
    {"try", "t r ay"},
    {"kind", "k ay n d"},
    {"hand", "hh ae n d"},
    {"again", "ax g eh n"},
    {"change", "ch ey n jh"},
    {"off", "ao f"},
    {"play", "p l ey"},
    {"air", "eh r"},
    {"away", "ax w ey"},
    {"house", "hh aw s"},
    {"point", "p oy n t"},
    {"page", "p ey jh"},
    {"letter", "l eh t er"},
    {"mother", "m ah dh er"},
    {"found", "f aw n d"},
    {"study", "s t ah d iy"},
    {"still", "s t ih l"},
    {"learn", "l er n"},
    {"should", "sh uh d"},
    {"world", "w er l d"},
    {"last", "l ae s t"},
    {"school", "s k uw l"},
    {"father", "f aa dh er"},
    {"keep", "k iy p"},
    {"tree", "t r iy"},
    {"never", "n eh v er"},
    {"start", "s t aa r t"},
    {"city", "s ih t iy"},
    {"earth", "er th"},
    {"light", "l ay t"},
    {"thought", "th ao t"},
    {"head", "hh eh d"},
    {"under", "ah n d er"},
    {"story", "s t ao r iy"},
    {"saw", "s ao"},
    {"left", "l eh f t"},
    {"few", "f y uw"},
    {"while", "w ay l"},
    {"might", "m ay t"},
    {"close", "k l ow z"},
    {"seem", "s iy m"},
    {"next", "n eh k s t"},
    {"hard", "hh aa r d"},
    {"open", "ow p ax n"},
    {"begin", "b ih g ih n"},
    {"life", "l ay f"},
    {"those", "dh ow z"},
    {"both", "b ow th"},
    {"paper", "p ey p er"},
    {"got", "g aa t"},
    {"group", "g r uw p"},
    {"run", "r ah n"},
    {"side", "s ay d"},
    {"feet", "f iy t"},
    {"car", "k aa r"},
    {"night", "n ay t"},
    {"white", "w ay t"},
    {"sea", "s iy"},
    {"grow", "g r ow"},
    {"took", "t uh k"},
    {"river", "r ih v er"},
    {"four", "f ao r"},
    {"state", "s t ey t"},
    {"once", "w ah n s"},
    {"book", "b uh k"},
    {"stop", "s t aa p"},
    {"second", "s eh k ax n d"},
    {"miss", "m ih s"},
    {"idea", "ay d iy ax"},
    {"eat", "iy t"},
    {"face", "f ey s"},
    {"far", "f aa r"},
};

#define ACCURACY_COUNT ((int)(sizeof accuracy_table / sizeof accuracy_table[0]))

void test_the_word_table_is_at_least_ninety_percent_right(void)
{
    int correct = 0;
    for (int i = 0; i < ACCURACY_COUNT; i++)
    {
        char got[256];
        transcribe(accuracy_table[i].word, got, sizeof got);
        if (strcmp(got, accuracy_table[i].phonemes) == 0)
        {
            correct++;
        }
        else
        {
            printf("  miss: %-10s got [%s] want [%s]\n", accuracy_table[i].word, got,
                   accuracy_table[i].phonemes);
        }
    }
    printf("  accuracy: %d/%d = %.1f%%\n", correct, ACCURACY_COUNT,
           100.0 * correct / ACCURACY_COUNT);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(ACCURACY_COUNT * 9 / 10, correct);
}

//==========================================================================
// The context forms, one test each (§7)
//==========================================================================

// `#` -- one or more vowels. "[AR]#" is the report's own worked example:
// "care" is not "car".
void test_context_vowel_run(void)
{
    assert_says("care", "k eh r");
    assert_says("car", "k aa r");
}

// `^` -- one consonant, and `%` -- a suffix (E, ER, ES, ED, ELY, ING).
// "[A]^%" together are what make a silent E lengthen the vowel before it.
void test_context_consonant_and_suffix(void)
{
    assert_says("gate", "g ey t");
    assert_says("gat", "g ae t");
}

// `:` -- zero or more consonants, so " :[ANY]" reaches back past the M to
// the space and "many" does not rhyme with "rainy".
void test_context_optional_consonants(void)
{
    assert_says("many", "m eh n iy");
}

// `+` -- a front vowel, which is the whole of the soft-C and soft-G rule.
void test_context_front_vowel(void)
{
    assert_says("city", "s ih t iy");
    assert_says("cat", "k ae t");
    assert_says("gem", "jh eh m");
}

// `.` -- a voiced consonant, which is what makes a plural buzz.
void test_context_voiced_consonant(void)
{
    assert_says("dogs", "d aa g z");
    assert_says("cats", "k ae t s");
}

// `&` -- a sibilant, which is what puts a vowel in the plural of one.
void test_context_sibilant(void)
{
    assert_says("wishes", "w ih sh ih z");
}

// `@` -- a consonant that colours a following long U, so "new" is not
// "nyew" while "few" still is.
void test_context_long_u_consonant(void)
{
    assert_says("new", "n uw");
    assert_says("few", "f y uw");
}

// A word boundary inside a right context, which is a rule reading the
// *next* word: "the" before a vowel is not "the" before a consonant. This
// is why a list is joined back into a sentence before the rules see it.
void test_context_word_boundary_reaches_across_the_space(void)
{
    assert_says("the apple", "dh iy ae p ax l");
    assert_says("the book", "dh ax b uh k");
}

//==========================================================================
// The exception list (§7)
//==========================================================================

void test_exceptions_beat_the_rules(void)
{
    assert_says("one", "w ah n");
    assert_says("of", "ah v");
    assert_says("two", "t uw");
    assert_says("people", "p iy p ax l");
}

// Whole-word, before the rules run -- "off" is not "of" with a spare F.
void test_exceptions_are_whole_word(void)
{
    assert_says("off", "ao f");
}

//==========================================================================
// Numbers, punctuation and the things the rules never see (§5.1)
//==========================================================================

void test_digits_are_spoken_one_at_a_time(void)
{
    assert_says("42", "f ao r t uw");
}

void test_sentence_punctuation_is_a_pause(void)
{
    assert_says("hi.", "hh ay _");
    assert_says("hi!", "hh ay _");
    assert_says("hi?", "hh ay _");
}

// Everything else is skipped: a character the rules cannot place is better
// silent than spelled out.
void test_other_punctuation_is_skipped(void)
{
    assert_says("hi, there", "hh ay dh eh r");
    assert_says("@#$", "");
}

void test_empty_input_says_nothing(void)
{
    assert_says("", "");
}

// An apostrophe is a letter to the rules and a clitic to the scan.
void test_contractions(void)
{
    assert_says("don't", "d ow n t");
    assert_says("isn't", "ih z ax n t");
}

//==========================================================================
// The streaming contract
//==========================================================================

// A caller whose buffer is too small for the sentence gets exactly the same
// phonemes, because the rules are handed the whole string every time and so
// keep their context across the seam.
void test_a_small_buffer_gives_the_same_answer(void)
{
    // A spelled-out letter ("w" is seven phonemes, the longest any rule
    // emits) and a digit, so the seam is tested where it is tightest.
    const char *text = "the w 4 humanoid must not escape";

    uint8_t whole[128];
    int next = 0;
    int n_whole = phonemes_translate(text, 0, whole, (int)sizeof whole, &next);
    TEST_ASSERT_EQUAL_INT((int)strlen(text), next);

    uint8_t piece[PHONEMES_MIN_OUT];
    int n_pieces = 0;
    int pos = 0;
    while (text[pos])
    {
        int n = phonemes_translate(text, pos, piece, (int)sizeof piece, &next);
        TEST_ASSERT_GREATER_THAN_INT(pos, next); // always makes progress
        for (int i = 0; i < n; i++)
        {
            TEST_ASSERT_EQUAL_UINT8(whole[n_pieces + i], piece[i]);
        }
        n_pieces += n;
        pos = next;
    }
    TEST_ASSERT_EQUAL_INT(n_whole, n_pieces);
}

//==========================================================================
// The rules themselves
//==========================================================================

// Every bucket ends in a fallback, so no letter is ever silently dropped --
// which also catches a mistyped phoneme name on a fallback's right-hand
// side, since an unknown name is simply not emitted.
void test_every_letter_says_something(void)
{
    char got[64];
    for (char c = 'a'; c <= 'z'; c++)
    {
        char word[2] = {c, '\0'};
        transcribe(word, got, sizeof got);
        TEST_ASSERT_TRUE_MESSAGE(got[0] != '\0', word);
    }
}

//==========================================================================
// §9.3's cost, measured rather than estimated
//==========================================================================

void test_translation_cost(void)
{
    const char *sentence = "the humanoid must not escape";
    const int letters = (int)strlen(sentence);
    const int reps = 20000;

    uint8_t buf[128];
    int next = 0;
    clock_t t0 = clock();
    for (int i = 0; i < reps; i++)
    {
        phonemes_translate(sentence, 0, buf, (int)sizeof buf, &next);
    }
    double us = 1e6 * (double)(clock() - t0) / CLOCKS_PER_SEC / reps;

    FILE *f = fopen("speech_frontend_cost.txt", "w");
    if (f)
    {
        fprintf(f, "P16 M2 -- letter-to-sound cost (docs/say-design.md 9.3)\n");
        fprintf(f, "host, one sentence of %d characters, %d repetitions\n", letters, reps);
        fprintf(f, "sentence: \"%s\"\n", sentence);
        fprintf(f, "rules:        %d\n", phonemes_rule_count());
        fprintf(f, "per sentence: %.2f us\n", us);
        fprintf(f, "per letter:   %.3f us\n", us / letters);
        fclose(f);
    }
    printf("  front end: %.2f us a sentence, %.3f us a letter (host)\n", us, us / letters);

    // Not a performance assertion -- the host is not the board -- only a
    // guard that the scan has not gone quadratic in the sentence.
    TEST_ASSERT_LESS_THAN_INT(1000, (int)us);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_word_table_is_at_least_ninety_percent_right);
    RUN_TEST(test_context_vowel_run);
    RUN_TEST(test_context_consonant_and_suffix);
    RUN_TEST(test_context_optional_consonants);
    RUN_TEST(test_context_front_vowel);
    RUN_TEST(test_context_voiced_consonant);
    RUN_TEST(test_context_sibilant);
    RUN_TEST(test_context_long_u_consonant);
    RUN_TEST(test_context_word_boundary_reaches_across_the_space);
    RUN_TEST(test_exceptions_beat_the_rules);
    RUN_TEST(test_exceptions_are_whole_word);
    RUN_TEST(test_digits_are_spoken_one_at_a_time);
    RUN_TEST(test_sentence_punctuation_is_a_pause);
    RUN_TEST(test_other_punctuation_is_skipped);
    RUN_TEST(test_empty_input_says_nothing);
    RUN_TEST(test_contractions);
    RUN_TEST(test_a_small_buffer_gives_the_same_answer);
    RUN_TEST(test_every_letter_says_something);
    RUN_TEST(test_translation_cost);
    return UNITY_END();
}
