#include "voice.h"
#include "sensors.h"
#include "speech.h"
#include "number_speech.h"

enum MessageType
{
    MSG_NONE,
    MSG_HELP,
    MSG_ENEMY,
    MSG_FALLBACK,
    MSG_AMBUSH
};

MessageType lastMessage = MSG_NONE;


/* ------------------------------------------------ */
/* INIT                                             */
/* ------------------------------------------------ */

void Voice_Init()
{
    Serial.println("VOICE SYSTEM READY");
    Speech_Init();
}


/* ------------------------------------------------ */
/* DEBUG SPEAK                                      */
/* ------------------------------------------------ */

void Voice_Speak(String text)
{
    Serial.print("SPEAKER: ");
    Serial.println(text);
}


/* ------------------------------------------------ */
/* SPEAK STATUS                                     */
/* ------------------------------------------------ */

void Voice_SpeakStatus()
{
    int watchID = 1;

    String health = Sensors_GetHealthState();
    int heading = (int)Sensors_GetYaw();
    int distance = 12;   // placeholder until RSSI/GPS added
    int floor = Sensors_GetFloor();

    Speech_PlayWord("watch");
    SpeakNumber(watchID);

    if (health == "HEALTHY")
        Speech_PlayWord("healthy");

    if (health == "INJURED")
        Speech_PlayWord("injured");

    if (health == "DEAD")
        Speech_PlayWord("dead");

    Speech_PlayWord("heading");

    SpeakNumber(heading);

    Speech_PlayWord("degrees");

    if (heading < 45 || heading >= 315)
        Speech_PlayWord("north");
    else if (heading < 135)
        Speech_PlayWord("east");
    else if (heading < 225)
        Speech_PlayWord("south");
    else
        Speech_PlayWord("west");

    SpeakNumber(distance);

    Speech_PlayWord("meters");

    Speech_PlayWord("floor");

    SpeakNumber(floor);
}


/* ------------------------------------------------ */
/* READ LAST MESSAGE                                */
/* ------------------------------------------------ */

void Voice_ReadLastMessage()
{
    Speech_PlayWord("last");
    Speech_PlayWord("message");

    switch(lastMessage)
    {
        case MSG_HELP:
            Speech_PlayWord("help");
        break;

        case MSG_ENEMY:
            Speech_PlayWord("enemy");
        break;

        case MSG_FALLBACK:
            Speech_PlayWord("fallback");
        break;

        case MSG_AMBUSH:
            Speech_PlayWord("ambush");
        break;

        default:
            Speech_PlayWord("none");
        break;
    }
}


/* ------------------------------------------------ */
/* COMMAND HANDLER                                  */
/* ------------------------------------------------ */

void handleCommand(String cmd)
{
    cmd.toUpperCase();

    if(cmd == "ZORO STATUS")
    {
        Voice_SpeakStatus();
    }

    else if(cmd == "ZORO SEND HELP")
    {
        lastMessage = MSG_HELP;

        Speech_PlayWord("help");
        Speech_PlayWord("sent");

        Voice_Speak("Help message sent");
    }

    else if(cmd == "ZORO SEND ENEMY")
    {
        lastMessage = MSG_ENEMY;

        Speech_PlayWord("enemy");
        Speech_PlayWord("sent");

        Voice_Speak("Enemy alert sent");
    }

    else if(cmd == "ZORO SEND FALLBACK")
    {
        lastMessage = MSG_FALLBACK;

        Speech_PlayWord("fallback");
        Speech_PlayWord("sent");

        Voice_Speak("Fallback command sent");
    }

    else if(cmd == "ZORO SEND AMBUSH")
    {
        lastMessage = MSG_AMBUSH;

        Speech_PlayWord("ambush");
        Speech_PlayWord("sent");

        Voice_Speak("Ambush signal sent");
    }

    else if(cmd == "ZORO READ")
    {
        Voice_ReadLastMessage();
    }
}


/* ------------------------------------------------ */
/* MICROPHONE COMMAND INTERFACE                     */
/* ------------------------------------------------ */

void Voice_HandleCommand(const char* cmd)
{
    handleCommand(String(cmd));
}


/* ------------------------------------------------ */
/* UPDATE (SERIAL DEBUG)                            */
/* ------------------------------------------------ */

void Voice_Update()
{
    if(Serial.available())
    {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        handleCommand(cmd);
    }
}