#include "speech.h"
#include "Audio_PCM5101.h"
#include "SD_MMC.h"

#define SPEECH_QUEUE_SIZE 32

String speechQueue[SPEECH_QUEUE_SIZE];

int speechHead = 0;
int speechTail = 0;


/* ------------------------------------------------ */
/* INIT                                             */
/* ------------------------------------------------ */

void Speech_Init()
{
    // Audio_Init();
}


/* ------------------------------------------------ */
/* QUEUE AUDIO FILE                                 */
/* ------------------------------------------------ */

void Speech_Play(const char* file)
{
    int next = (speechTail + 1) % SPEECH_QUEUE_SIZE;

    if(next == speechHead)
    {
        Serial.println("Speech queue full");
        return;
    }

    speechQueue[speechTail] = file;
    speechTail = next;
}


/* ------------------------------------------------ */
/* PLAY WORD                                        */
/* ------------------------------------------------ */

void Speech_PlayWord(const char* word)
{
    String path = "/audio/";
    path += word;
    path += ".mp3";

    Serial.print("SPEECH QUEUE: ");
    Serial.println(path);

    if(!SD_MMC.exists(path.c_str()))
    {
        Serial.print("AUDIO FILE MISSING: ");
        Serial.println(path);
        return;
    }

    Serial.print("AUDIO FILE FOUND: ");
    Serial.println(path);

    Speech_Play(path.c_str());
}

/* ------------------------------------------------ */
/* UPDATE                                           */
/* ------------------------------------------------ */

void Speech_Update()
{
    if(speechHead == speechTail)
        return;

    if(!audio.isRunning())
    {
        String file = speechQueue[speechHead];

        audio.connecttoFS(SD_MMC, file.c_str());

        speechHead = (speechHead + 1) % SPEECH_QUEUE_SIZE;
    }
}