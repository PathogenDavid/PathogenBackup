/*
A utility for backing up and viewing GBA SRAM save files.
Super simple, pretty bare-minimum.

I made this because I need to back up the save file from a counterfeit cart I purchased by mistake.
The cart in question has a game that normally uses EEPROM backup, but was patched to use SRAM so traditional backup programs are failing me.
*/
#include <nds.h>
#include <fat.h>
#include <stdio.h>

#define CONSOLE_WIDTH 32
#define CONSOLE_HEIGHT 23

#define BYTES_PER_ROW (CONSOLE_WIDTH / 3)
#define BYTES_PER_SCREEN (BYTES_PER_ROW * CONSOLE_HEIGHT)
#define ROWS_PER_SCREEN (BYTES_PER_SCREEN / BYTES_PER_ROW)

#define GBA_SAVE_DATA_LENGTH (64 * 1024)

#define MAX_OFFSET ((GBA_SAVE_DATA_LENGTH - BYTES_PER_SCREEN) / BYTES_PER_ROW)

//! Console handles for the top and bottom screens
static PrintConsole topScreen;
static PrintConsole bottomScreen;

//! True when the save data contained anything that wasn't a 0 or 0xFF
static bool saveIsntEmpty;

//! True once the file system driver has been intiailzed.
static bool fatIsInitialized = false;

//! The number of times that the save data has been dumped to the file systen.
static int numFilesSaved = 0;

//! Sets the cursor to the given position on screen in the current console.
void setCursor(int x, int y)
{
    iprintf("\x1b[%d;%dH", y, x);
}

//! Prints an error to the screen and halts execution.
void __fatal(const char* file, int line, const char* message)
{
    // Strip the directory from the file name:
    const char* fileEnd = file + strlen(file);
    while (*fileEnd != '/' && *fileEnd != '\\' && fileEnd != file) { fileEnd--; }
    if (fileEnd != file) { fileEnd++; }
    file = fileEnd;

    // Print the error:
    //setCursor(0, 0);
    iprintf("\x1b[31;1mFATAL @ %s:%d\n%s\n", file, line, message);
    while (true) { swiWaitForVBlank(); }
}

#define fatal(message) __fatal(__FILE__, __LINE__, message)

//! Returns true if the specified file already exists
bool fileExists(const char* file)
{
    if (!fatIsInitialized)
    { fatal("Called fileExists without initializing filesystem."); }

    FILE* f = fopen(file, "r");
    if (f != NULL)
    {
        fclose(f);
        return true;
    }
    return false;
}

//! Draws the screens
//! The top screen will contain a hex view of the save file
//! The bottom one will contain an ASCII view as well as some extra info.
void drawScreens(uint offset)
{
    consoleSelect(&topScreen);
    consoleClear();
    consoleSelect(&bottomScreen);
    consoleClear();

    // Print hex/ASCII view of save data
    for (int i = 0; i < BYTES_PER_SCREEN; )
    {
        for (int j = 0; j < BYTES_PER_ROW; j++, i++)
        {
            u8 b = SRAM[offset + i];

            // Draw byte on hex view
            consoleSelect(&topScreen);
            if (j != 0) { iprintf(" "); }
            iprintf("%02X", b);

            // Draw byte on ASCII view
            consoleSelect(&bottomScreen);
            iprintf("%c", (b < ' ' || b > '~') ? '.' : b);
        }
        consoleSelect(&topScreen);
        iprintf("\n");
        consoleSelect(&bottomScreen);
        iprintf("\n");
    }

    // Print extra info:
    consoleSelect(&bottomScreen);
    {
        int line = 0;
        #define iprintfl setCursor(BYTES_PER_ROW + 1, line++); iprintf
        iprintfl("Game: %s", GBA_HEADER.title);
        iprintfl(saveIsntEmpty ? "Found non-00/FF! :D" : "All 00 or FF :(");

        // Dump the EXMEMCNT register onto the screen:
        // (See http://problemkaputt.de/gbatek.htm#dsmemorycontrolcartridgesandmainram for details.)
        iprintfl("EXMEMCNT=0x%X", REG_EXMEMCNT);
        line++;
        iprintfl("SRAM WAIT: %d%d", (REG_EXMEMCNT & BIT(0)) != 0, (REG_EXMEMCNT & BIT(1)) != 0);
        iprintfl("ROM1 WAIT: %d%d", (REG_EXMEMCNT & BIT(2)) != 0, (REG_EXMEMCNT & BIT(3)) != 0);
        iprintfl("ROM2 WAIT: %d cycles", (REG_EXMEMCNT & BIT(4)) ? 4 : 6);
        iprintfl("PHI PIN O: %d%d", (REG_EXMEMCNT & BIT(5)) != 0, (REG_EXMEMCNT & BIT(6)) != 0);
        iprintfl("SLOT2PERM: %s", (REG_EXMEMCNT & BIT(7)) ? "ARM7" : "ARM9");
        iprintfl("SLOT1PERM: %s", (REG_EXMEMCNT & BIT(11)) ? "ARM7" : "ARM9");
        iprintfl("MAIN MODE: %s", (REG_EXMEMCNT & BIT(14)) ? "SYNC" : "ASYNC");
        iprintfl("APRIORITY: %s", (REG_EXMEMCNT & BIT(15)) ? "ARM7" : "ARM9");

        // Display the offset in the save memory:
        line++;
        iprintfl("OFFSET: 0x%X", offset);
        iprintfl("      : %d%%", (int)(100.f * (float)offset / (float)(GBA_SAVE_DATA_LENGTH - BYTES_PER_SCREEN)));
        if (numFilesSaved > 0)
        {
            line++;
            if (numFilesSaved > 1)
            { iprintfl("%d SAVES DUMPED.", numFilesSaved); }
            else
            { iprintfl("SAVE DUMPED."); }
        }
        #undef iprintfl
    }
}

