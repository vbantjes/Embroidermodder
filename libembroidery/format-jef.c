#include "emb-time.h"
#include "format-jef.h"
#include "helpers-binary.h"
#include "helpers-misc.h"

#define HOOP_126X110 0
#define	HOOP_110X110 1
#define	HOOP_50X50 2
#define	HOOP_140X200 3
#define HOOP_230X200 4

static int jefGetHoopSize(int width, int height)
{
    if(width <  50 && height <  50) { return HOOP_50X50; }
    if(width < 110 && height < 110) { return HOOP_110X110; }
    if(width < 140 && height < 200) { return HOOP_140X200; }
    return ((int) HOOP_110X110);
}

static void jefSetHoopFromId(EmbPattern *pattern, int hoopCode)
{
    switch(hoopCode)
    {
        case HOOP_126X110:
            pattern->hoop.height = 126.0;
            pattern->hoop.width = 110.0;
            break;
        case HOOP_50X50:
            pattern->hoop.height = 50.0;
            pattern->hoop.width = 50.0;
            break;
        case HOOP_110X110:
           pattern->hoop.height = 110.0;
            pattern->hoop.width = 110.0;
            break;
        case HOOP_140X200:
            pattern->hoop.height = 140.0;
            pattern->hoop.width = 200.0;
            break;
        case HOOP_230X200:
            pattern->hoop.height = 230.0;
            pattern->hoop.width = 200.0;
            break;
    }
}

struct hoop_padding {
    int left;
    int right;
    int top;
    int bottom;
};

int readJef(EmbPattern* pattern, const char* fileName)
{
    int stitchOffset, formatFlags, numberOfColors, numberOfStitchs;
    int hoopSize, i;
    struct hoop_padding bounds, rectFrom110x110, rectFrom50x50, rectFrom200x140, rect_from_custom;
    int stitchCount;
    char date[8], time[8];
    char dx, dy;

    FILE* file = 0;

    file = fopen(fileName, "rb");
    if(!file)
        return 0;

    stitchOffset = binaryReadInt32(file);
    formatFlags = binaryReadInt32(file); /* TODO: find out what this means */

    binaryReadBytes(file, (unsigned char*) date, 8);
    binaryReadBytes(file, (unsigned char*) time, 8);
    numberOfColors = binaryReadInt32(file);
    numberOfStitchs = binaryReadInt32(file);
    hoopSize = binaryReadInt32(file);
    jefSetHoopFromId(pattern, hoopSize);

    bounds.left = binaryReadInt32(file);
    bounds.top = binaryReadInt32(file);
    bounds.right = binaryReadInt32(file);
    bounds.bottom = binaryReadInt32(file);

    rectFrom110x110.left = binaryReadInt32(file);
    rectFrom110x110.top = binaryReadInt32(file);
    rectFrom110x110.right = binaryReadInt32(file);
    rectFrom110x110.bottom = binaryReadInt32(file);

    rectFrom50x50.left = binaryReadInt32(file);
    rectFrom50x50.top = binaryReadInt32(file);
    rectFrom50x50.right = binaryReadInt32(file);
    rectFrom50x50.bottom = binaryReadInt32(file);

    rectFrom200x140.left = binaryReadInt32(file);
    rectFrom200x140.top = binaryReadInt32(file);
    rectFrom200x140.right = binaryReadInt32(file);
    rectFrom200x140.bottom = binaryReadInt32(file);

    rect_from_custom.left = binaryReadInt32(file);
    rect_from_custom.top = binaryReadInt32(file);
    rect_from_custom.right = binaryReadInt32(file);
    rect_from_custom.bottom = binaryReadInt32(file);

    for(i = 0; i < numberOfColors; i++)
    {
        embPattern_addThread(pattern, jefThreads[binaryReadInt32(file) % 79]);
    }
    fseek(file, stitchOffset, SEEK_SET);
    stitchCount = 0;
    while(stitchCount < numberOfStitchs + 100)
    {
        int flags = NORMAL;
        unsigned char b0 = binaryReadByte(file);
        unsigned char b1 = binaryReadByte(file);

        if(b0 == 0x80)
        {
            if(b1 & 0x01)
            {
                b0 = binaryReadByte(file);
                b1 = binaryReadByte(file);
                flags = STOP;
            }
            else if(b1 == 0x02 || b1 == 0x04)
            {
                b0 = binaryReadByte(file);
                b1 = binaryReadByte(file);
                flags = TRIM;
            }
            else if(b1 == 0x10)
            {
                embPattern_addStitchRel(pattern, 0, 0, END, 1);
                break;
            }
        }
        dx = (char)b0;
        dy = (char)b1;
        embPattern_addStitchRel(pattern, dx / 10.0, dy / 10.0, flags, 1);
        stitchCount++;
    }
    fclose(file);
    return 1;
}

static void jefEncode(FILE* file, float x, float y, int stitchType)
{
    /* TODO: pointer safety */
    unsigned char dx = (char)(x * 10.0f);
    unsigned char dy = (char)(y * 10.0f);
    if(stitchType == TRIM)
    {
        binaryWriteByte(file, 0x80);
        binaryWriteByte(file, 0x02);
        binaryWriteByte(file, dx);
        binaryWriteByte(file, dy);
    }
    else if(stitchType == STOP)
    {
        binaryWriteByte(file, 0x80);
        binaryWriteByte(file, 0x01);
        binaryWriteByte(file, dx);
        binaryWriteByte(file, dy);
    }
    else if (stitchType == END)
    {
        binaryWriteByte(file, 0x80);
        binaryWriteByte(file, 0x10);
    }
    else
    {
        binaryWriteByte(file, dx);
        binaryWriteByte(file, dy);
    }
}

