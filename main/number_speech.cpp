#include "number_speech.h"
#include "speech.h"

void SpeakNumber(int n)
{
    if (n >= 100)
    {
        int hundreds = n / 100;

        Speech_PlayWord(String(hundreds).c_str());
        Speech_PlayWord("hundred");

        n = n % 100;
    }

    if (n >= 20)
    {
        int tens = (n / 10) * 10;

        Speech_PlayWord(String(tens).c_str());

        n = n % 10;
    }

    if (n > 0)
    {
        Speech_PlayWord(String(n).c_str());
    }
}