//! Dumps the entire save region into a file named with the title in the game's header.
//! NOTE: This function makes no attempt to detect the actualy size of the save file.
//! This isn't probably an issue, but know that the save will be larger than what you might expect.
void dumpSaveDataToFile()
{
    consoleSelect(&topScreen);
    setCursor(0, 0);
    iprintf("Dumping save data...\n");

    // Initialize the file system driver:
    if (!fatIsInitialized)
    {
        iprintf("Initializing file system...\n");
        if (!fatInitDefault())
        { fatal("Failed to initialize FAT driver!"); }
        fatIsInitialized = true;
    }

    // Open the file:
    iprintf("Opening file for writing...\n");
    char fileName[1024];
    sprintf(fileName, "%s.sav", GBA_HEADER.title);
    FILE* f = NULL;
    int fileNo = 0;
    while (fileNo < 10)
    {
        if (!fileExists(fileName))
        {
            f = fopen(fileName, "wb");
            break;
        }
        fileNo++;
        sprintf(fileName, "%s_%d.sav", GBA_HEADER.title, fileNo);
    }

    if (f == NULL)
    { fatal("Could not open file for writing!"); }

    // Save the file:
    iprintf("Saving...");

    const uint chunkSize = 32;
    if ((GBA_SAVE_DATA_LENGTH % chunkSize) != 0)
    { fatal("BAD CHUNK SIZE"); }

    u8 buffer[chunkSize];
    for (uint i = 0; i < GBA_SAVE_DATA_LENGTH; i += chunkSize)
    {
        iprintf("Saving 0x%X/0x%X...\n", i, GBA_SAVE_DATA_LENGTH);
        
        // Copy a chunk into RAM:
        // (fwrite is not used because the SRAM bus is only 8 bits wide, and fwrite probably uses larger sized load instructions.)
        for (uint j = 0; j < chunkSize; j++)
        {
            buffer[j] = SRAM[i + j];
        }

        // Write the chunk to filesystem:
        if (fwrite(buffer, sizeof(u8), chunkSize, f) != chunkSize)
        { fatal("Error while writing!"); }
    }

    if (fclose(f) != 0)
    { fatal("Error closing file."); }

    numFilesSaved++;
}

//! Program entrypoint
int main(void)
{
    // Initialize consoles
    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);

    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);

    consoleInit(&topScreen, 3,BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
    consoleInit(&bottomScreen, 3,BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

    // Enable accessing slot 2:
    sysSetCartOwner(true);

    // Check the entire save region for non-zero
    saveIsntEmpty = false;
    for (int i = 0; i < GBA_SAVE_DATA_LENGTH; i++)
    {
        u8 b = SRAM[i];
        if (b != 0x00 && b != 0xFF)
        {
            saveIsntEmpty = true;
            break;
        }
    }

    // Loop forever
    uint offset = 0;
    while(true)
    {
        drawScreens(offset * BYTES_PER_ROW);

        while (true)
        {
            swiWaitForVBlank();

            // Handle input
            scanKeys();
            uint32 input = keysDownRepeat();

            // Slow scrolling
            if ((input & KEY_UP) && offset > 0)
            {
                offset--;
                break;
            }
            else if ((input & KEY_DOWN) && offset < MAX_OFFSET)
            {
                offset++;
                break;
            }

            // Fast scrolling
            if ((input & KEY_X) && offset > 0)
            {
                uint oldOffset = offset;
                offset -= ROWS_PER_SCREEN;

                // Handle underflow:
                if (offset > oldOffset)
                { offset = 0; }
                break;
            }
            else if ((input & KEY_B) && offset < MAX_OFFSET)
            {
                offset += ROWS_PER_SCREEN;
                if (offset > MAX_OFFSET)
                { offset = MAX_OFFSET; }
                break;
            }

            // Save file dumping:
            input = keysDown();
            if (input & KEY_START)
            {
                dumpSaveDataToFile();
                break;
            }
        }
    }

    return 0;
}