int writeJef(EmbPattern* pattern, const char* fileName)
{
    int colorlistSize, minColors, designWidth, designHeight, i;
    EmbRect boundingRect;
    FILE* file = 0;
    EmbTime time;
    EmbThreadList* threadPointer = 0;
    EmbStitch c;
    EmbStitchList* pointer = 0;
    double prevX, prevY;

    file = fopen(fileName, "wb");
    if(!file)
    {
        return 0;
    }
    embPattern_correctForMaxStitchLength(pattern, 127, 127);

    colorlistSize = embThreadList_count(pattern->threadList);
    minColors = max(colorlistSize, 6);
    binaryWriteInt(file, 0x74 + (minColors * 4));
    binaryWriteInt(file, 0x0A);

    embTime_initNow(&time);

    fprintf(file, "%04d%02d%02d%02d%02d%02d", (int)(time.year + 1900),
            (int)(time.month + 1), (int)(time.day), (int)(time.hour),
            (int)(time.minute), (int)(time.second));
    binaryWriteByte(file, 0x00);
    binaryWriteByte(file, 0x00);
    binaryWriteInt(file, embThreadList_count(pattern->threadList));
    binaryWriteInt(file, embStitchList_count(pattern->stitchList) + max(0, (6 - colorlistSize) * 2) + 1);

    boundingRect = embPattern_calcBoundingBox(pattern);

    designWidth = (int)(embRect_width(boundingRect) * 10.0);
    designHeight = (int)(embRect_width(boundingRect) * 10.0);

    binaryWriteInt(file, jefGetHoopSize(designWidth, designHeight));

    /* Distance from center of Hoop */
    binaryWriteInt(file, (int) (designWidth / 2));  /* left */
    binaryWriteInt(file, (int) (designHeight / 2)); /* top */
    binaryWriteInt(file, (int) (designWidth / 2));  /* right */
    binaryWriteInt(file, (int) (designHeight / 2)); /* bottom */

    /* Distance from default 110 x 110 Hoop */
    if(min(550 - designWidth / 2, 550 - designHeight / 2) >= 0)
    {
        binaryWriteInt(file, (int) max(-1, 550 - designWidth / 2));  /* left */
        binaryWriteInt(file, (int) max(-1, 550 - designHeight / 2)); /* top */
        binaryWriteInt(file, (int) max(-1, 550 - designWidth / 2));  /* right */
        binaryWriteInt(file, (int) max(-1, 550 - designHeight / 2)); /* bottom */
    }
    else
    {
        binaryWriteInt(file, -1);
        binaryWriteInt(file, -1);
        binaryWriteInt(file, -1);
        binaryWriteInt(file, -1);
    }

    /* Distance from default 50 x 50 Hoop */
    if(min(250 - designWidth / 2, 250 - designHeight / 2) >= 0)
    {
        binaryWriteInt(file, (int) max(-1, 250 - designWidth / 2));  /* left */
        binaryWriteInt(file, (int) max(-1, 250 - designHeight / 2)); /* top */
        binaryWriteInt(file, (int) max(-1, 250 - designWidth / 2));  /* right */
        binaryWriteInt(file, (int) max(-1, 250 - designHeight / 2)); /* bottom */
    }
    else
    {
        binaryWriteInt(file, -1);
        binaryWriteInt(file, -1);
        binaryWriteInt(file, -1);
        binaryWriteInt(file, -1);
    }

    /* Distance from default 140 x 200 Hoop */
    binaryWriteInt(file, (int) (700 - designWidth / 2));   /* left */
    binaryWriteInt(file, (int) (1000 - designHeight / 2)); /* top */
    binaryWriteInt(file, (int) (700 - designWidth / 2));   /* right */
    binaryWriteInt(file, (int) (1000 - designHeight / 2)); /* bottom */

    /* repeated Distance from default 140 x 200 Hoop */
    /* TODO: Actually should be distance to custom hoop */
    binaryWriteInt(file, (int) (630 - designWidth / 2));  /* left */
    binaryWriteInt(file, (int) (550 - designHeight / 2)); /* top */
    binaryWriteInt(file, (int) (630 - designWidth / 2));  /* right */
    binaryWriteInt(file, (int) (550 - designHeight / 2)); /* bottom */

    threadPointer = pattern->threadList;

    while(threadPointer)
    {
        binaryWriteInt(file, embThread_findNearestColorInArray(threadPointer->thread.color, (EmbThread*)jefThreads, 79));
        threadPointer = threadPointer->next;
    }
    for(i = 0; i < (minColors - colorlistSize); i++)
    {
        binaryWriteInt(file, 0x0D);
    }
    pointer = pattern->stitchList;

    prevX = pointer->stitch.xx;
    prevY = pointer->stitch.yy;
    jefEncode(file, (unsigned char) prevX, (unsigned char) prevY, pointer->stitch.flags);
    pointer = pointer->next;
    while(pointer)
    {
        c = pointer->stitch;
        jefEncode(file, (unsigned char) (c.xx - prevX), (unsigned char) (c.yy - prevY), c.flags);
        prevX = c.xx;
        prevY = c.yy;
        pointer = pointer->next;
    }
    fclose(file);
    return 1;
}

/* kate: bom off; indent-mode cstyle; indent-width 4; replace-trailing-space-save on; */
