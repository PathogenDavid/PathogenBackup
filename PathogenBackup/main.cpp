/*
A utility for backing up GBA save files.
Super simple, pretty bare-minimum.

I made this because I need to back up the save file from a pirate cart I purchased by mistake.
The cart in question has a game that normally uses EEPROM backup, but was patched to use SRAM so traditional backup programs are failing me.
*/
#include <nds.h>
#include <stdio.h>

#define CONSOLE_WIDTH 32
#define CONSOLE_HEIGHT 23

#define BYTES_PER_ROW (CONSOLE_WIDTH / 3)
#define BYTES_PER_SCREEN (BYTES_PER_ROW * CONSOLE_HEIGHT)

#define GBA_SAVE_DATA_LENGTH (64 * 1024)

void setCursor(int x, int y)
{
    iprintf("\x1b[%d;%dH", y, x);
}

int main(void)
{
    // Initialize consoles
    PrintConsole topScreen;
    PrintConsole bottomScreen;
    
    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);

    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);

    consoleInit(&topScreen, 3,BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
    consoleInit(&bottomScreen, 3,BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

    // Enable accessing slot 2:
    sysSetCartOwner(true);

    // Print hex/ASCII view of save data
    for (int i = 0; i < BYTES_PER_SCREEN; )
    {
        for (int j = 0; j < BYTES_PER_ROW; j++, i++)
        {
            u8 c = SRAM[i];

            // Hex view
            consoleSelect(&topScreen);
            if (j != 0) { iprintf(" "); }
            iprintf("%02X", c);

            // ASCII view
            consoleSelect(&bottomScreen);
            iprintf("%c", (c < ' ' || c > '~') ? '.' : c);
        }
        consoleSelect(&topScreen);
        iprintf("\n");
        consoleSelect(&bottomScreen);
        iprintf("\n");
    }

    // Check the entire save region for non-zero
    bool foundNonZero = false;
    for (int i = 0; i < GBA_SAVE_DATA_LENGTH; i++)
    {
        if (SRAM[i] != 0x00 && SRAM[i] != 0xFF)
        {
            foundNonZero = true;
            break;
        }
    }

    // Print extra info:
    consoleSelect(&bottomScreen);
    {
        int line = 0;
        #define iprintfl setCursor(BYTES_PER_ROW + 1, line++); iprintf
        iprintfl("Game: %s", GBA_HEADER.title);
        iprintfl(foundNonZero ? "Found non-00/FF! :D" : "All 00 or FF :(");
        iprintfl("EXMEMCNT=0x%X", REG_EXMEMCNT);
        line++;
        iprintfl("SRAM WAIT: %d%d", REG_EXMEMCNT & BIT(0), REG_EXMEMCNT & BIT(1));
        iprintfl("ROM1 WAIT: %d%d", REG_EXMEMCNT & BIT(2), REG_EXMEMCNT & BIT(3));
        iprintfl("ROM2 WAIT: %d cycles", (REG_EXMEMCNT & BIT(4)) ? 4 : 6);
        iprintfl("PHI PIN O: %d%d", REG_EXMEMCNT & BIT(5), REG_EXMEMCNT & BIT(6));
        iprintfl("SLOT2PERM: %s", (REG_EXMEMCNT & BIT(7)) ? "ARM7" : "ARM9");
        iprintfl("SLOT1PERM: %s", (REG_EXMEMCNT & BIT(11)) ? "ARM7" : "ARM9");
        iprintfl("MAIN MODE: %s", (REG_EXMEMCNT & BIT(14)) ? "SYNC" : "ASYNC");
        iprintfl("APRIORITY: %s", (REG_EXMEMCNT & BIT(15)) ? "ARM7" : "ARM9");
        #undef iprintfl
    }

    // Loop forever
    while(true)
    {
        swiWaitForVBlank();
    }

    return 0;
}
